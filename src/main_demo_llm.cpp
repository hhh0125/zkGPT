
#include "model_binding.hpp"
#include "zkgpt_artifact_io.hpp"
#include "circuit.h"
#include "neuralNetwork.hpp"
#include "gkr_proof_builder.hpp"
#include "models.hpp"
#include "global_var.hpp"
#include <iostream>

#include "range_prover.hpp"
#include "gkr_serialization.hpp"
#include "gkr_verifier.hpp"
#include "zkgpt_serialization.hpp"
#include "hyrax_rp.hpp"
#include "stats.hpp"
#include "utils.hpp"
#include "thread_joiner.hpp"
#include "memory_diagnostics.hpp"
#include <limits>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
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

bool has_flag(int argc,char **argv,const string &flag) {
    for (int i=1;i<argc;++i)
        if (argv[i]==flag) return true;
    return false;
}

template <typename Bytes>
string trace_hex(const Bytes &bytes) {
    ostringstream output;
    output << hex << setfill('0');
    for (const auto byte : bytes)
        output << setw(2) << static_cast<unsigned>(byte);
    return output.str();
}

int positive_int_option(int argc,char **argv,const string &option,
                        int default_value) {
    for (int i=1;i<argc;++i) {
        if (argv[i]!=option) continue;
        if (i+1>=argc)
            throw invalid_argument(option+" requires a positive integer");
        const string value=argv[i+1];
        std::size_t consumed=0;
        const long parsed=std::stol(value,&consumed);
        if (consumed!=value.size() || parsed<=0 ||
            parsed>std::numeric_limits<int>::max())
            throw invalid_argument(option+" requires a positive integer");
        return static_cast<int>(parsed);
    }
    return default_value;
}

