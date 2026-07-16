
#include "circuit.h"
#include "neuralNetwork.hpp"
#include "verifier.hpp"
#include "models.hpp"
#include "global_var.hpp"
#include <iostream>

#include "range_prover.hpp"
#include "gkr_serialization.hpp"
#include "gkr_verifier.hpp"
#include "zkgpt_serialization.hpp"
#include "hyrax_rp.hpp"
#include "stats.hpp"
#include <stdexcept>
using namespace mcl::bn;
using namespace std;

namespace {
string stage_a_test_mode(int argc, char **argv) {
    const string prefix="--stage-a-test=";
    for (int i=1;i<argc;++i) {
        const string arg=argv[i];
        if (arg.rfind(prefix, 0)==0) return arg.substr(prefix.size());
    }
    return "";
}

string artifact_prefix(int argc,char **argv) {
    for (int i=1;i<argc;++i) {
        const string argument=argv[i];
        if (argument=="--artifact-prefix") {
            if (i+1>=argc)
                throw invalid_argument("--artifact-prefix requires a path");
            return argv[i+1];
        }
    }
    return "";
}

void write_artifact(const string &path,const vector<uint8_t> &bytes) {
    ofstream output(path,ios::binary);
    if (!output) throw runtime_error("cannot create artifact file: "+path);
    output.write(reinterpret_cast<const char *>(bytes.data()),bytes.size());
    if (!output) throw runtime_error("cannot write artifact file: "+path);
}

const RangeConstraint &find_constraint(const WitnessRegistry &registry,
                                       WitnessKind kind,
                                       const string &name_fragment) {
    const RangeConstraint *match=nullptr;
    for (const auto &constraint : registry.constraints()) {
        if (constraint.kind==kind &&
            constraint.name.find(name_fragment)!=string::npos)
            match=&constraint;
    }
    if (match) return *match;
    throw runtime_error("missing test constraint: "+name_fragment);
}
}



