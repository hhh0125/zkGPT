
#include "circuit.h"
#include "neuralNetwork.hpp"
#include "verifier.hpp"
#include "models.hpp"
#include "global_var.hpp"
#include <iostream>

#include "range_prover.hpp"
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

    prover p;
    LLM nn(12, 12, 64, 768, 3072);
    nn.create(p, 0);

    if (test_mode=="gkr-only") {
        verifier v(&p, p.C);
        v.prove(32);
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
    stats::print_stats();

}

