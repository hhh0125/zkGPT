#include "tests/zkgpt_artifact_test_fixture.hpp"

#include "gkr_proof_builder.hpp"
#include "range_prover.hpp"
#include "thread_joiner.hpp"

#include <fstream>
#include <stdexcept>

namespace gkr_test {

SmallZkGPTArtifactFixture::SmallZkGPTArtifactFixture()
{
    { // Form owning public values, then retire every private owner/borrower.
        LLM network_(kDepth,12,64,768,3072);
        prover prover_;
        network_.create(prover_,false);

        range_prover range(
            network_.getLayerCount(),network_.getHeadCount(),
            network_.getHeadDimension(),network_.getHiddenDimension(),
            network_.getMlpDimension(),network_.getSequenceLength(),
            static_cast<int>(zkgpt::workerThreadCount()),32);
        range.init();
        range.buildFromWitness(
            prover_.val.at(0),network_.getWitnessRegistry());
        const std::size_t commitment_count=std::size_t{1}
            << (prover_.cc.l/2);
        const std::size_t generator_count=std::size_t{1}
            << (prover_.cc.l-prover_.cc.l/2);
        statement_.range_statement=range.makePublicStatement(
            prover_.cc.l,prover_.cc.comm,commitment_count,prover_.cc.g,
            generator_count,prover_.cc.G);
        proof_.range_proof=range.proveStageB(statement_.range_statement);

        GKRProofBuilder gkr(
            &prover_,prover_.C,network_.getWitnessRegistry().shape(),
            statement_.range_statement.val0_generator_domain);
        gkr.setRangeProverTime(proof_.range_proof.totalProverTime());
        gkr.build();
        proof_.gkr_proof=gkr.proof();
        statement_.model_shape=gkr.publicStatement().model_shape;
        statement_.circuit_fingerprint=
            gkr.publicStatement().circuit_fingerprint;
        statement_.output_claim.mle_evaluation=
            proof_.gkr_proof.output_evaluation;
        bindZkGPTProof(statement_,proof_);
    }

        statement_bytes_=serializeZkGPTPublicStatement(statement_);
        proof_bytes_=serializeZkGPTProof(proof_);
        ZkGPTPublicStatement decoded_statement;
        ZkGPTProof decoded_proof;
        std::string error;
        if (!deserializeZkGPTPublicStatement(
                statement_bytes_,decoded_statement,&error) ||
            serializeZkGPTPublicStatement(decoded_statement)!=statement_bytes_)
            throw std::runtime_error(
                "small zkGPT statement round-trip failed: "+error);
        if (!deserializeZkGPTProof(proof_bytes_,decoded_proof,&error) ||
            serializeZkGPTProof(decoded_proof)!=proof_bytes_)
            throw std::runtime_error(
                "small zkGPT proof round-trip failed: "+error);
        if (!verifyZkGPT(decoded_statement,decoded_proof,&error))
            throw std::runtime_error(
                "small zkGPT production verification failed: "+error);
        statement_=std::move(decoded_statement);
        proof_=std::move(decoded_proof);
}

SmallZkGPTArtifactFixture::~SmallZkGPTArtifactFixture() = default;

void SmallZkGPTArtifactFixture::writeArtifact(
    const std::string &prefix) const {
    const auto write=[](const std::string &path,
                        const std::vector<std::uint8_t> &bytes) {
        std::ofstream output(path,std::ios::binary);
        if (!output) throw std::runtime_error("cannot create "+path);
        output.write(reinterpret_cast<const char *>(bytes.data()),bytes.size());
        if (!output) throw std::runtime_error("cannot write "+path);
    };
    write(prefix+".statement.bin",statement_bytes_);
    write(prefix+".proof.bin",proof_bytes_);
}

}  // namespace gkr_test