int main(int argc, char **argv)
{
    initPairing(mcl::BN254);
    const string test_mode=stage_a_test_mode(argc, argv);
    const string artifact_path_prefix=artifact_prefix(argc,argv);
    if (test_mode=="range-kernel") {
        vector<F> val0(1024);
        for (size_t i=0;i<val0.size();++i)
            val0[i]=Fr(static_cast<int>(i)-512);
        WitnessRegistry registry;
        registry.setShape({32, 12, 12, 64, 768, 3072});
        registry.addRange(WitnessKind::ROUNDING, "kernel_signed64", 0,
                          val0.size(), 64, true);
        range_prover kernel(12, 12, 64, 768, 3072, 32, 4, 1);
        kernel.init();
        kernel.buildFromWitness(val0, registry);
        std::vector<G1> val0_generators(32);
        const G1 val0_u=gen_gi(val0_generators.data(),
                               val0_generators.size());
        G1 *val0_commitments=prover_commit(
            val0.data(), val0_generators.data(), 10, 4);
        const auto statement=kernel.makePublicStatement(
            10, val0_commitments, 32, val0_generators.data(),
            val0_generators.size(), val0_u);
        delete[] val0_commitments;
        const auto proof=kernel.proveStageB(statement);
        range_verifier range_verifier;
        string error;
        if (!range_verifier.verify(statement, proof, &error))
            throw runtime_error("Stage B kernel verification failed: "+error);
        cout << "Stage B Range Proof kernel test passed" << endl;
        return 0;
    }

    LLM nn(12, 12, 64, 768, 3072);
    if (test_mode=="public-circuit") {
        const auto circuit=nn.buildPublicCircuit();
        size_t unary=0,binary=0;
        for (const auto &layer : circuit.circuit) {
            unary+=layer.uni_gates.size();
            binary+=layer.bin_gates.size();
        }
        cout << "Public circuit built without private witness: layers="
             << circuit.size
             << ", unary_gates=" << unary
             << ", binary_gates=" << binary << endl;
        cout << "Circuit fingerprint: "
             << circuitFingerprintHex(fingerprintCircuit(circuit)) << endl;
        return 0;
    }
    prover p;
    nn.create(p, 0);

    if (test_mode=="circuit-fingerprint") {
        cout << "Circuit fingerprint: "
             << circuitFingerprintHex(fingerprintCircuit(p.C)) << endl;
        return 0;
    }

    if (test_mode=="gkr-only") {
        verifier v(&p, p.C);
        v.prove(32);
        const auto gkr_bytes=serializeGKRProof(v.gkrProof());
        GKRProof decoded_gkr;
        string gkr_error;
        if (!deserializeGKRProof(gkr_bytes, decoded_gkr, &gkr_error) ||
            serializeGKRProof(decoded_gkr)!=gkr_bytes)
            throw runtime_error("GKR proof serialization failed: "+gkr_error);
        const size_t commitment_count=static_cast<size_t>(1)<<(p.cc.l/2);
        const size_t generator_count=static_cast<size_t>(1)
            <<(p.cc.l-p.cc.l/2);
        GKRPublicStatement gkr_statement;
        gkr_statement.model_shape=nn.getWitnessRegistry().shape();
        gkr_statement.val0_log_size=p.cc.l;
        gkr_statement.val0_commitment.assign(
            p.cc.comm,p.cc.comm+commitment_count);
        gkr_statement.val0_generator_domain={
            "zkGPT/main",static_cast<uint32_t>(generator_count)};
        gkr_statement.circuit_fingerprint=fingerprintCircuit(p.C);
        gkr_statement.output_evaluation=decoded_gkr.output_evaluation;
        if (!verifyGKR(p.C,gkr_statement,decoded_gkr,&gkr_error))
            throw runtime_error("Independent GKR verification failed: "+
                                gkr_error);
        cout << "Independent witness-free GKR verification passed" << endl;

        GKRProof mutated_gkr=decoded_gkr;
        bool mutated_round=false;
        for (auto &layer : mutated_gkr.layers) {
            if (!layer.phase1_rounds.empty()) {
                layer.phase1_rounds[0].a+=Fr(1);
                mutated_round=true;
                break;
            }
            if (!layer.matrix_rounds.empty()) {
                layer.matrix_rounds[0].a+=Fr(1);
                mutated_round=true;
                break;
            }
        }
        if (!mutated_round)
            throw runtime_error("GKR mutation test found no proof round");
        const auto mutated_bytes=serializeGKRProof(mutated_gkr);
        GKRProof decoded_mutation;
        if (!deserializeGKRProof(mutated_bytes,decoded_mutation,&gkr_error))
            throw runtime_error("mutated GKR proof did not round-trip: "+
                                gkr_error);
        if (verifyGKR(p.C,gkr_statement,decoded_mutation,&gkr_error))
            throw runtime_error("modified and reserialized GKR proof was accepted");
        cout << "Modified/reserialized GKR proof rejected: " << gkr_error
             << endl;

        auto wrong_statement=gkr_statement;
        const auto &main_generators=
            getGeneratorSet(gkr_statement.val0_generator_domain);
        wrong_statement.val0_commitment[0]+=
            main_generators.generators[0];
        if (verifyGKR(p.C,wrong_statement,decoded_gkr,&gkr_error))
            throw runtime_error("wrong GKR val0 commitment was accepted");
        cout << "Wrong GKR val0 commitment rejected: " << gkr_error << endl;
        cout << "Serialized GKR proof bytes: " << gkr_bytes.size() << endl;
        stats::print_stats();
        return 0;
    }

    if (test_mode=="seq-mismatch") {
        try {
            range_prover mismatched(nn.getLayerCount(), nn.getHeadCount(),
                nn.getHeadDimension(), nn.getHiddenDimension(),
                nn.getMlpDimension(), nn.getSequenceLength()-2, 32, 1);
            mismatched.buildFromWitness(p.val.at(0), nn.getWitnessRegistry());
        } catch (const invalid_argument &error) {
            cout << "Stage A negative test passed: " << error.what() << endl;
            return 0;
        }
        throw runtime_error("mismatched sequence length was accepted");
    }

    if (test_mode=="mutation-suite") {
        struct MutationCase {
            WitnessKind kind;
            const char *fragment;
            const char *name;
        };
        const MutationCase cases[] = {
            {WitnessKind::GELU, "_delta3", "GELU delta3"},
            {WitnessKind::SOFTMAX, "_sumE", "Softmax sumE"},
            {WitnessKind::LAYER_NORM, "_delta2", "LayerNorm delta2"},
            {WitnessKind::ROUNDING, "_term1", "rounding remainder"}
        };
        for (const auto &test : cases) {
            const auto &constraint=find_constraint(
                nn.getWitnessRegistry(), test.kind, test.fragment);
            F original=p.val.at(0).at(constraint.val0_offset);
            p.val.at(0).at(constraint.val0_offset)+=Fr(1);
            bool rejected=false;
            try {
                nn.validateCurrentWitness(p);
            } catch (const logic_error &error) {
                rejected=true;
                cout << "Stage A mutation rejected (" << test.name
                     << "): " << error.what() << endl;
            }
            p.val.at(0).at(constraint.val0_offset)=original;
            if (!rejected)
                throw runtime_error(string("Stage A mutation was accepted: ")+
                                    test.name);
        }
        cout << "Stage A mutation suite passed" << endl;
        return 0;
    }

    size_t mutated_offset=static_cast<size_t>(-1);
    if (test_mode=="negative-unsigned") {
        const auto &constraint=find_constraint(
            nn.getWitnessRegistry(), WitnessKind::GELU, "_abs");
        p.val.at(0).at(constraint.val0_offset)=Fr(-1);
    } else if (test_mode=="gelu") {
        const auto &constraint=find_constraint(
            nn.getWitnessRegistry(), WitnessKind::GELU, "_delta3");
        mutated_offset=constraint.val0_offset;
        p.val.at(0).at(mutated_offset)+=Fr(1);
    } else if (test_mode=="softmax-sumE") {
        const auto &constraint=find_constraint(
            nn.getWitnessRegistry(), WitnessKind::SOFTMAX, "_sumE");
        mutated_offset=constraint.val0_offset;
        p.val.at(0).at(mutated_offset)+=Fr(1);
    } else if (test_mode=="layernorm-delta") {
        const auto &constraint=find_constraint(
            nn.getWitnessRegistry(), WitnessKind::LAYER_NORM, "_delta2");
        mutated_offset=constraint.val0_offset;
        p.val.at(0).at(mutated_offset)+=Fr(1);
    } else if (test_mode=="rounding-remainder") {
        const auto &constraint=find_constraint(
            nn.getWitnessRegistry(), WitnessKind::ROUNDING, "_term1");
        mutated_offset=constraint.val0_offset;
        p.val.at(0).at(mutated_offset)+=Fr(1);
    }

    const bool gkr_mutation_test=test_mode=="gelu" ||
        test_mode=="softmax-sumE" || test_mode=="layernorm-delta" ||
        test_mode=="rounding-remainder";
    if (gkr_mutation_test) {
        size_t references=0;
        u32 referenced_layer=0;
        u32 referenced_gate=0;
        for (u32 layer_id=1; layer_id<p.C.size; ++layer_id) {
            const auto &layer=p.C.circuit.at(layer_id);
            for (const auto &gate : layer.uni_gates) {
                if (gate.lu==0 &&
                    layer.ori_id_u.at(gate.u)==mutated_offset) {
                    ++references;
                    referenced_layer=layer_id;
                    referenced_gate=gate.g;
                }
            }
            for (const auto &gate : layer.bin_gates) {
                if (gate.getLayerIdU(layer_id)==0 &&
                    layer.ori_id_u.at(gate.u)==mutated_offset)
                    ++references;
                if (gate.getLayerIdV(layer_id)==0 &&
                    layer.ori_id_v.at(gate.v)==mutated_offset)
                    ++references;
            }
        }
        cout << "Stage A mutation " << test_mode << " changed val[0]["
             << mutated_offset << "]; mapped gate references=" << references
             << endl;
        if (references==0)
            throw runtime_error("mutated witness value is not GKR-constrained");
        const auto &layer=p.C.circuit.at(referenced_layer);
        F recomputed;
        recomputed.clear();
        for (const auto &gate : layer.uni_gates) {
            if (gate.g!=referenced_gate) continue;
            const F &source=gate.lu==0
                ? p.val.at(0).at(layer.ori_id_u.at(gate.u))
                : p.val.at(gate.lu).at(gate.u);
            recomputed+=source*gate.sc;
        }
        for (const auto &gate : layer.bin_gates) {
            if (gate.g!=referenced_gate) continue;
            const u32 lu=gate.getLayerIdU(referenced_layer);
            const u32 lv=gate.getLayerIdV(referenced_layer);
            const F &u=lu==0 ? p.val.at(0).at(layer.ori_id_u.at(gate.u))
                             : p.val.at(lu).at(gate.u);
            const F &v=lv==0 ? p.val.at(0).at(layer.ori_id_v.at(gate.v))
                             : p.val.at(lv).at(gate.v);
            recomputed+=u*v*gate.sc;
        }
        cout << "Mutated mapped equation at layer " << referenced_layer
             << ", gate " << referenced_gate << ": recomputed=" << recomputed
             << ", stored=" << p.val.at(referenced_layer).at(referenced_gate)
             << endl;
        try {
            nn.validateCurrentWitness(p);
        } catch (const logic_error &error) {
            cout << "Stage A consistency negative test passed: "
                 << error.what() << endl;
            return 0;
        }
        throw runtime_error("Stage A mutation was accepted: "+test_mode);
    }

    printf("Start range proving from the GKR val[0] witness...\n");
    range_prover range_prover(nn.getLayerCount(), nn.getHeadCount(),
        nn.getHeadDimension(), nn.getHiddenDimension(), nn.getMlpDimension(),
        nn.getSequenceLength(), 32, 1);
    range_prover.init();
    range_prover.buildFromWitness(p.val.at(0), nn.getWitnessRegistry());
    if (test_mode=="range-copy") {
        range_prover.tamperBuiltValueForTest();
        try {
            range_prover.verifyWitnessConsistency();
        } catch (const logic_error &error) {
            cout << "Stage A negative test passed: " << error.what() << endl;
            return 0;
        }
        throw runtime_error("tampered Range Proof copy was accepted");
    }
    const std::size_t val0_commitment_count=static_cast<std::size_t>(1)
        << (p.cc.l/2);
    const std::size_t val0_generator_count=static_cast<std::size_t>(1)
        << (p.cc.l-p.cc.l/2);
    const auto range_statement=range_prover.makePublicStatement(
        p.cc.l, p.cc.comm, val0_commitment_count, p.cc.g,
        val0_generator_count, p.cc.G);
    const auto range_proof=range_prover.proveStageB(range_statement);
    range_verifier range_verifier;
    string range_error;
    if (!range_verifier.verify(
            range_statement, range_proof, &range_error))
        throw runtime_error("Range verification failed: "+
                            range_error);
    const double range_prover_time=range_proof.totalProverTime();
    cout << "Range chunk reconstruction proof verified; prover time="
         << range_proof.reconstruction_prover_time << "s" << endl;

    cout << "Range/GKR commitment binding verified." << endl;
    verifier v(&p, p.C);
    v.range_prove(range_prover_time);
    v.prove(32); // prove with 32 threads

    ZkGPTPublicStatement statement;
    statement.model_shape=nn.getWitnessRegistry().shape();
    statement.circuit_fingerprint=fingerprintCircuit(p.C);
    statement.output_claim.mle_evaluation=v.gkrProof().output_evaluation;
    statement.range_statement=range_statement;
    ZkGPTProof proof;
    proof.gkr_proof=v.gkrProof();
    proof.range_proof=range_proof;
    bindZkGPTProof(statement,proof);
    const auto statement_bytes=serializeZkGPTPublicStatement(statement);
    const auto proof_bytes=serializeZkGPTProof(proof);
    ZkGPTPublicStatement decoded_statement;
    ZkGPTProof decoded_proof;
    string artifact_error;
    if (!deserializeZkGPTPublicStatement(
            statement_bytes,decoded_statement,&artifact_error) ||
        serializeZkGPTPublicStatement(decoded_statement)!=statement_bytes)
        throw runtime_error("top-level statement round-trip failed: "+
                            artifact_error);
    if (!deserializeZkGPTProof(proof_bytes,decoded_proof,&artifact_error) ||
        serializeZkGPTProof(decoded_proof)!=proof_bytes)
        throw runtime_error("top-level proof round-trip failed: "+
                            artifact_error);
    cout << "Serialized zkGPT statement bytes: " << statement_bytes.size()
         << endl;
    cout << "Serialized zkGPT proof bytes: " << proof_bytes.size() << endl;
    if (!artifact_path_prefix.empty()) {
        write_artifact(artifact_path_prefix+".statement.bin",statement_bytes);
        write_artifact(artifact_path_prefix+".proof.bin",proof_bytes);
        cout << "Wrote zkGPT artifact prefix: " << artifact_path_prefix << endl;
    }
    stats::print_stats();

}