void write_artifact(const string &path,const vector<uint8_t> &bytes) {
    const int descriptor=::open(
        path.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);
    if (descriptor<0)
        throw runtime_error("cannot exclusively create artifact file "+path+
                            ": "+std::strerror(errno));
    bool complete=false;
    try {
        std::size_t offset=0;
        while (offset<bytes.size()) {
            const ssize_t written=::write(
                descriptor,bytes.data()+offset,bytes.size()-offset);
            if (written<0 && errno==EINTR) continue;
            if (written<=0)
                throw runtime_error("cannot write artifact file "+path+
                                    ": "+std::strerror(errno));
            offset+=static_cast<std::size_t>(written);
        }
        if (::fsync(descriptor)!=0)
            throw runtime_error("cannot sync artifact file "+path+
                                ": "+std::strerror(errno));
        if (::close(descriptor)!=0)
            throw runtime_error("cannot close artifact file "+path+
                                ": "+std::strerror(errno));
        complete=true;
    } catch (...) {
        if (!complete) {
            ::close(descriptor);
            ::unlink(path.c_str());
        }
        throw;
    }
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
    const int range_threads=positive_int_option(
        argc,argv,"--range-threads",
        static_cast<int>(zkgpt::workerThreadCount()));
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

    ZkGPTPublicStatement statement;
    ZkGPTProof proof;
    std::vector<uint8_t> strict_bytes;
    std::string trusted_anchor;
    for(int i=1;i+1<argc;i++) if(std::string(argv[i])=="--trusted-model") trusted_anchor=argv[i+1];
    { // Every private owner and borrower ends before serialization/verification.
    LLM nn(positive_int_option(argc,argv,"--blocks",12), 12, 64, 768, 3072);
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
        const bool trace_enabled=has_flag(argc,argv,"--gkr-challenge-trace");
        ChallengeTraceRecorder prover_trace;
        ChallengeTraceRecorder verifier_trace;
        const size_t generator_count=static_cast<size_t>(1)
            <<(p.cc.l-p.cc.l/2);
        const GeneratorDomain generator_domain{
            "zkGPT/main",static_cast<uint32_t>(generator_count)};
        GKRProofBuilder gkr_builder(
            &p,p.C,nn.getWitnessRegistry().shape(),generator_domain,
            trace_enabled ? &prover_trace : nullptr);
        gkr_builder.build();
        const auto gkr_bytes=serializeGKRProof(gkr_builder.proof());
        GKRProof decoded_gkr;
        string gkr_error;
        if (!deserializeGKRProof(gkr_bytes, decoded_gkr, &gkr_error) ||
            serializeGKRProof(decoded_gkr)!=gkr_bytes)
            throw runtime_error("GKR proof serialization failed: "+gkr_error);
        GKRPublicStatement gkr_statement=gkr_builder.publicStatement();
        timer gkr_verifier_timer;
        gkr_verifier_timer.start();
        const bool gkr_valid=verifyGKR(
            p.C,gkr_statement,decoded_gkr,&gkr_error,
            trace_enabled ? &verifier_trace : nullptr);
        gkr_verifier_timer.stop();
        if (!gkr_valid)
            throw runtime_error("Independent GKR verification failed: "+
                                gkr_error);
        cout << "Independent witness-free GKR verification passed" << endl;
        cout << "Independent GKR verification time: "
             << gkr_verifier_timer.elapse_sec() << "s" << endl;
        if (trace_enabled) {
            string trace_error;
            if (!compareChallengeTraces(
                    prover_trace.entries,verifier_trace.entries,&trace_error))
                throw runtime_error(
                    "full-scale GKR challenge trace mismatch: "+trace_error);
            size_t output_points=0,zero_scale=0,phase1=0,phase2=0;
            size_t matrix_u=0,matrix_v=0;
            size_t layer_mix=0,lasso_batch=0,fconn_lasso_batch=0;
            size_t lasso_sumcheck=0,ipa=0,final_challenges=0;
            for (const auto &entry : prover_trace.entries) {
                if (entry.label=="gkr.output.point") ++output_points;
                else if (entry.label=="gkr.relu.zero_scale") ++zero_scale;
                else if (entry.label=="gkr.phase1.challenge") ++phase1;
                else if (entry.label=="gkr.phase2.challenge") ++phase2;
                else if (entry.label.find("gkr.matrix.u.")==0) ++matrix_u;
                else if (entry.label.find("gkr.matrix.v.")==0) ++matrix_v;
                else if (entry.label=="gkr.layer.alpha" ||
                         entry.label=="gkr.layer.beta") ++layer_mix;
                else if (entry.label=="lasso.sigma_u" ||
                         entry.label=="lasso.sigma_v") ++lasso_batch;
                else if (entry.label=="lasso.fconn_v.sigma_u" ||
                         entry.label=="lasso.fconn_v.sigma_v")
                    ++fconn_lasso_batch;
                else if (entry.label=="lasso.sumcheck.challenge")
                    ++lasso_sumcheck;
                else if (entry.label=="ipa-round") ++ipa;
                else if (entry.label=="gkr.proof.final") ++final_challenges;
            }
            cout << "Full-scale GKR challenge trace matched: entries="
                 << prover_trace.entries.size() << endl;
            cout << "  output=" << output_points
                 << ", zero_scale=" << zero_scale
                 << ", phase1=" << phase1 << ", phase2=" << phase2
                 << ", matrix_u=" << matrix_u
                 << ", matrix_v=" << matrix_v
                 << ", alpha_beta=" << layer_mix << endl;
            cout << "  lasso_batch=" << lasso_batch
                 << ", fconn_v_lasso_batch=" << fconn_lasso_batch
                 << ", lasso_sumcheck=" << lasso_sumcheck
                 << ", ipa=" << ipa << ", final=" << final_challenges
                 << endl;
            if (prover_trace.entries.empty())
                throw runtime_error("full-scale GKR challenge trace is empty");
            cout << "  final transcript digest="
                 << trace_hex(prover_trace.entries.back().post_state_digest)
                 << endl;
        }

        auto v1_gkr_statement=gkr_statement;
        v1_gkr_statement.protocol_version=1;
        if (verifyGKR(p.C,v1_gkr_statement,decoded_gkr,&gkr_error) ||
            gkr_error.find("unsupported protocol version")==string::npos)
            throw runtime_error("v1 GKR statement was not explicitly rejected");
        cout << "v1 GKR statement explicitly rejected: " << gkr_error << endl;

        auto wrong_shape=gkr_statement;
        ++wrong_shape.model_shape.mlp_dimension;
        if (verifyGKR(p.C,wrong_shape,decoded_gkr,&gkr_error))
            throw runtime_error("modified GKR ModelShape was accepted");
        cout << "Modified GKR ModelShape rejected: " << gkr_error << endl;

        auto wrong_generator_domain=gkr_statement;
        wrong_generator_domain.val0_generator_domain.domain="zkGPT/range";
        if (verifyGKR(
                p.C,wrong_generator_domain,decoded_gkr,&gkr_error) ||
            gkr_error.find("invalid val0 generator descriptor")==string::npos)
            throw runtime_error("modified GKR generator domain was accepted");
        cout << "Modified GKR generator domain rejected: "
             << gkr_error << endl;

        auto wrong_generator_count=gkr_statement;
        ++wrong_generator_count.val0_generator_domain.count;
        if (verifyGKR(
                p.C,wrong_generator_count,decoded_gkr,&gkr_error) ||
            gkr_error.find("invalid val0 generator descriptor")==string::npos)
            throw runtime_error("modified GKR generator count was accepted");
        cout << "Modified GKR generator count rejected: "
             << gkr_error << endl;

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
        nn.getSequenceLength(), range_threads, 32);
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
    const double range_prover_time=range_proof.totalProverTime();
    zkgpt::memory_diagnostics::logProcess("after_range_prover");
    cout << "Range proof generation time: reconstruction/opening="
         << range_proof.reconstruction_prover_time
         << "s + LogUp membership=" << range_proof.membership_prover_time
         << "s = total=" << range_prover_time << "s" << endl;

    cout << "Range/GKR commitment binding verified." << endl;
    GKRProofBuilder gkr_builder(
        &p,p.C,nn.getWitnessRegistry().shape(),
        range_statement.val0_generator_domain);
    gkr_builder.setRangeProverTime(range_prover_time);
    gkr_builder.build();
    zkgpt::memory_diagnostics::logProcess("lifecycle_gkr_proof_complete");
    statement.model_shape=gkr_builder.publicStatement().model_shape;
    statement.circuit_fingerprint=
        gkr_builder.publicStatement().circuit_fingerprint;
    statement.output_claim.mle_evaluation=
        gkr_builder.proof().output_evaluation;
    statement.range_statement=range_statement;
    proof.gkr_proof=gkr_builder.proof();
    proof.range_proof=range_proof;
    bindZkGPTProof(statement,proof);
    if (!trusted_anchor.empty()) {
        const char *embedding=std::getenv("ZKGPT_PUBLIC_EMBEDDING");
        if(!embedding) throw runtime_error("strict v4 requires ZKGPT_PUBLIC_EMBEDDING");
        auto anchor=decodeModelAnchor(readZkGPTArtifactFile(trusted_anchor,16ULL<<20,"trusted model"));
        auto x=readPublicEmbedding(embedding);
        timer identity_timer; identity_timer.start();
        auto bound=bindStrictArtifactV4(anchor,statement,proof,p.val[0],x,range_threads);
        identity_timer.stop();
        cout << "Model/input binding prove time: " << identity_timer.elapse_sec() << "s; mixed row openings=" << bound.equality_openings.size() << endl;
        std::string error;
        timer binding_verify_timer; binding_verify_timer.start();
        if(!verifyModelInputBinding(anchor,bound,x,&error)) throw runtime_error("strict binding: "+error);
        binding_verify_timer.stop();
        cout << "Model/input binding verification time: " << binding_verify_timer.elapse_sec() << "s" << endl;
        strict_bytes=encodeBoundArtifact(bound);
    }
    p.logPrivateMemory("lifecycle_before_private_destruction");
    nn.logPrivateMemory("lifecycle_before_private_destruction");
    range_prover.logPrivateMemory("lifecycle_before_private_destruction");
    } // GKR/Range borrowers, prover and network owners are now destroyed.
    zkgpt::memory_diagnostics::logProcess("lifecycle_after_private_destruction");
    zkgpt::memory_diagnostics::logScalar(
        "lifecycle_after_private_destruction","private_scope_alive",0);
    zkgpt::memory_diagnostics::logProcess("before_proof_serialization");
    const auto statement_bytes=serializeZkGPTPublicStatement(statement);
    const auto proof_bytes=serializeZkGPTProof(proof);
    zkgpt::memory_diagnostics::logProcess("after_proof_serialization");
    zkgpt::memory_diagnostics::logVector(
        "after_proof_serialization","statement_bytes",statement_bytes);
    zkgpt::memory_diagnostics::logVector(
        "after_proof_serialization","proof_bytes",proof_bytes);
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
    timer independent_verifier_timer;
    independent_verifier_timer.start();
    zkgpt::memory_diagnostics::logProcess("before_inprocess_artifact_verify");
    const bool artifact_valid=verifyZkGPT(
        decoded_statement,decoded_proof,&artifact_error);
    zkgpt::memory_diagnostics::logProcess("after_inprocess_artifact_verify");
    independent_verifier_timer.stop();
    if (!artifact_valid)
        throw runtime_error("independent zkGPT verification failed: "+
                            artifact_error);
    cout << "Independent zkGPT sequential verification passed; total verifier time="
         << independent_verifier_timer.elapse_sec() << "s" << endl;
    auto v1_statement=decoded_statement;
    v1_statement.protocol_version=1;
    if (verifyZkGPT(v1_statement,decoded_proof,&artifact_error) ||
        artifact_error.find("unsupported zkGPT protocol version")==string::npos)
        throw runtime_error("v1 zkGPT statement was not explicitly rejected");
    cout << "v1 zkGPT statement explicitly rejected: "
         << artifact_error << endl;
    cout << "Serialized zkGPT statement size: "
         << byte2KB(statement_bytes.size()) << " KB" << endl;
    cout << "Serialized zkGPT proof size: "
         << byte2KB(proof_bytes.size()) << " KB" << endl;
    if (!artifact_path_prefix.empty()) {
        if(!strict_bytes.empty()) write_artifact(artifact_path_prefix+".strict-v4.bin",strict_bytes);
        write_artifact(artifact_path_prefix+".statement.bin",statement_bytes);
        write_artifact(artifact_path_prefix+".proof.bin",proof_bytes);
        cout << "Wrote zkGPT artifact prefix: " << artifact_path_prefix << endl;
    }
    stats::print_stats();

}
