#include "memory_diagnostics.hpp"
#include "zkgpt_protocol.hpp"

#include "gkr_serialization.hpp"
#include "gkr_verifier.hpp"
#include "models.hpp"
#include "range_serialization.hpp"
#include "zkgpt_serialization.hpp"

#include <limits>
#include <stdexcept>
#include <iostream>
#include "timer.hpp"

namespace {

void appendBlob(Transcript &transcript, const std::string &label,
                const std::vector<std::uint8_t> &bytes) {
    transcript.appendString(label,std::string(
        reinterpret_cast<const char *>(bytes.data()),bytes.size()));
}

Fr deriveBinding(const ZkGPTPublicStatement &statement,
                 const ZkGPTProof &proof) {
    Transcript transcript(kZkGPTArtifactTranscriptDomain);
    appendBlob(transcript,"statement",serializeZkGPTPublicStatement(statement));
    appendBlob(transcript,"gkr_proof",serializeGKRProof(proof.gkr_proof));
    appendBlob(transcript,"range_proof",serializeRangeProof(proof.range_proof));
    return transcript.challenge("zkGPT.artifact.final");
}

bool sameShape(const WitnessShape &lhs,const WitnessShape &rhs) {
    return lhs.sequence_length==rhs.sequence_length &&
        lhs.layer_count==rhs.layer_count &&
        lhs.head_count==rhs.head_count &&
        lhs.head_dimension==rhs.head_dimension &&
        lhs.hidden_dimension==rhs.hidden_dimension &&
        lhs.mlp_dimension==rhs.mlp_dimension;
}

void validateShapeForBuilder(const WitnessShape &shape) {
    const std::size_t maximum=static_cast<std::size_t>(
        std::numeric_limits<int>::max());
    if (shape.sequence_length!=32 || shape.layer_count==0 ||
        shape.head_count==0 || shape.head_dimension==0 ||
        shape.hidden_dimension==0 || shape.mlp_dimension==0 ||
        shape.layer_count>maximum || shape.head_count>maximum ||
        shape.head_dimension>maximum || shape.hidden_dimension>maximum ||
        shape.mlp_dimension>maximum ||
        shape.head_count>maximum/shape.head_dimension ||
        shape.head_count*shape.head_dimension!=shape.hidden_dimension)
        throw std::invalid_argument("unsupported zkGPT model shape");
}

} // namespace

void bindZkGPTProof(const ZkGPTPublicStatement &statement,
                    ZkGPTProof &proof) {
    if (statement.protocol_version!=kZkGPTProtocolVersion)
        throw std::invalid_argument("unsupported zkGPT protocol version");
    proof.transcript_binding=deriveBinding(statement,proof);
}

void validateZkGPTDeploymentBudget(const ZkGPTPublicStatement &statement) {
    const WitnessShape &shape=statement.model_shape;
    // Production v2 is deployed for the GPT-2-small family. Smaller fixtures
    // remain useful for tests, while a larger profile needs its own reviewed
    // CPU/RSS budget before it reaches buildPublicCircuit().
    if (shape.sequence_length!=32 || shape.layer_count==0 ||
        shape.layer_count>12 || shape.head_count==0 || shape.head_count>12 ||
        shape.head_dimension==0 || shape.head_dimension>64 ||
        shape.hidden_dimension==0 || shape.hidden_dimension>768 ||
        shape.mlp_dimension==0 || shape.mlp_dimension>3072 ||
        shape.head_count>std::numeric_limits<std::size_t>::max()/
            shape.head_dimension ||
        shape.head_count*shape.head_dimension!=shape.hidden_dimension)
        throw std::length_error(
            "zkGPT model shape exceeds deployment resource budget");
}

bool verifyZkGPT(const ZkGPTPublicStatement &statement,
                 const ZkGPTProof &proof, std::string *error) {
    try {
        zkgpt::memory_diagnostics::logProcess("verifier_begin");
        if (statement.protocol_version!=kZkGPTProtocolVersion)
            throw std::runtime_error("unsupported zkGPT protocol version");
        if (statement.circuit_id!="gpt2-quantized-v1")
            throw std::runtime_error("unsupported zkGPT circuit id");
        validateShapeForBuilder(statement.model_shape);
        if (!sameShape(statement.model_shape,
                       statement.range_statement.shape))
            throw std::runtime_error(
                "top-level and Range model shapes do not match");
        if (proof.gkr_proof.output_evaluation!=
            statement.output_claim.mle_evaluation)
            throw std::runtime_error(
                "GKR proof output does not match public output claim");
        if (deriveBinding(statement,proof)!=proof.transcript_binding)
            throw std::runtime_error("zkGPT top-level transcript mismatch");

        // Verify Range and GKR serially so each cost is measured separately.
        std::string range_error;
        timer range_timer;
        range_timer.start();
        range_verifier range;
        const bool range_valid=range.verify(
            statement.range_statement,proof.range_proof,&range_error);
        range_timer.stop();
        std::cout << "Independent verifier Range verification time: "
                  << range_timer.elapse_sec() << "s\n";
        if (!range_valid)
            throw std::runtime_error("Range verification failed: "+range_error);

        zkgpt::memory_diagnostics::logProcess("verifier_after_range");
        zkgpt::memory_diagnostics::logProcess("verifier_before_public_rebuild");
        timer circuit_timer;
        circuit_timer.start();
        LLM network(static_cast<int>(statement.model_shape.layer_count),
                    static_cast<int>(statement.model_shape.head_count),
                    static_cast<int>(statement.model_shape.head_dimension),
                    static_cast<int>(statement.model_shape.hidden_dimension),
                    static_cast<int>(statement.model_shape.mlp_dimension));
        const auto circuit=network.buildPublicCircuit();
        zkgpt::memory_diagnostics::logProcess("verifier_after_public_rebuild");
        zkgpt::memory_diagnostics::logCircuit("verifier_after_public_rebuild",circuit);
        // The fingerprint of the rebuilt circuit is checked inside verifyGKR.
        // The Range layout is not covered by it, so bind it to the registry
        // the public circuit builder produced.
        requireRangeLayoutMatchesRegistry(statement.range_statement,
                                          network.getWitnessRegistry());
        circuit_timer.stop();
        std::cout << "Independent verifier public-circuit rebuild time: "
                  << circuit_timer.elapse_sec() << "s\n";

        GKRPublicStatement gkr_statement;
        gkr_statement.protocol_version=statement.protocol_version;
        gkr_statement.model_shape=statement.model_shape;
        gkr_statement.val0_log_size=
            statement.range_statement.val0_log_size;
        gkr_statement.val0_commitment=
            statement.range_statement.val0_commitment;
        gkr_statement.val0_generator_domain=
            statement.range_statement.val0_generator_domain;
        gkr_statement.circuit_fingerprint=statement.circuit_fingerprint;
        gkr_statement.output_evaluation=
            statement.output_claim.mle_evaluation;
        std::string gkr_error;
        timer gkr_timer;
        gkr_timer.start();
        const bool gkr_valid=verifyGKR(
            circuit,gkr_statement,proof.gkr_proof,&gkr_error);
        gkr_timer.stop();
        std::cout << "Independent verifier GKR verification time: "
                  << gkr_timer.elapse_sec() << "s\n";
        zkgpt::memory_diagnostics::logProcess("verifier_after_gkr");
        if (!gkr_valid)
            throw std::runtime_error("GKR verification failed: "+gkr_error);
        return true;
    } catch (const std::exception &exception) {
        if (error) *error=exception.what();
        return false;
    }
}
