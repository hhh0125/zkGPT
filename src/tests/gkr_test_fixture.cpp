#include "tests/gkr_test_fixture.hpp"

#include "hyrax_rp.hpp"
#include "utils.hpp"

#include <stdexcept>
#include <string>

namespace gkr_test {

SmallFconnFixture::SmallFconnFixture(bool input_ports) {
    prover_.mat_val=nullptr;
    prover_.cc.comm=nullptr;
    prover_.cc.w=nullptr;
    try {
        constexpr unsigned dimension=4;
        prover_.C.init(0,1+2*kFconnCount);
        prover_.val.resize(prover_.C.size);
        prover_.val[0].assign(32,Fr(0));
        prover_.val[0][0]=Fr(3);
        for (unsigned output=0;output<dimension;++output)
            prover_.val[0][dimension+output*dimension]=Fr(1);
        initLayer(
            prover_.C.circuit[0],prover_.val[0].size(),layerType::INPUT);
        prover_.mat_val=new int *[kFconnCount]{};
        for (unsigned fc=0;fc<kFconnCount;++fc) {
            const unsigned matrix_layer=1+2*fc;
            const unsigned parent_layer=matrix_layer+1;
            auto &matrix=prover_.C.circuit[matrix_layer];
            initLayer(matrix,dimension,layerType::FCONN);
            matrix.need_phase2=true;
            for (unsigned i=0;i<dimension;++i)
                matrix.ori_id_u.push_back(i);
            for (unsigned i=0;i<dimension*dimension;++i)
                matrix.ori_id_v.push_back(dimension+i);
            matrix.size_u[0]=matrix.ori_id_u.size();
            matrix.size_v[0]=matrix.ori_id_v.size();
            prover_.val[matrix_layer].assign(dimension,Fr(3));

            auto &parent=prover_.C.circuit[parent_layer];
            initLayer(parent,dimension,layerType::Sqr);
            parent.need_phase2=true;
            for (unsigned i=0;i<dimension;++i)
                parent.bin_gates.emplace_back(i,i,i,1,1);
            if(input_ports) {
                parent.uni_gates.emplace_back(0,1,0,1);
                parent.uni_gates.emplace_back(1,2,0,1);
            }
            prover_.val[parent_layer].assign(dimension,Fr(9));

            prover_.fc_input_row[fc]=1;
            prover_.fc_input_col[fc]=dimension;
            prover_.fc_row[fc]=dimension;
            prover_.fc_col[fc]=dimension;
            prover_.fc_input_id[fc]=0;
            prover_.mat_val[fc]=new int[dimension*dimension]{};
            for (unsigned output=0;output<dimension;++output)
                prover_.mat_val[fc][output*dimension]=1;
        }
        prover_.C.initSubset();

        const int input_bits=prover_.C.circuit[0].bit_length;
        const std::size_t generator_count=std::size_t{1}<<
            (input_bits-input_bits/2);
        prover_.gens.resize(generator_count);
        const G1 u=gen_gi(prover_.gens.data(),generator_count);
        prover_.gens.push_back(u);
        prover_.commitInput(prover_.gens,1);
        const GeneratorDomain domain{
            "zkGPT/main",static_cast<std::uint32_t>(generator_count)};
        const WitnessShape shape{1,1,1,4,4,4};

        GKRProofBuilder unobserved(
            &prover_,prover_.C,shape,domain);
        unobserved.build();
        const auto unobserved_bytes=serializeGKRProof(unobserved.proof());

        GKRProofBuilder observed(
            &prover_,prover_.C,shape,domain,&prover_trace_);
        observed.build();
        proof_bytes_=serializeGKRProof(observed.proof());
        if (proof_bytes_!=unobserved_bytes)
            throw std::runtime_error(
                "observer changed canonical small GKR proof bytes");
        if (observed.transcriptStateDigest()!=
                unobserved.transcriptStateDigest() ||
            observed.transcriptChallengeCounter()!=
                unobserved.transcriptChallengeCounter())
            throw std::runtime_error(
                "observer changed final small GKR Transcript state");
        statement_=observed.publicStatement();

        std::string error;
        if (!deserializeGKRProof(proof_bytes_,proof_,&error))
            throw std::runtime_error(
                "small GKR proof deserialization failed: "+error);
        if (serializeGKRProof(proof_)!=proof_bytes_)
            throw std::runtime_error(
                "small GKR proof canonical round-trip changed bytes");
        if (!verifyGKR(
                prover_.C,statement_,proof_,&error,&verifier_trace_))
            throw std::runtime_error(
                "small GKR baseline verification failed: "+error);
        if (!compareChallengeTraces(
                prover_trace_.entries,verifier_trace_.entries,&error))
            throw std::runtime_error(
                "small GKR baseline trace mismatch: "+error);

        matrix_index_=proof_.layers.size();
        parent_index_=proof_.layers.size();
        for (std::size_t i=0;i<proof_.layers.size();++i) {
            if (proof_.layers[i].layer_type==
                static_cast<std::uint32_t>(layerType::FCONN)) {
                matrix_index_=i;
                if (i>0) parent_index_=i-1;
                break;
            }
        }
        if (matrix_index_>=proof_.layers.size() ||
            parent_index_>=proof_.layers.size())
            throw std::runtime_error(
                "small GKR fixture has no FCONN/parent proof pair");
    } catch (...) {
        cleanup();
        throw;
    }
}

SmallFconnFixture::~SmallFconnFixture() {
    cleanup();
}

void SmallFconnFixture::cleanup() noexcept {
    if (prover_.mat_val) {
        for (unsigned fc=0;fc<kFconnCount;++fc)
            delete[] prover_.mat_val[fc];
        delete[] prover_.mat_val;
        prover_.mat_val=nullptr;
    }
}

}  // namespace gkr_test
