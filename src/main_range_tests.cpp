#include "range_prover.hpp"
#include "range_wide.hpp"
#include "hyrax.hpp"
#include "hyrax_opening.hpp"
#include "gkr_serialization.hpp"
#include "gkr_proof_builder.hpp"
#include "gkr_verifier.hpp"
#include "range_serialization.hpp"
#include "zkgpt_serialization.hpp"
#include "range_sumcheck.hpp"
#include "range_logup.hpp"
#include "hyrax_rp.hpp"
#include "utils.hpp"
#include "tests/gkr_test_fixture.hpp"
#include <cybozu/sha2.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using range_wide::SignedWide;
using range_wide::UnsignedWide;
using namespace mcl::bn;

namespace {

void require(bool condition, const std::string &message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename Bytes>
std::string hexBytes(const Bytes &bytes) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const auto byte : bytes)
        output << std::setw(2) << static_cast<unsigned>(byte);
    return output.str();
}

std::array<std::uint8_t,32> sha256Bytes(
    const std::vector<std::uint8_t> &bytes) {
    cybozu::Sha256 hash;
    std::array<std::uint8_t,32> digest{};
    require(hash.digest(digest.data(),digest.size(),bytes.data(),bytes.size())==
                digest.size(),
            "SHA-256 test-vector digest failed");
    return digest;
}

std::vector<std::uint8_t> bytesFromHex(const std::string &hex) {
    require(hex.size()%2==0,"odd canonical hex length");
    auto nibble=[](char value) -> std::uint8_t {
        if (value>='0' && value<='9') return value-'0';
        if (value>='a' && value<='f') return value-'a'+10;
        if (value>='A' && value<='F') return value-'A'+10;
        throw std::runtime_error("invalid canonical hex digit");
    };
    std::vector<std::uint8_t> bytes(hex.size()/2);
    for (std::size_t i=0;i<bytes.size();++i)
        bytes[i]=static_cast<std::uint8_t>(
            (nibble(hex[2*i])<<4)|nibble(hex[2*i+1]));
    return bytes;
}

void appendTestLe64(std::vector<std::uint8_t> &bytes,std::uint64_t value) {
    for (unsigned i=0;i<8;++i)
        bytes.push_back(static_cast<std::uint8_t>(value>>(8*i)));
}

void appendTestTranscriptItem(std::vector<std::uint8_t> &bytes,
                              const std::string &label,
                              const std::uint8_t *payload,
                              std::size_t payload_size) {
    appendTestLe64(bytes,label.size());
    bytes.insert(bytes.end(),label.begin(),label.end());
    appendTestLe64(bytes,payload_size);
    bytes.insert(bytes.end(),payload,payload+payload_size);
}

void appendTestTranscriptString(std::vector<std::uint8_t> &bytes,
                                const std::string &label,
                                const std::string &payload) {
    appendTestTranscriptItem(bytes,label,
        reinterpret_cast<const std::uint8_t *>(payload.data()),payload.size());
}

void appendTestTranscriptU64(std::vector<std::uint8_t> &bytes,
                             const std::string &label,std::uint64_t value) {
    std::uint8_t payload[8];
    for (unsigned i=0;i<8;++i)
        payload[i]=static_cast<std::uint8_t>(value>>(8*i));
    appendTestTranscriptItem(bytes,label,payload,sizeof(payload));
}

std::vector<std::uint8_t> encodeInitialGkrTranscriptForVector(
    const GKRPublicStatement &statement) {
    std::vector<std::uint8_t> bytes;
    appendTestTranscriptString(bytes,"domain",kGKRTranscriptDomain);
    appendTestTranscriptU64(bytes,"protocol.version",statement.protocol_version);
    appendTestTranscriptString(bytes,"curve.id","BN254");
    appendTestTranscriptItem(bytes,"circuit.fingerprint",
        statement.circuit_fingerprint.data(),
        statement.circuit_fingerprint.size());
    appendTestTranscriptU64(bytes,"model.sequence_length",
                            statement.model_shape.sequence_length);
    appendTestTranscriptU64(bytes,"model.layer_count",
                            statement.model_shape.layer_count);
    appendTestTranscriptU64(bytes,"model.head_count",
                            statement.model_shape.head_count);
    appendTestTranscriptU64(bytes,"model.head_dimension",
                            statement.model_shape.head_dimension);
    appendTestTranscriptU64(bytes,"model.hidden_dimension",
                            statement.model_shape.hidden_dimension);
    appendTestTranscriptU64(bytes,"model.mlp_dimension",
                            statement.model_shape.mlp_dimension);
    appendTestTranscriptU64(bytes,"input.log_size",statement.val0_log_size);
    appendTestTranscriptString(bytes,"input.generator.domain",
                               statement.val0_generator_domain.domain);
    appendTestTranscriptU64(bytes,"input.generator.count",
                            statement.val0_generator_domain.count);
    appendTestTranscriptU64(bytes,"input.commitment_count",
                            statement.val0_commitment.size());
    for (const auto &commitment : statement.val0_commitment) {
        std::vector<std::uint8_t> encoded(G1::getSerializedByteSize());
        require(commitment.serialize(encoded.data(),encoded.size())==
                    encoded.size(),
                "cannot serialize initial Transcript commitment");
        appendTestTranscriptItem(bytes,"input.commitment",
                                 encoded.data(),encoded.size());
    }
    return bytes;
}

template <typename Function>
void requireRejected(const std::string &name, Function verify,
                     std::string &error,
                     const std::string &expected_step = std::string()) {
    error.clear();
    require(!verify(), name+" was accepted");
    require(!error.empty(), name+" was rejected without an error location");
    if (!expected_step.empty())
        require(error.find(expected_step)!=std::string::npos,
                name+" failed at an unexpected step: "+error);
    std::cout << "  [rejected] " << name << ": " << error << std::endl;
}

template <typename Function>
void requireThrows(Function function, const std::string &message) {
    try {
        function();
    } catch (const std::exception &) {
        return;
    }
    throw std::runtime_error(message);
}

void checkRoundTrip(const std::string &name, UnsignedWide value,
                    unsigned bits) {
    const auto chunks=range_wide::decomposeWide(value, bits);
    require(range_wide::reconstructWide(chunks, bits)==value,
            name+" did not reconstruct exactly");
    for (std::size_t i=0;i<chunks.size();++i) {
        const unsigned offset=static_cast<unsigned>(i)*
                              range_wide::kDefaultChunkBits;
        const unsigned actual_bits=std::min(
            range_wide::kDefaultChunkBits, bits-offset);
        require(chunks[i]<(static_cast<unsigned>(1)<<actual_bits),
                name+" produced an out-of-range chunk");
    }
}

void testWideEncodingAndChunks() {
    const UnsignedWide one=1;
    checkRoundTrip("2^63-1", (one<<63)-1, 63);
    checkRoundTrip("2^63", one<<63, 64);
    checkRoundTrip("2^76-1", (one<<76)-1, 76);
    checkRoundTrip("2^100+123", (one<<100)+123, 126);
    checkRoundTrip("2^125-1", (one<<125)-1, 125);

    require(range_wide::encodeSigned(-1, 64)==(one<<63)-1,
            "signed -1 offset encoding is incorrect");
    require(range_wide::encodeSigned(-(static_cast<SignedWide>(one<<63)), 64)==0,
            "signed -2^63 offset encoding is incorrect");
    require(range_wide::encodeSigned(
                static_cast<SignedWide>((one<<63)-1), 64)==(one<<64)-1,
            "signed 2^63-1 offset encoding is incorrect");

    const unsigned widths[]={1, 9, 10, 63, 64, 76, 100, 125, 126};
    for (unsigned bits : widths)
        checkRoundTrip(std::to_string(bits)+"-bit maximum",
                       range_wide::limitForBits(bits)-1, bits);

    requireThrows([&] { range_wide::encodeUnsigned(
        static_cast<SignedWide>(one<<126), 126); },
        "2^126 was accepted as a 126-bit value");
    requireThrows([&] { range_wide::encodeSigned(
        static_cast<SignedWide>(one<<63), 64); },
        "2^63 was accepted as a signed 64-bit value");

    auto chunks=range_wide::decomposeWide((one<<125)-1, 126);
    chunks.back()=static_cast<std::uint16_t>(1u<<9);
    requireThrows([&] { range_wide::reconstructWide(chunks, 126); },
                  "an oversized final chunk was accepted");
}

Fr fieldFromWide(UnsignedWide value) {
    Fr result;
    result.setStr(range_wide::unsignedWideToString(value));
    return result;
}

void testBuildFromWitnessWidePath() {
    const UnsignedWide one=1;
    std::vector<F> val0={
        fieldFromWide((one<<63)-1),
        fieldFromWide(one<<63),
        fieldFromWide((one<<76)-1),
        fieldFromWide((one<<100)+123),
        fieldFromWide((one<<125)-1),
        Fr(-1),
        fieldFromWide(one<<63)
    };
    val0.back()=-val0.back();

    WitnessRegistry registry;
    registry.setShape({32, 12, 12, 64, 768, 3072});
    registry.addRange(WitnessKind::ROUNDING, "wide_u63", 0, 1, 63, false);
    registry.addRange(WitnessKind::ROUNDING, "wide_u64", 1, 1, 64, false);
    registry.addRange(WitnessKind::ROUNDING, "wide_u76", 2, 1, 76, false);
    registry.addRange(WitnessKind::ROUNDING, "wide_u101", 3, 1, 101, false);
    registry.addRange(WitnessKind::ROUNDING, "wide_u125", 4, 1, 125, false);
    registry.addRange(WitnessKind::ROUNDING, "wide_s64_neg1", 5, 1, 64, true);
    registry.addRange(WitnessKind::ROUNDING, "wide_s64_min", 6, 1, 64, true);

    range_prover prover(12, 12, 64, 768, 3072, 32, 1, 1);
    prover.buildFromWitness(val0, registry);
    prover.verifyWitnessConsistency();

    auto &first=prover.ops.front().constraints.front();
    first.chunk_queries.front().chunks.front()^=1;
    requireThrows([&] { prover.verifyWitnessConsistency(); },
                  "tampered built chunk was accepted by host consistency check");

    std::vector<F> oversized={fieldFromWide(one<<126)};
    WitnessRegistry oversized_registry;
    oversized_registry.setShape({32, 12, 12, 64, 768, 3072});
    oversized_registry.addRange(WitnessKind::ROUNDING, "oversized", 0, 1,
                                126, false);
    range_prover oversized_prover(12, 12, 64, 768, 3072, 32, 1, 1);
    requireThrows([&] {
        oversized_prover.buildFromWitness(oversized, oversized_registry);
    }, "2^126 field value was accepted by a 126-bit witness constraint");
}

void writeArtifact(const std::string &path,
                   const std::vector<std::uint8_t> &bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("cannot create artifact: "+path);
    if (!bytes.empty())
        output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    if (!output) throw std::runtime_error("cannot write artifact: "+path);
}

void proveWideRangeKernel(const std::string &artifact_prefix) {
    const UnsignedWide one=1;
    std::vector<F> val0={
        Fr(0), Fr(1), fieldFromWide((one<<63)-1), fieldFromWide(one<<63),
        fieldFromWide((one<<76)-1), fieldFromWide((one<<100)+123),
        fieldFromWide((one<<125)-1), fieldFromWide((one<<126)-1)
    };
    WitnessRegistry registry;
    registry.setShape({32, 12, 12, 64, 768, 3072});
    registry.addRange(WitnessKind::ROUNDING, "wide_kernel_u126", 0,
                      val0.size(), 126, false);
    range_prover prover(12, 12, 64, 768, 3072, 32, 1, 1);
    prover.init();
    prover.buildFromWitness(val0, registry);
    std::vector<G1> val0_generators(4);
    const G1 val0_u=gen_gi(val0_generators.data(), val0_generators.size());
    std::vector<Fr> oversized_commitment_scalar={fieldFromWide(one<<127)};
    requireThrows([&] {
        G1 *commitment=prover_commit(
            oversized_commitment_scalar.data(), val0_generators.data(), 0, 1);
        delete[] commitment;
    }, "a 2^127 scalar was accepted by the signed-128 Hyrax commitment");
    G1 *val0_commitments=prover_commit(
        val0.data(), val0_generators.data(), 3, 1);
    const auto statement=prover.makePublicStatement(
        3, val0_commitments, 2, val0_generators.data(),
        val0_generators.size(), val0_u);
    delete[] val0_commitments;
    require(statement.queries.size()==1 &&
            statement.queries[0].chunk_bits.size()==14 &&
            std::all_of(statement.queries[0].chunk_bits.begin(),
                        statement.queries[0].chunk_bits.end(),
                        [](unsigned bits) { return bits==9; }),
            "126-bit fixture has unexpected radix chunk widths");
    const auto proof=prover.proveStageB(statement);

    const auto statement_bytes=serializeRangePublicStatement(statement);
    const auto proof_bytes=serializeRangeProof(proof);
    if (!artifact_prefix.empty()) {
        writeArtifact(artifact_prefix+".statement.bin", statement_bytes);
        writeArtifact(artifact_prefix+".proof.bin", proof_bytes);
    }
    RangePublicStatement decoded_statement;
    RangeProof decoded_proof;
    std::string serialization_error;
    require(deserializeRangePublicStatement(
                statement_bytes, decoded_statement, &serialization_error),
            "Range statement round-trip failed: "+serialization_error);
    require(deserializeRangeProof(
                proof_bytes, decoded_proof, &serialization_error),
            "Range proof round-trip failed: "+serialization_error);
    require(serializeRangePublicStatement(decoded_statement)==statement_bytes,
            "Range statement canonical round-trip changed bytes");
    require(serializeRangeProof(decoded_proof)==proof_bytes,
            "Range proof canonical round-trip changed bytes");
    require(decoded_proof.membership_prover_time==0 &&
            decoded_proof.reconstruction_prover_time==0,
            "benchmark timing fields were serialized into Range proof");
    require(estimateRangeProofSizeBytes(proof)==proof_bytes.size(),
            "Range proof size estimate differs from serialized size");

    range_verifier verifier;
    std::string error;
    require(verifier.verify(decoded_statement, decoded_proof, &error),
            "deserialized Stage B proof failed: "+error);
    require(verifier.verify(statement, proof, &error),
            "honest full Range proof failed: "+error);
    const auto verify_start=std::chrono::steady_clock::now();
    require(verifier.verify(statement, proof, &error),
            "full Stage B verification failed: "+error);
    const double verifier_seconds=std::chrono::duration<double>(
        std::chrono::steady_clock::now()-verify_start).count();
    std::cout << "Stage B metrics: statement_bytes=" << statement_bytes.size()
              << ", proof_bytes=" << proof_bytes.size()
              << ", prover_seconds=" << proof.totalProverTime()
              << ", verifier_seconds=" << verifier_seconds << std::endl;

    auto trailing_statement=statement_bytes;
    trailing_statement.push_back(0);
    require(!deserializeRangePublicStatement(
                trailing_statement, decoded_statement, &serialization_error) &&
            serialization_error.find("trailing bytes")!=std::string::npos,
            "Range statement trailing bytes were accepted");
    auto truncated_proof=proof_bytes;
    truncated_proof.pop_back();
    require(!deserializeRangeProof(
                truncated_proof, decoded_proof, &serialization_error) &&
            serialization_error.find("truncated")!=std::string::npos,
            "truncated Range proof was accepted");
    auto v1_proof=proof_bytes;
    v1_proof[4]=1;
    v1_proof[5]=0;
    v1_proof[6]=0;
    v1_proof[7]=0;
    require(!deserializeRangeProof(
                v1_proof,decoded_proof,&serialization_error) &&
            serialization_error.find("unsupported Range proof version")!=
                std::string::npos,
            "Range v1 proof was accepted by the v2 decoder");
    auto oversized_length=proof_bytes;
    std::fill(oversized_length.begin()+8, oversized_length.begin()+12, 0xff);
    require(!deserializeRangeProof(
                oversized_length, decoded_proof, &serialization_error) &&
            serialization_error.find("length exceeds limit")!=std::string::npos,
            "oversized Range proof vector length was accepted");
    auto invalid_g1=proof_bytes;
    require(invalid_g1.size()>20+G1::getSerializedByteSize(),
            "serialized proof is too short for G1 mutation test");
    std::fill(invalid_g1.begin()+20,
              invalid_g1.begin()+20+G1::getSerializedByteSize(), 0xff);
    require(!deserializeRangeProof(
                invalid_g1, decoded_proof, &serialization_error) &&
            serialization_error.find("G1")!=std::string::npos,
            "invalid serialized G1 was accepted");
    auto invalid_fr=proof_bytes;
    require(invalid_fr.size()>=32,
            "serialized proof is too short for Fr mutation test");
    std::fill(invalid_fr.end()-32, invalid_fr.end(), 0xff);
    require(!deserializeRangeProof(
                invalid_fr, decoded_proof, &serialization_error) &&
            serialization_error.find("Fr")!=std::string::npos,
            "non-canonical serialized Fr was accepted");
    std::cout << "Range artifact round-trip and malformed-input tests passed"
              << std::endl;

    std::cout << "Commitment opening soundness tests:" << std::endl;

    auto bad_chunk_commitment=proof;
    bad_chunk_commitment.chunk_commitments[0][0][0]+=prover.GG;
    requireRejected("modified chunk commitment", [&] {
        return verifier.verify(statement, bad_chunk_commitment, &error);
    }, error, "LogUp value commitment mismatch");

    auto bad_chunk_opening=proof;
    bad_chunk_opening.reconstruction_proofs[0]
        .encoded_chunk_opening.ipa.final_witness+=Fr(1);
    requireRejected("modified encoded chunk MLE opening", [&] {
        return verifier.verify(statement, bad_chunk_opening, &error);
    }, error, "encoded chunk MLE opening failed");

    auto bad_sparse_opening=proof;
    bad_sparse_opening.val0_openings[0].patterns[0].opening.final_witness+=Fr(1);
    requireRejected("modified val[0] sparse opening", [&] {
        return verifier.verify(statement, bad_sparse_opening, &error);
    }, error, "val[0] sparse opening failed");

    auto bad_encoded_evaluation=proof;
    bad_encoded_evaluation.reconstruction_proofs[0].encoded_evaluation+=Fr(1);
    requireRejected("modified encoded evaluation", [&] {
        return verifier.verify(statement, bad_encoded_evaluation, &error);
    }, error, "encoded chunk MLE opening failed");

    // Preserve the radix aggregate while changing two individual chunk
    // commitments.  Reconstruction alone only needs their linear aggregate;
    // the complete verifier must still reject because each commitment is
    // separately bound to its LogUp membership proof.
    auto cancelling_commitments=proof;
    const auto aggregate_before=aggregateRangeChunkCommitments(
        proof.chunk_commitments[0],statement.queries[0].chunk_bits);
    const G1 delta=prover.GG;
    cancelling_commitments.chunk_commitments[0][0][0]+=delta;
    Fr next_weight=1;
    for (unsigned bit=0;bit<statement.queries[0].chunk_bits[0];++bit)
        next_weight+=next_weight;
    Fr inverse_weight;
    Fr::inv(inverse_weight,next_weight);
    G1 scaled_delta;
    G1::mul(scaled_delta,delta,inverse_weight);
    cancelling_commitments.chunk_commitments[0][1][0]-=scaled_delta;
    const auto aggregate_after=aggregateRangeChunkCommitments(
        cancelling_commitments.chunk_commitments[0],
        statement.queries[0].chunk_bits);
    require(aggregate_after==aggregate_before,
            "radix-cancelling commitment mutation changed aggregate");
    requireRejected("radix-cancelling chunk commitments", [&] {
        return verifier.verify(statement,cancelling_commitments,&error);
    }, error, "LogUp value commitment mismatch");

    auto bad_commitment_statement=statement;
    bad_commitment_statement.val0_commitment[0]+=prover.GG;
    requireRejected("modified val[0] commitment", [&] {
        return verifier.verify(bad_commitment_statement, proof, &error);
    }, error, "opening failed");

    std::vector<Fr> alternate_values=val0;
    alternate_values[0]+=Fr(1);
    G1 *alternate_commitments=prover_commit(
        alternate_values.data(), val0_generators.data(), 3, 1);
    auto alternate_statement=statement;
    alternate_statement.val0_commitment.assign(
        alternate_commitments, alternate_commitments+2);
    delete[] alternate_commitments;
    requireRejected("alternate val[0] witness commitment with old proof", [&] {
        return verifier.verify(alternate_statement, proof, &error);
    }, error, "opening failed");

    auto bad_region_statement=statement;
    ++bad_region_statement.regions[0].bits;
    requireRejected("modified region bits", [&] {
        return verifier.verify(bad_region_statement, proof, &error);
    }, error, "range region metadata is inconsistent");

    auto bad_logup_multiplicity=proof;
    bad_logup_multiplicity.membership_proofs[0][0]
        .multiplicity_evaluation+=Fr(1);
    requireRejected("modified LogUp multiplicity evaluation", [&] {
        return verifier.verify(statement, bad_logup_multiplicity, &error);
    }, error, "LogUp table reciprocal sumcheck failed");

    auto bad_logup_sumcheck=proof;
    bad_logup_sumcheck.membership_proofs[0][0]
        .reciprocal_value_sumcheck.rounds[0].value_at_2+=Fr(1);
    requireRejected("modified LogUp sumcheck polynomial", [&] {
        return verifier.verify(statement, bad_logup_sumcheck, &error);
    }, error, "LogUp value reciprocal sumcheck failed");

    auto bad_logup_opening=proof;
    bad_logup_opening.membership_proofs[0][0]
        .reciprocal_value_opening.ipa.final_witness+=Fr(1);
    requireRejected("modified LogUp reciprocal opening", [&] {
        return verifier.verify(statement, bad_logup_opening, &error);
    }, error, "LogUp value reciprocal opening failed");

    std::cout << "Transcript tampering test:" << std::endl;
    auto bad_transcript=proof;
    bad_transcript.transcript_binding+=Fr(1);
    requireRejected("modified Fiat-Shamir transcript binding", [&] {
        return verifier.verify(statement, bad_transcript, &error);
    }, error, "Range Proof transcript binding mismatch");

    std::cout << "126-bit Range Proof kernel test passed" << std::endl;
    std::cout << "Stage B commitment opening negative tests passed"
              << std::endl;
}

void testMixedSignedStageB() {
    std::vector<F> val0={Fr(-3), Fr(7), Fr(-2)};
    WitnessRegistry registry;
    registry.setShape({32, 12, 12, 64, 768, 3072});
    registry.addRange(WitnessKind::ROUNDING, "mixed_signed_a", 0, 1,
                      64, true);
    registry.addRange(WitnessKind::ROUNDING, "mixed_unsigned", 1, 1,
                      64, false);
    registry.addRange(WitnessKind::ROUNDING, "mixed_signed_b", 2, 1,
                      64, true);
    range_prover prover(12, 12, 64, 768, 3072, 32, 1, 1);
    prover.init();
    prover.buildFromWitness(val0, registry);

    std::vector<G1> val0_generators(2);
    const G1 val0_u=gen_gi(val0_generators.data(), val0_generators.size());
    std::vector<Fr> committed_values(4, Fr(0));
    std::copy(val0.begin(), val0.end(), committed_values.begin());
    G1 *commitments=prover_commit(
        committed_values.data(), val0_generators.data(), 2, 1);
    const auto statement=prover.makePublicStatement(
        2, commitments, 2, val0_generators.data(), val0_generators.size(),
        val0_u);
    delete[] commitments;
    require(statement.queries.size()==1 &&
            statement.queries[0].chunk_bits.back()==1,
            "64-bit fixture does not end in a 1-bit radix chunk");
    const auto proof=prover.proveStageB(statement);
    range_verifier verifier;
    std::string error;
    require(verifier.verify(statement, proof, &error),
            "mixed signed/unsigned Stage B proof failed: "+error);
    require(!proof.val0_openings[0].signed_bias_evaluation.isZero(),
            "mixed signed query produced a zero signed bias");

    const auto serial_bytes=serializeRangeProof(proof);
    for (const int threads : {8,32}) {
        range_prover parallel_prover(
            12,12,64,768,3072,32,threads,1);
        parallel_prover.init();
        parallel_prover.buildFromWitness(val0,registry);
        const auto parallel_proof=parallel_prover.proveStageB(statement);
        require(serializeRangeProof(parallel_proof)==serial_bytes,
                "parallel Stage B proof bytes differ with "+
                std::to_string(threads)+" threads");
        require(verifier.verify(statement,parallel_proof,&error),
                "parallel Stage B proof failed with "+
                std::to_string(threads)+" threads: "+error);
    }

    auto bad_signed_bias=proof;
    bad_signed_bias.val0_openings[0].signed_bias_evaluation+=Fr(1);
    requireRejected("modified signed bias evaluation", [&] {
        return verifier.verify(statement, bad_signed_bias, &error);
    }, error, "sparse signed bias evaluation mismatch");

    auto bad_signed_statement=statement;
    bad_signed_statement.regions[1].is_signed=true;
    requireRejected("modified region signed flag", [&] {
        return verifier.verify(bad_signed_statement, proof, &error);
    }, error, "opening failed");
    auto bad_offset_statement=statement;
    bad_offset_statement.regions[1].val0_offset=3;
    requireRejected("modified non-overlapping region offset", [&] {
        return verifier.verify(bad_offset_statement, proof, &error);
    }, error, "opening failed");

    require(proof.val0_openings[0].patterns.size()>=2,
            "sparse replay fixture has fewer than two patterns");
    auto replayed_sparse_pattern=proof;
    replayed_sparse_pattern.val0_openings[0].patterns[0].opening=
        proof.val0_openings[0].patterns[1].opening;
    requireRejected("sparse IPA replayed across patterns", [&] {
        return verifier.verify(statement,replayed_sparse_pattern,&error);
    }, error, "sparse pattern opening failed");

    auto overlapping_statement=statement;
    overlapping_statement.regions[1].val0_offset=0;
    requireRejected("overlapping public regions", [&] {
        return verifier.verify(overlapping_statement, proof, &error);
    }, error, "overlapping public val[0] regions");
    auto uncovered_statement=statement;
    uncovered_statement.regions.pop_back();
    requireRejected("query not fully covered by regions", [&] {
        return verifier.verify(uncovered_statement, proof, &error);
    }, error, "do not cover the query");

    prover.ops.front().constraints.front().inputs.back().encoded=1;
    prover.ops.front().constraints.front().chunk_queries.front().chunks.back()=1;
    requireThrows([&] { prover.proveStageB(statement); },
                  "nonzero padded encoded entry was accepted");
    prover.ops.front().constraints.front().inputs.back().encoded=0;
    requireThrows([&] { prover.proveStageB(statement); },
                  "nonzero padded chunk was accepted");
    std::cout << "Mixed signed/unsigned/padding Stage B test passed"
              << std::endl;
}

void testRadixTailWidthStageB() {
    std::vector<F> val0={Fr((1ULL<<31)+7),Fr(0)};
    WitnessRegistry registry;
    registry.setShape({32,12,12,64,768,3072});
    registry.addRange(WitnessKind::ROUNDING,"radix_tail_u32",0,1,32,false);
    range_prover prover(12,12,64,768,3072,32,8,1);
    prover.init();
    prover.buildFromWitness(val0,registry);
    std::vector<G1> generators(2);
    const G1 u=gen_gi(generators.data(),generators.size());
    G1 *commitments=prover_commit(
        val0.data(),generators.data(),1,1);
    const auto statement=prover.makePublicStatement(
        1,commitments,1,generators.data(),generators.size(),u);
    delete[] commitments;
    require(statement.queries.size()==1 &&
            statement.queries[0].chunk_bits==
                std::vector<unsigned>({9,9,9,5}),
            "32-bit fixture does not use 9/9/9/5 radix chunks");
    const auto proof=prover.proveStageB(statement);
    range_verifier verifier;
    std::string error;
    require(verifier.verify(statement,proof,&error),
            "32-bit radix-tail Stage B proof failed: "+error);
    std::cout << "32-bit radix-tail aggregation test passed" << std::endl;
}

void testLogUpReplayStageB() {
    std::vector<F> val0={Fr(3), Fr(17)};
    WitnessRegistry registry;
    registry.setShape({32, 12, 12, 64, 768, 3072});
    registry.addRange(WitnessKind::ROUNDING, "replay_q0", 0, 1, 9, false);
    registry.addRange(WitnessKind::ROUNDING, "replay_q1", 1, 1, 10, false);
    range_prover prover(12, 12, 64, 768, 3072, 32, 1, 1);
    prover.init();
    prover.buildFromWitness(val0, registry);
    std::vector<G1> generators(2);
    const G1 u=gen_gi(generators.data(), generators.size());
    std::vector<Fr> committed(4, Fr(0));
    std::copy(val0.begin(), val0.end(), committed.begin());
    G1 *commitments=prover_commit(committed.data(), generators.data(), 2, 1);
    const auto statement=prover.makePublicStatement(
        2, commitments, 2, generators.data(), generators.size(), u);
    delete[] commitments;
    const auto proof=prover.proveStageB(statement);
    range_verifier verifier;
    std::string error;
    require(verifier.verify(statement, proof, &error),
            "replay fixture failed verification: "+error);

    auto replay_reconstruction=proof;
    replay_reconstruction.reconstruction_proofs[1].encoded_chunk_opening=
        proof.reconstruction_proofs[0].encoded_chunk_opening;
    requireRejected("encoded opening replayed across queries", [&] {
        return verifier.verify(statement,replay_reconstruction,&error);
    }, error, "encoded chunk MLE opening failed");

    auto replay_query=proof;
    replay_query.membership_proofs[1][0]=proof.membership_proofs[0][0];
    requireRejected("LogUp proof replayed across queries", [&] {
        return verifier.verify(statement, replay_query, &error);
    }, error, "LogUp public metadata mismatch");

    auto replay_chunk=proof;
    replay_chunk.membership_proofs[1][0]=proof.membership_proofs[1][1];
    requireRejected("LogUp proof replayed across chunks", [&] {
        return verifier.verify(statement, replay_chunk, &error);
    }, error, "LogUp public metadata mismatch");

    auto wrong_chunk_bits=statement;
    wrong_chunk_bits.queries[0].chunk_bits[0]=8;
    requireRejected("commitment paired with wrong chunk bits", [&] {
        return verifier.verify(wrong_chunk_bits, proof, &error);
    }, error, "range chunk widths do not cover the query");

    auto bad_multiplicity=proof;
    bad_multiplicity.membership_proofs[0][0]
        .multiplicity_commitment[0]+=prover.GG;
    bad_multiplicity.membership_proofs[0][0]
        .multiplicity_evaluation+=Fr(1);
    requireRejected("modified multiplicity commitment and evaluation", [&] {
        return verifier.verify(statement, bad_multiplicity, &error);
    }, error, "LogUp");

    auto bad_reciprocal_commitment=proof;
    bad_reciprocal_commitment.membership_proofs[0][0]
        .reciprocal_value_commitment[0]+=prover.GG;
    requireRejected("modified reciprocal commitment", [&] {
        return verifier.verify(statement, bad_reciprocal_commitment, &error);
    }, error, "LogUp");
    std::cout << "LogUp replay and metadata mutation tests passed" << std::endl;
}

void testSumcheckConsistency() {
    range_prover prover;

    Fr linear_values[]={Fr(1), Fr(2), Fr(3), Fr(4)};
    auto linear=prover.sumcheck_deg1(2, linear_values, Fr(10));
    delete[] linear.random;
    requireThrows([&] {
        auto invalid=prover.sumcheck_deg1(2, linear_values, Fr(11));
        delete[] invalid.random;
    }, "degree-1 sumcheck accepted an inconsistent claim");

    Fr point[]={Fr(3), Fr(5)};
    Fr f[]={Fr(2), Fr(7), Fr(11), Fr(13)};
    Fr g[]={Fr(17), Fr(19), Fr(23), Fr(29)};
    Fr *eq=range_proof_get_eq(point, 2);
    Fr claim=0;
    for (int i=0;i<4;++i) claim+=eq[i]*f[i]*g[i];
    delete[] eq;
    auto cubic=prover.sumcheck_deg3(2, point, f, g, claim);
    delete[] cubic.random;
    requireThrows([&] {
        auto invalid=prover.sumcheck_deg3(2, point, f, g, claim+Fr(1));
        delete[] invalid.random;
    }, "degree-3 sumcheck accepted an inconsistent claim");
}

void testSerializableSumchecks() {
    std::vector<Fr> linear_values={Fr(1), Fr(2), Fr(3), Fr(4)};
    Transcript degree1_prover_transcript("degree1-test");
    std::vector<Fr> degree1_point;
    const auto degree1_proof=proveDegree1Sumcheck(
        linear_values, Fr(10), degree1_prover_transcript, "degree1",
        &degree1_point);
    Transcript degree1_verifier_transcript("degree1-test");
    std::vector<Fr> verified_degree1_point;
    std::string error;
    require(verifyDegree1Sumcheck(
                Fr(10), degree1_proof, degree1_verifier_transcript,
                "degree1", &verified_degree1_point, &error),
            "serialized degree-1 sumcheck failed: "+error);
    require(degree1_point==verified_degree1_point,
            "degree-1 transcript points diverged");
    auto bad_degree1=degree1_proof;
    bad_degree1.rounds[0].value_at_0+=Fr(1);
    Transcript bad_degree1_transcript("degree1-test");
    require(!verifyDegree1Sumcheck(
                Fr(10), bad_degree1, bad_degree1_transcript, "degree1",
                nullptr, &error),
            "modified serialized degree-1 round was accepted");

    std::vector<Fr> equality_point={Fr(3), Fr(5)};
    std::vector<Fr> f={Fr(2), Fr(7), Fr(11), Fr(13)};
    std::vector<Fr> g={Fr(17), Fr(19), Fr(23), Fr(29)};
    Fr *weights=range_proof_get_eq(equality_point.data(), 2);
    Fr claim=0;
    for (std::size_t i=0;i<f.size();++i) claim+=weights[i]*f[i]*g[i];
    delete[] weights;
    Transcript degree3_prover_transcript("degree3-test");
    std::vector<Fr> degree3_point;
    const auto degree3_proof=proveDegree3Sumcheck(
        equality_point, f, g, claim, degree3_prover_transcript, "degree3",
        &degree3_point);
    Transcript degree3_verifier_transcript("degree3-test");
    std::vector<Fr> verified_degree3_point;
    require(verifyDegree3Sumcheck(
                equality_point, claim, degree3_proof,
                degree3_verifier_transcript, "degree3",
                &verified_degree3_point, &error),
            "serialized degree-3 sumcheck failed: "+error);
    require(degree3_point==verified_degree3_point,
            "degree-3 transcript points diverged");
    auto bad_degree3=degree3_proof;
    bad_degree3.rounds[0].value_at_2+=Fr(1);
    Transcript bad_degree3_transcript("degree3-test");
    require(!verifyDegree3Sumcheck(
                equality_point, claim, bad_degree3,
                bad_degree3_transcript, "degree3", nullptr, &error),
            "modified serialized degree-3 round was accepted");
}

void testRowMajorMleOpening() {
    std::vector<Fr> values={Fr(1), Fr(2), Fr(3), Fr(4),
                            Fr(5), Fr(6), Fr(7), Fr(8)};
    std::vector<G1> generators(4);
    const G1 u=gen_gi(generators.data(), generators.size());
    G1 *raw_commitments=prover_commit(
        values.data(), generators.data(), 3, 1);
    std::vector<G1> commitments(raw_commitments, raw_commitments+2);
    delete[] raw_commitments;
    const std::vector<Fr> point={Fr(3), Fr(5), Fr(7)};
    std::vector<Fr> folded=values;
    std::size_t active=folded.size();
    for (const Fr &random : point) {
        for (std::size_t i=0;i<active/2;++i)
            folded[i]=folded[2*i]+random*(folded[2*i+1]-folded[2*i]);
        active/=2;
    }
    const Fr evaluation=folded[0];
    Transcript prover_transcript("row-major-mle-test");
    const auto proof=hyraxMleOpenProveFrRowMajor(
        values, commitments, point, generators, u, evaluation,
        prover_transcript, "row-major");
    Transcript verifier_transcript("row-major-mle-test");
    std::string error;
    require(hyraxMleOpenVerifyRowMajor(
                commitments, point, generators, u, evaluation, proof,
                verifier_transcript, "row-major", &error),
            "row-major MLE opening failed: "+error);

    // Hold the commitment, point, dimensions and evaluation fixed.  The same
    // proof must still be unusable under another query/opening label.
    Transcript replay_transcript("row-major-mle-test");
    require(!hyraxMleOpenVerifyRowMajor(
                commitments, point, generators, u, evaluation, proof,
                replay_transcript, "row-major/other-query", &error),
            "same-shape MLE opening replay under another label was accepted");
}

void testCommittedWitnessCapacityLifecycle() {
    const auto run_case=[](std::size_t logical_size) {
        prover instance;
        instance.mat_val=nullptr;
        instance.cc.comm=nullptr;
        instance.cc.w=nullptr;
        try {
            instance.C.init(0,1);
            instance.val.resize(1);
            instance.val[0].reserve(64);
            instance.val[0].resize(logical_size);
            for (std::size_t i=0;i<logical_size;++i)
                instance.val[0][i]=Fr(static_cast<int>(i+1));
            const auto original=instance.val[0];
            initLayer(instance.C.circuit[0],logical_size,layerType::INPUT);
            const int log_size=instance.C.circuit[0].bit_length;
            const std::size_t padded=std::size_t{1}<<log_size;
            const std::size_t generator_count=std::size_t{1}<<
                (log_size-log_size/2);
            instance.gens.resize(generator_count);
            const G1 u=gen_gi(instance.gens.data(),generator_count);
            instance.gens.push_back(u);
            instance.commitInput(instance.gens,1);

            require(instance.val[0]==original,
                    "commitInput changed logical val[0] witness values");
            require(instance.val[0].size()==logical_size,
                    "commitInput did not restore logical val[0] size");
            require(instance.val[0].capacity()==logical_size,
                    "commitInput retained padded val[0] capacity");
            require(instance.cc.l==log_size && instance.cc.w!=nullptr,
                    "commitInput produced invalid padded witness ownership");
            require(instance.val[0].data()!=instance.cc.w,
                    "capacity optimization unexpectedly aliased cc.w");
            for (std::size_t i=0;i<logical_size;++i)
                require(instance.cc.w[i]==original[i],
                        "cc.w differs from logical val[0]");
            for (std::size_t i=logical_size;i<padded;++i)
                require(instance.cc.w[i]==Fr(0),
                        "cc.w padding is nonzero");

            G1 *recomputed=prover_commit(
                instance.cc.w,instance.gens.data(),log_size,1);
            const std::size_t rows=std::size_t{1}<<(log_size/2);
            for (std::size_t row=0;row<rows;++row)
                require(recomputed[row]==instance.cc.comm[row],
                        "compacted witness commitment changed geometry or value");
            delete[] recomputed;

            const Fr committed_first=instance.cc.w[0];
            instance.val[0][0]+=Fr(1);
            require(instance.cc.w[0]==committed_first,
                    "cc.w lifetime unexpectedly aliases compacted val[0]");
            instance.val[0][0]=original[0];

            // Lasso and input opening consume the complete committed domain.
            // Prove that the padded source remains valid after val[0]
            // compaction, including the first element beyond logical size.
            if (logical_size<padded)
                require(instance.cc.w[logical_size]==Fr(0),
                        "committed padded source was invalidated by compaction");
        } catch (...) {
            throw;
        }
    };
    run_case(5);  // padding boundary: logical 5, committed domain 8.
    run_case(8);  // exact power-of-two boundary: no logical padding required.
}

void testGkrProofSerialization() {
    std::vector<G1> generators(2);
    gen_gi(generators.data(), generators.size());
    GKRProof proof;
    proof.output_evaluation=Fr(11);
    GKRLayerProof layer;
    layer.layer_index=3;
    layer.layer_type=static_cast<std::uint32_t>(layerType::FCONN);
    layer.phase1_rounds.push_back({Fr(1),Fr(2),Fr(3)});
    layer.phase2_rounds.push_back({Fr(4),Fr(5),Fr(6)});
    layer.matrix_v_rounds.push_back({Fr(19),Fr(20),Fr(21)});
    layer.final_claim_u0=Fr(7);
    layer.final_claim_u1=Fr(8);
    layer.final_claim_v0=Fr(9);
    layer.final_claim_v1=Fr(10);
    layer.matrix_v_final_claim_u0=Fr(22);
    layer.matrix_v_final_claim_v0=Fr(23);
    proof.layers.push_back(layer);
    proof.lasso.rounds.push_back({Fr(12),Fr(13),Fr(14)});
    proof.lasso.input_evaluation=Fr(15);
    proof.lasso.mapping_evaluation=Fr(16);
    proof.lasso.input_opening.ipa.rounds.push_back(
        {generators[0],generators[1]});
    proof.lasso.input_opening.ipa.final_witness=Fr(17);
    proof.transcript_binding=Fr(18);
    const auto bytes=serializeGKRProof(proof);
    GKRProof decoded;
    std::string error;
    require(deserializeGKRProof(bytes,decoded,&error),
            "GKR proof round-trip failed: "+error);
    require(serializeGKRProof(decoded)==bytes,
            "GKR proof canonical round-trip changed bytes");
    auto normal_with_second_point=proof;
    normal_with_second_point.layers[0].layer_type=
        static_cast<std::uint32_t>(layerType::RELU);
    requireThrows([&] { serializeGKRProof(normal_with_second_point); },
                  "normal layer serialized ignored FCONN second-point data");
    require(bytes.size()>=8 && bytes[4]==3 && bytes[5]==0 &&
                bytes[6]==0 && bytes[7]==0,
            "GKR proof does not carry the v3 encoding version");
    auto v2=bytes;
    v2[4]=2;
    require(!deserializeGKRProof(v2,decoded,&error) &&
                error.find("unsupported GKR proof version")!=std::string::npos,
            "single-point v2 GKR proof was not explicitly rejected");
    auto v1=bytes;
    v1[4]=1;
    require(!deserializeGKRProof(v1,decoded,&error) &&
                error.find("unsupported GKR proof version")!=std::string::npos,
            "v1 GKR proof was not explicitly rejected");
    auto truncated=bytes;
    truncated.pop_back();
    require(!deserializeGKRProof(truncated,decoded,&error) &&
            error.find("truncated")!=std::string::npos,
            "truncated GKR proof was accepted");
}

void testGkrPublicInstanceBinding() {
    layeredCircuit circuit;
    circuit.init(0,2);
    auto &input=circuit.circuit[0];
    input.ty=layerType::INPUT;
    input.size=2;
    input.bit_length=1;
    input.zero_start_id=2;
    auto &output=circuit.circuit[1];
    output.ty=layerType::RELU;
    output.size=2;
    output.bit_length=1;
    output.bit_length_u[0]=1;
    output.size_u[0]=2;
    output.max_bl_u=1;
    output.zero_start_id=2;
    output.uni_gates.emplace_back(0,0,0,1);
    output.uni_gates.emplace_back(1,1,0,1);

    GKRPublicStatement statement;
    statement.model_shape={2,1,1,2,2,4};
    statement.val0_log_size=1;
    statement.val0_generator_domain={"zkGPT/main",2};
    const auto &generators=getGeneratorSet(statement.val0_generator_domain);
    statement.val0_commitment={generators.generators[0]};
    const auto fingerprint=fingerprintCircuit(circuit);
    statement.circuit_fingerprint=fingerprint;
    validateGKRPublicStatement(circuit,statement,fingerprint);

    const auto initialChallenge=[](
        const GKRPublicStatement &value,
        const std::array<std::uint8_t,32> &trusted_fingerprint) {
        auto transcript=initializeGKRTranscript(value,trusted_fingerprint);
        return transcript.challenge("gkr.output.point");
    };
    const Fr baseline=initialChallenge(statement,fingerprint);
    require(initialChallenge(statement,fingerprint)==baseline,
            "same GKR public instance produced different challenges");

    auto rewired=circuit;
    rewired.circuit[1].uni_gates[0].u=1;
    const auto rewired_fingerprint=fingerprintCircuit(rewired);
    require(rewired_fingerprint!=fingerprint,
            "GKR gate/wiring mutation did not change circuit fingerprint");
    auto rewired_statement=statement;
    rewired_statement.circuit_fingerprint=rewired_fingerprint;
    require(initialChallenge(rewired_statement,rewired_fingerprint)!=baseline,
            "GKR gate/wiring mutation did not change initial challenge");
    requireThrows([&] {
        validateGKRPublicStatement(rewired,statement,rewired_fingerprint);
    },"mutated circuit was accepted with the old fingerprint");

    auto changed_shape=statement;
    ++changed_shape.model_shape.mlp_dimension;
    require(initialChallenge(changed_shape,fingerprint)!=baseline,
            "GKR ModelShape mutation did not change initial challenge");

    auto changed_domain=statement;
    changed_domain.val0_generator_domain.domain="zkGPT/range";
    require(initialChallenge(changed_domain,fingerprint)!=baseline,
            "GKR generator domain mutation did not change initial challenge");
    requireThrows([&] {
        validateGKRPublicStatement(circuit,changed_domain,fingerprint);
    },"invalid GKR generator domain was accepted");

    auto changed_count=statement;
    ++changed_count.val0_generator_domain.count;
    require(initialChallenge(changed_count,fingerprint)!=baseline,
            "GKR generator count mutation did not change initial challenge");
    requireThrows([&] {
        validateGKRPublicStatement(circuit,changed_count,fingerprint);
    },"invalid GKR generator count was accepted");

    auto zero_fingerprint=statement;
    zero_fingerprint.circuit_fingerprint.fill(0);
    requireThrows([&] {
        validateGKRPublicStatement(
            circuit,zero_fingerprint,zero_fingerprint.circuit_fingerprint);
    },"all-zero GKR circuit fingerprint was accepted");
}

void testFconnTwoPointAuthentication() {
    // Four 1x4 by 4x4 FCONNs match one public transformer block.  Every
    // matrix maps X=[3,0,0,0] through four identical [1,0,0,0] output rows
    // in the implementation's (output,input) layout, so Y=[3,3,3,3].
    // Consequently both parent MLE
    // evaluations are exactly 3 at every Fiat-Shamir point: this is the
    // executable X=3, W=1, honest-Y=3 regression requested for the old gap.
    gkr_test::SmallFconnFixture fixture;
    auto &p=fixture.proverInstance();
    const auto &statement=fixture.statement();
    const auto &proof=fixture.proof();
    const auto &bytes=fixture.proofBytes();
    const auto &prover_trace=fixture.proverTrace();
    const auto &verifier_trace=fixture.verifierTrace();
    std::string error;
    require(!prover_trace.entries.empty(),"small GKR trace is empty");
    for (const auto &entry : prover_trace.entries) {
        require(entry.transcript_domain==kGKRTranscriptDomain,
                "small GKR trace mixed another Transcript domain");
        require(entry.transcript_instance=="gkr-main",
                "small GKR trace mixed another Transcript instance");
        require(entry.challenge_encoding.size()==32,
                "observed challenge is not canonical BN254 Fr encoding");
    }

    const auto find_trace=[&](const std::string &label_fragment) {
        return std::find_if(
            prover_trace.entries.begin(),prover_trace.entries.end(),
            [&](const ChallengeTraceEntry &entry) {
                return entry.label.find(label_fragment)!=std::string::npos;
            });
    };
    const char *required_labels[]={
        "gkr.output.point", "gkr.phase1.challenge",
        "gkr.phase2.challenge", "gkr.layer.alpha", "gkr.layer.beta",
        "gkr.matrix.u.", "gkr.matrix.v.", "lasso.sigma_u",
        "lasso.sigma_v", "lasso.fconn_v.sigma_u",
        "lasso.fconn_v.sigma_v", "lasso.sumcheck.challenge",
        "ipa-round", "gkr.proof.final"};
    for (const char *label : required_labels)
        require(find_trace(label)!=prover_trace.entries.end(),
                std::string("small GKR trace is missing ")+label);
    const auto matrix_u_trace=find_trace("gkr.matrix.u.");
    const auto matrix_v_trace=find_trace("gkr.matrix.v.");
    require(matrix_u_trace->label!=matrix_v_trace->label,
            "FCONN U/V matrix challenges reused one label");
    require(matrix_u_trace->challenge_encoding!=
                matrix_v_trace->challenge_encoding,
            "FCONN V matrix challenge reused the U challenge value");

    const std::size_t matrix_index=fixture.matrixLayerProofIndex();
    const std::size_t parent_index=fixture.parentLayerProofIndex();
    require(proof.layers[parent_index].final_claim_u1==Fr(3) &&
                proof.layers[parent_index].final_claim_v1==Fr(3),
            "small FCONN fixture did not produce U1=V1=3");
    require(!proof.layers[matrix_index].matrix_v_rounds.empty(),
            "small FCONN proof has no second matrix round");
    std::size_t matrix_u_challenges=0;
    std::size_t matrix_v_challenges=0;
    std::size_t expected_matrix_u_challenges=0;
    std::size_t expected_matrix_v_challenges=0;
    for (const auto &layer : proof.layers) {
        expected_matrix_u_challenges+=layer.matrix_rounds.size();
        expected_matrix_v_challenges+=layer.matrix_v_rounds.size();
    }
    for (const auto &entry : prover_trace.entries) {
        if (entry.label.find("gkr.matrix.u.")!=std::string::npos)
            ++matrix_u_challenges;
        if (entry.label.find("gkr.matrix.v.")!=std::string::npos)
            ++matrix_v_challenges;
    }
    require(matrix_u_challenges==expected_matrix_u_challenges &&
                matrix_v_challenges==expected_matrix_v_challenges,
            "FCONN trace challenge count does not match contraction rounds");

    auto wrong_v=proof;
    wrong_v.layers[parent_index].final_claim_v1=Fr(5);
    requireRejected("FCONN parent U1=3, V1=5", [&] {
        return verifyGKR(p.C,statement,wrong_v,&error);
    },error);

    auto wrong_u=proof;
    wrong_u.layers[parent_index].final_claim_u1=Fr(5);
    requireRejected("FCONN parent U1=5, V1=3", [&] {
        return verifyGKR(p.C,statement,wrong_u,&error);
    },error);

    auto wrong_second_round=proof;
    wrong_second_round.layers[matrix_index].matrix_v_rounds[0].a+=Fr(1);
    requireRejected("modified second FCONN matrix round", [&] {
        return verifyGKR(p.C,statement,wrong_second_round,&error);
    },error,"matrix v sumcheck");

    // Keep p(0)+p(1) unchanged, so the modified polynomial is absorbed and a
    // divergent challenge is actually generated before verification fails.
    auto transcript_divergence=proof;
    transcript_divergence.layers[matrix_index].matrix_v_rounds[0].a+=Fr(1);
    transcript_divergence.layers[matrix_index].matrix_v_rounds[0].b-=Fr(1);
    ChallengeTraceRecorder divergent_trace;
    requireRejected("absorbed second FCONN polynomial divergence", [&] {
        return verifyGKR(
            p.C,statement,transcript_divergence,&error,&divergent_trace);
    },error);
    const std::size_t common_size=std::min(
        verifier_trace.entries.size(),divergent_trace.entries.size());
    std::size_t divergence=0;
    while (divergence<common_size &&
           verifier_trace.entries[divergence].transcript_domain==
               divergent_trace.entries[divergence].transcript_domain &&
           verifier_trace.entries[divergence].transcript_instance==
               divergent_trace.entries[divergence].transcript_instance &&
           verifier_trace.entries[divergence].label==
               divergent_trace.entries[divergence].label &&
           verifier_trace.entries[divergence].counter==
               divergent_trace.entries[divergence].counter &&
           verifier_trace.entries[divergence].pre_state_digest==
               divergent_trace.entries[divergence].pre_state_digest &&
           verifier_trace.entries[divergence].challenge_encoding==
               divergent_trace.entries[divergence].challenge_encoding)
        ++divergence;
    require(divergence<common_size,
            "modified FCONN-V polynomial did not fork challenge trace");
    require(verifier_trace.entries[divergence].label.find("gkr.matrix.v.")!=
                std::string::npos &&
            divergent_trace.entries[divergence].label==
                verifier_trace.entries[divergence].label &&
            divergent_trace.entries[divergence].pre_state_digest!=
                verifier_trace.entries[divergence].pre_state_digest,
            "FCONN-V trace did not fork after polynomial absorption");

    auto wrong_second_evaluation=proof;
    wrong_second_evaluation.layers[matrix_index]
        .matrix_v_final_claim_u0+=Fr(1);
    requireRejected("modified second FCONN final evaluation", [&] {
        return verifyGKR(p.C,statement,wrong_second_evaluation,&error);
    },error,"matrix v final product");

    auto wrong_opening=proof;
    wrong_opening.lasso.input_opening.ipa.final_witness+=Fr(1);
    requireRejected("valid matrix sumchecks with inconsistent opening", [&] {
        return verifyGKR(p.C,statement,wrong_opening,&error);
    },error,"input commitment opening");

    const bool restored_valid=verifyGKR(p.C,statement,proof,&error);
    require(restored_valid,"restored FCONN proof failed: "+error);
    std::cout << "FCONN two-point proof bytes: " << bytes.size()
              << ", second matrix rounds per FCONN: "
              << proof.layers[matrix_index].matrix_v_rounds.size()
              << ", added committed-input evaluations per FCONN: 2"
              << std::endl;
    auto initial_transcript=initializeGKRTranscript(
        statement,statement.circuit_fingerprint);
    const auto initial_transcript_bytes=
        encodeInitialGkrTranscriptForVector(statement);
    const auto expected_initial_transcript_bytes=bytesFromHex(
        "0600000000000000646f6d61696e1b000000000000007a6b4750542d70726f6f"
        "662d76322d636861696e2d736861323536100000000000000070726f746f636f"
        "6c2e76657273696f6e0800000000000000020000000000000008000000000000"
        "0063757276652e69640500000000000000424e3235341300000000000000636972"
        "637569742e66696e6765727072696e742000000000000000064d729d6cefaf7bca"
        "374dba0b46a546e499f5cb086c5bc175a7fdd6f0f4024515000000000000006d"
        "6f64656c2e73657175656e63655f6c656e677468080000000000000001000000"
        "0000000011000000000000006d6f64656c2e6c617965725f636f756e74080000"
        "0000000000010000000000000010000000000000006d6f64656c2e686561645f"
        "636f756e740800000000000000010000000000000014000000000000006d6f64"
        "656c2e686561645f64696d656e73696f6e080000000000000004000000000000"
        "0016000000000000006d6f64656c2e68696464656e5f64696d656e73696f6e08"
        "00000000000000040000000000000013000000000000006d6f64656c2e6d6c70"
        "5f64696d656e73696f6e080000000000000004000000000000000e0000000000"
        "0000696e7075742e6c6f675f73697a6508000000000000000500000000000000"
        "1600000000000000696e7075742e67656e657261746f722e646f6d61696e0a00"
        "0000000000007a6b4750542f6d61696e1500000000000000696e7075742e6765"
        "6e657261746f722e636f756e7408000000000000000800000000000000160000"
        "0000000000696e7075742e636f6d6d69746d656e745f636f756e740800000000"
        "00000004000000000000001000000000000000696e7075742e636f6d6d69746d"
        "656e742000000000000000eea7bbc432103c12a5a04879b949e5ebb42b396281c"
        "7ce74b99a10794f46eb241000000000000000696e7075742e636f6d6d69746d65"
        "6e742000000000000000aa19358c902b89f07659e43865a21c79f636fe487cd6f"
        "e0c82cef8f27ed4810b1000000000000000696e7075742e636f6d6d69746d656e"
        "742000000000000000904354e1ab5ab031bbf0fa95b00b46e1ef1630f1a128de1"
        "37a64d46be3caa1001000000000000000696e7075742e636f6d6d69746d656e74"
        "2000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000");
    require(initial_transcript_bytes==expected_initial_transcript_bytes,
            "canonical initial GKR Transcript bytes changed");
    require(hexBytes(initial_transcript.stateDigest())==
                "16f4572357d3f9b4495b4353f26104829af95de276f1360169b92e770f12b2a5",
            "canonical small-GKR initial transcript digest changed");
    struct ExpectedTrace {
        const char *label;
        std::uint64_t counter;
        const char *pre;
        const char *challenge;
    };
    const ExpectedTrace canonical_entries[]={
        {"gkr.output.point",0,
         "16f4572357d3f9b4495b4353f26104829af95de276f1360169b92e770f12b2a5",
         "d26a303482fb14b634510e43947c1a52969aa00fee2da6f35721dd2f32556b15"},
        {"gkr.output.point",1,
         "ba70cfff7dedcc34a4950479ba94f099b959a70390bf8656e746d14d6370a388",
         "132bec2ad48715a2e005fcbfca4a30427333f1d891386a4e6f5c4b5ecd8a6b1b"},
        {"gkr.phase1.challenge",2,
         "a91c99a4fbb122b997cf5dbd27421b82c5e0495e9598af1382fab3116674dc1c",
         "396d609004597e1cc6ddfc13974094a9c272329b6baba8ce78c4f8ea3a5b0218"},
        {"gkr.matrix.u.7.challenge",8,
         "1650cffdda8141270822002d10ba8ad4200c0c6235bcccf7574aaafc953187da",
         "db379168b02502f5ed37b4e798b887dd15dc160c0f594660d1a1699995258d1c"},
        {"gkr.matrix.v.7.challenge",10,
         "f3dcffb586bf65da601bf4daf068e30f24e3e515679253ef3dc4dc0edf648002",
         "bf97687412fa51d1b4e44a764ac10d7a9f335b3de5dfa6a1300e0d8b5dfc4214"},
        {"lasso.fconn_v.sigma_u",58,
         "b75693f9644b8009952e95324e5d7085fa8f4b2123526b25528e461ae64f71b3",
         "3a03d548217f32b2d05df58567e3c9670c06b9f2b98436fca18139037030f021"},
        {"lasso.fconn_v.sigma_v",59,
         "efe8cc55f2f32066007a444790f6127647845067e2b6a03f5ff15e9c1ba28931",
         "4f5fba8479ed0955388790957e8af7e1f2744b04e628510c668bd3cce1be6404"}
    };
    for (const auto &expected : canonical_entries) {
        const auto found=std::find_if(
            prover_trace.entries.begin(),prover_trace.entries.end(),
            [&](const ChallengeTraceEntry &entry) {
                return entry.label==expected.label &&
                       entry.counter==expected.counter;
            });
        require(found!=prover_trace.entries.end() &&
                    hexBytes(found->pre_state_digest)==expected.pre &&
                    hexBytes(found->challenge_encoding)==expected.challenge,
                std::string("canonical challenge vector changed at ")+
                    expected.label);
    }
    require(hexBytes(prover_trace.entries.back().post_state_digest)==
                "3b08a8baaf3c80c4089a97af7ddeb83c2e48e633007e7dea545db428856aefd2",
            "canonical small-GKR final transcript digest changed");
    require(hexBytes(sha256Bytes(bytes))==
                "d237039f950017bdd524f34f412690d9672a018ddc04585507400a7020f97707",
            "canonical GKR proof v3 SHA-256 changed");
    const char *canonical_post_digests[]={
        "ba70cfff7dedcc34a4950479ba94f099b959a70390bf8656e746d14d6370a388",
        "8d13ff3bb867ac5b1b0ddefe478680d9117542bfb76a8818c3d1e51535edf195",
        "c962c273f2d852807bdc5ce8da9b477dfc940499463f4ffc286ba6f4f06ab652",
        "a4eeb6c2c38c7525ca83a6f1d393157dc7ce2f302d7cc2ec7ac71f6dc8c8db31",
        "8fcd455a06f947ae155596f0efc006bff7375fe6b76973f11569574c891b23cc",
        "c7778a9b35883cc8114100f1e10834dafa176cc01acc1dd423ea34da0b908c87",
        "020487ea0abf5f94eed141eb9c96ab04e98a3dbcdf10e692a7da70144cdc2484",
        "a09fefa4cfe2512f1168260d8fac69748ac7fa8436bfaca8812965c26d27dfa9",
        "264d5a9a2a6e4a563e6269944b302f1a0dffcc5ba554ad695137901ba0079a93",
        "1c1ed91d270897fd95376376ad22d56569b7fe559b1c43b20c1d0f30bc9e5d53",
        "58ce4b1e5cbf6af2fbfd4ccbdeca424ac431075b0d902d6f8852ed3f62150649",
        "90aa149763977855fa61c994e74f102830590390a31870e5814c1121866e042d",
        "239517617263fab2294fedfdd2c3cbdc5d175fd67c027f6357f1ad6c0ef60461",
        "0b380e7f939bbb93ffebb76a36e3658495be45343282f67145e550c9fd1ec24a",
        "254db4ab75bd1ebb463524420defcc0c4edfb6bcde3688a79207be24b2a62e76",
        "2a39965b280941bbe43de0826b604d4616cb4060d473a2be38cf0c01b25dadd3",
        "cd73c06092ec9221d98dab544de070db563132c1dc29a3f00a3e0cfa9d0f95d5",
        "916e7d0914399ee98aac7c0908df16b528f6cb9d302b22c87e2d1fdcb906a429",
        "88363021a2e09145f768f09c3add6b5c7fcc39482c4851326533050525b36929",
        "44516291c4f06dd3d9fce2dcff54146eaefdf25d5c184a9ed5bc291b66aa9bfe",
        "7c5a3e92f4e43b77293c665627105356c7b8615f3070c5fd6c2338f5f21f0008",
        "8d26ff2530f4cd20ef81128d2e21a19e6b41f5d0eac58ac5fe2890cef07fbd85",
        "3be9369c709a56bd56272081c85bf5c8139f9a9207aba19f5dfcf5f81313b071",
        "ae9d5ef31c1cec1d0c53d9fb70860ea9eef76cca4985caa4ad6544e2e51489e9",
        "c775bc88f4a9ad1a1d4b42e895b4c0df8ab4ee9696ddb7041d2fc7aa16ac3a88",
        "e8afcc2e032c2362e5d7e3cd5005f08288de12a9d2b4cfdb54a45c97ea03bf08",
        "bc34a4aa7e3a4c95213188b347458c123c1c8c6d23449f3d9d5888ae47772a41",
        "c370edbce6b000e22bf3146b8128602f717f123528a8c13cdb22bef76a702434",
        "784e0217970764ee1300bd887ec244de7f430493f80474005841aa9fc45dfd75",
        "c5c41674eb7545c4cafd66827842f3fff22f1fa053fb3549600c25f473f2cd73",
        "9f34adaf24a9122c1479990363fdfde8fbe0e1ee87eda757a6bc255df0a8034a",
        "60ef43817dbd179c11867d27a2ca68f37d7d6811711e33f2dcce0a2412d98695",
        "868e89de70234e9b6e406f96fc0f98288eb9381a4668127ddcf48ede91a53e89",
        "199d4730fd5df1da30e164dc0ef891ff9488ee85408a2644e0df011650f9e7d4",
        "9a5499103d14012042899179a7690b5890c2717da046000224fd572f0e6ce609",
        "30d8bf44ec7bccd9b4739b6e9cf7a9c1227d486d48fc46cf3dfb2538d6800ee9",
        "62385aa712738bcc3b9353a2b2196675ee77d7e333abd60a3f2e4ca6ddcad1d9",
        "c7db024e2de22ace1644a51c459272db0c716e6136aa0c210bc0a725df8a5390",
        "5d99c6ca06f2012ffa2246c7d0829e902322eb81a7145c23aa4b876d2a4a58e2",
        "766b2bcbcd9677b464f0cfbdfdbf54bfd72660e36ea15b44682682bcccacd238",
        "d1929040620066bcfa21a0464b3e98c251f2ae53d4b69ca34bd45b8b5331e5e8",
        "efe906c3f7002168c57484f54816e31aefe317ed52eb06ed4f073693bc61b3cc",
        "f2fc61e2d51d8ec117ad64d450364232d7e9c88af9c0d4ab368f1ead9adeea53",
        "0f1fb2542a30b2b5fc58c402eb49b611b04a1a2970791996ab9bcf77a036b135",
        "a1becc467e66c190caed5436a2f514fd4b7e03c2a4f74c8b0a5f2b2f03371026",
        "3c871836aa7d660c6a4280bb6b75fd9be26011e6e1d300b94fd66f0509333c05",
        "ab472baacfdebf2cb8fee37648fa14391bc473cf02c2c4c0134ea20cc03bead9",
        "d4b4231e16f72cf226194d4346980a9dbcf6aa4ed358ae0e6869d69fb1eafbc7",
        "5c7aaf38e790fab6091ac0707f71da9cb8e3dbb49d53c209dc0bdc027c499a42",
        "da18c82287ada76bf9372f888156e3c82ba32d4ef8ad972c39485d14a0c87e72",
        "ade726313153f8603dca06d90666a220f169fe0d783f7787443b9ce8a7a1e467",
        "d005f245a4a5c8041af58d2872a8d9f31b32a9c19f4f813aef33c89475851393",
        "e3b7a7e036b1d2d8da6ef9f7ee4d8e09b55ad3834825cfaf3c0f1acdc609253f",
        "a89f6b6566ef85bbe856916c9b96c0414ac875ccc0e86b4c709d62c4034d1f4f",
        "45bf36191ffbf942a1daac4247541078896b0036db95f1c917883900056a51dc",
        "172a5ef757547bc51c18e7c41ab80882a4a4a14a31933e13fb3df99ad19a91ac",
        "d15c23b692105e456f44b7857f1969a15b8aa0f6a9abe4c74222465df2f479b5",
        "b75693f9644b8009952e95324e5d7085fa8f4b2123526b25528e461ae64f71b3",
        "efe8cc55f2f32066007a444790f6127647845067e2b6a03f5ff15e9c1ba28931",
        "43c88be154916996ceb8f82d5b66f81d76540ade771a3f203280fdfd65a5953b",
        "50c2a1c56971d9a9586b569df80753234bc85935bb0c4b228b5541b24d7bf9ed",
        "664988c080d60c66c86064bd554fad023f172a86e38a78ec3b0b26ff4390eb33",
        "f4e0230f370d294390c6cd55a06f58f5e12a0e6cf06c439e9a45693efdae59b6",
        "d57142de32c46f5ae3b45fc62672a03fdd8fbd832eecd058150dceae7db0f175",
        "ba744d6ac71aa2530afb0413ef6f6f02e9e3abe6c9562c97f0d079d44b9d5a61",
        "747bf311bb13f9d79f2206b3cb7463f175b89f9d9b768c3b75572d345ee8f0e6",
        "10690bc4f80c6fcb5de10064cf8b861d42cc4df8096a3e3e622fd3a186bb1870",
        "5b535b77b898ef38baceb035e5deb455162a6b7bcca6b81382c42ffccda360c1",
        "97f97d0c096db8ecd4da41bc78d5db9f31249bd12cb4fbf84102673c03226fcc",
        "8b635cacb513b9bb0e8683bc1aed309ee8c7a8a3d0cac67a38f2d37dcd143b3f",
        "7dc4ae72a86ccb301f41c20583443044dd88eb475bde47cf3e1ed4da860bf050",
        "3a55875eaee0f159931edb5ee149778f53d7215024aa853880bfa205424afc7c",
        "1c1cc5b28c2dc9d693c49b4bf7a98762ca0a754fae582b11301555ab067c9b07",
        "76241e1d63fa89f1558708fb4a45ef3176f917693ff8386058036eaccae325ff",
        "3b08a8baaf3c80c4089a97af7ddeb83c2e48e633007e7dea545db428856aefd2"
    };
    require(prover_trace.entries.size()==
                sizeof(canonical_post_digests)/sizeof(canonical_post_digests[0]),
            "canonical GKR chaining digest count changed");
    for (std::size_t i=0;i<prover_trace.entries.size();++i)
        require(hexBytes(prover_trace.entries[i].post_state_digest)==
                    canonical_post_digests[i],
                "canonical GKR post-challenge digest changed at counter "+
                    std::to_string(i));
    std::cout << "Small GKR challenge trace entries: "
              << prover_trace.entries.size() << std::endl;
    std::cout << "TRACE_VECTOR circuit_fingerprint="
              << circuitFingerprintHex(statement.circuit_fingerprint)
              << std::endl;
    for (std::size_t i=0;i<statement.val0_commitment.size();++i) {
        std::vector<std::uint8_t> encoded(G1::getSerializedByteSize());
        require(statement.val0_commitment[i].serialize(
                    encoded.data(),encoded.size())==encoded.size(),
                "failed to encode canonical trace-vector commitment");
        std::cout << "TRACE_VECTOR commitment[" << i << "]="
                  << hexBytes(encoded) << std::endl;
    }
    std::cout << "TRACE_VECTOR initial_digest="
              << hexBytes(initial_transcript.stateDigest()) << std::endl;
    for (std::size_t i=0;i<std::min<std::size_t>(3,prover_trace.entries.size());
         ++i) {
        const auto &entry=prover_trace.entries[i];
        std::cout << "TRACE_VECTOR first[" << i << "] label=" << entry.label
                  << " counter=" << entry.counter
                  << " pre=" << hexBytes(entry.pre_state_digest)
                  << " challenge=" << hexBytes(entry.challenge_encoding)
                  << std::endl;
    }
    for (const auto *entry : {&*matrix_u_trace,&*matrix_v_trace,
                              &*find_trace("lasso.fconn_v.sigma_u"),
                              &*find_trace("lasso.fconn_v.sigma_v")})
        std::cout << "TRACE_VECTOR selected label=" << entry->label
                  << " counter=" << entry->counter
                  << " pre=" << hexBytes(entry->pre_state_digest)
                  << " challenge=" << hexBytes(entry->challenge_encoding)
                  << std::endl;
    std::cout << "TRACE_VECTOR final_digest="
              << hexBytes(prover_trace.entries.back().post_state_digest)
              << std::endl;

}

void testZkGPTArtifactSerialization() {
    require(kZkGPTProtocolVersion==2,
            "canonical zkGPT protocol version changed");
    require(kGKRProofEncodingVersion==3,
            "canonical GKR proof encoding version changed");
    require(kZkGPTArtifactEncodingVersion==2,
            "canonical ZKGS/ZKPF encoding version changed");
    require(kRangeStatementArtifactVersion==1 &&
                kRangeProofArtifactVersion==2,
            "canonical Range artifact versions changed");
    require(std::string(kGKRTranscriptDomain)==
                "zkGPT-proof-v2-chain-sha256",
            "canonical GKR Transcript domain changed");
    require(std::string(kZkGPTArtifactTranscriptDomain)==
                "zkGPT-artifact-v2",
            "canonical Artifact Transcript domain changed");
    ZkGPTPublicStatement statement;
    statement.model_shape={32,12,12,64,768,3072};
    statement.circuit_fingerprint[0]=0x42;
    statement.output_claim.mle_evaluation=Fr(19);
    statement.range_statement.shape=statement.model_shape;
    statement.range_statement.val0_log_size=2;
    statement.range_statement.val0_generator_domain={"zkGPT/main",2};
    statement.range_statement.range_generator_domain={"zkGPT/range",2};

    ZkGPTProof proof;
    proof.gkr_proof.output_evaluation=Fr(19);
    // Keep the serialization-only fixture deterministic across build modes.
    // These empty-Lasso endpoint fields are still part of canonical ZKGP
    // bytes even though this synthetic fixture does not contain a Lasso
    // proof, and mcl::Fr's default constructor does not promise zero.
    proof.gkr_proof.lasso.input_evaluation=Fr(0);
    proof.gkr_proof.lasso.mapping_evaluation=Fr(0);
    proof.gkr_proof.lasso.input_opening.ipa.final_witness=Fr(0);
    proof.gkr_proof.transcript_binding=Fr(20);
    proof.range_proof.transcript_binding=Fr(21);
    bindZkGPTProof(statement,proof);

    const auto statement_bytes=serializeZkGPTPublicStatement(statement);
    const auto proof_bytes=serializeZkGPTProof(proof);
    std::array<std::uint8_t,64> binding_bytes{};
    const auto binding_size=proof.transcript_binding.serialize(
        binding_bytes.data(),binding_bytes.size());
    require(binding_size!=0,"cannot serialize synthetic artifact binding");
    const auto statement_digest=hexBytes(sha256Bytes(statement_bytes));
    const auto proof_digest=hexBytes(sha256Bytes(proof_bytes));
    require(statement_digest==
                "c0c8d6b91ce76dc473f7eb61a94a59081182734bce69feec06a89db2ec242a8c",
            "synthetic ZKGS canonical SHA-256 changed: "+statement_digest);
    require(proof_digest==
                "b8d53a19576b04f6ac879a5b7e9539b4e92a55db2d87b2e9f058423632ee5bb7",
            "synthetic ZKPF canonical SHA-256 changed: "+proof_digest);
    const auto binding_hex=hexBytes(std::vector<std::uint8_t>(
        binding_bytes.begin(),binding_bytes.begin()+binding_size));
    require(binding_hex==
                "619960f6e9d542df95d7e91ebb2e12136d9b2b87feff3ce7bf10074cb0911b10",
            "synthetic Artifact binding Fr changed: "+binding_hex);
    ZkGPTPublicStatement decoded_statement;
    ZkGPTProof decoded_proof;
    std::string error;
    require(deserializeZkGPTPublicStatement(
                statement_bytes,decoded_statement,&error),
            "zkGPT statement round-trip failed: "+error);
    require(deserializeZkGPTProof(proof_bytes,decoded_proof,&error),
            "zkGPT proof round-trip failed: "+error);
    require(serializeZkGPTPublicStatement(decoded_statement)==statement_bytes,
            "zkGPT statement canonical round-trip changed bytes");
    require(serializeZkGPTProof(decoded_proof)==proof_bytes,
            "zkGPT proof canonical round-trip changed bytes");
    error.clear();
    require(!verifyZkGPT(decoded_statement,decoded_proof,&error) &&
                error.find("Range verification failed")!=std::string::npos,
            "serialization-only synthetic Artifact unexpectedly passed "
            "production verification: "+error);
    require(statement_bytes.size()>=12 && statement_bytes[4]==2 &&
                statement_bytes[8]==2 && proof_bytes.size()>=8 &&
                proof_bytes[4]==2,
            "zkGPT artifact does not carry v2 encoding/protocol versions");

    auto v1_statement_envelope=statement_bytes;
    v1_statement_envelope[4]=1;
    require(!deserializeZkGPTPublicStatement(
                v1_statement_envelope,decoded_statement,&error) &&
                error.find("unsupported zkGPT statement version")!=
                    std::string::npos,
            "v1 zkGPT statement envelope was not explicitly rejected");
    auto v1_statement_protocol=statement_bytes;
    v1_statement_protocol[8]=1;
    require(!deserializeZkGPTPublicStatement(
                v1_statement_protocol,decoded_statement,&error) &&
                error.find("unsupported zkGPT protocol version")!=
                    std::string::npos,
            "v1 zkGPT statement protocol was not explicitly rejected");
    auto v1_proof=proof_bytes;
    v1_proof[4]=1;
    require(!deserializeZkGPTProof(v1_proof,decoded_proof,&error) &&
                error.find("unsupported zkGPT proof version")!=
                    std::string::npos,
            "v1 zkGPT proof was not explicitly rejected");
    auto v1_statement=statement;
    v1_statement.protocol_version=1;
    requireThrows([&] { bindZkGPTProof(v1_statement,proof); },
                  "v1 zkGPT statement was accepted by the proof binder");
    require(!verifyZkGPT(v1_statement,proof,&error) &&
                error.find("unsupported zkGPT protocol version")!=
                    std::string::npos,
            "v1 zkGPT statement was accepted by the production verifier");

    auto truncated=proof_bytes;
    truncated.pop_back();
    require(!deserializeZkGPTProof(truncated,decoded_proof,&error) &&
            error.find("truncated")!=std::string::npos,
            "truncated zkGPT proof was accepted");
    auto trailing=statement_bytes;
    trailing.push_back(0);
    require(!deserializeZkGPTPublicStatement(
                trailing,decoded_statement,&error) &&
            error.find("trailing bytes")!=std::string::npos,
            "zkGPT statement trailing bytes were accepted");

    auto oversized=proof_bytes;
    oversized[8]=0xff;
    oversized[9]=0xff;
    oversized[10]=0xff;
    oversized[11]=0x7f;
    require(!deserializeZkGPTProof(oversized,decoded_proof,&error) &&
            error.find("exceeds limit")!=std::string::npos,
            "oversized nested zkGPT proof length was accepted");
}

std::vector<G1> classicIntegerCommitments(
    const std::vector<ll> &values, const std::vector<G1> &generators) {
    std::size_t log_size=0;
    for (std::size_t size=values.size();size>1;size>>=1) ++log_size;
    const std::size_t rows=static_cast<std::size_t>(1)<<(log_size/2);
    const std::size_t columns=values.size()/rows;
    require(rows*columns==values.size() && generators.size()>=columns,
            "invalid classic integer commitment fixture");
    std::vector<G1> commitments(rows);
    std::vector<Fr> scalars(columns);
    for (std::size_t row=0;row<rows;++row) {
        for (std::size_t column=0;column<columns;++column)
            scalars[column]=Fr(values[row+column*rows]);
        G1::mulVec(commitments[row],
                   const_cast<G1 *>(generators.data()),scalars.data(),columns);
    }
    return commitments;
}

void checkIntegerCommitmentCase(
    const std::string &name, const std::vector<ll> &values,
    const std::vector<G1> &generators) {
    const auto expected=classicIntegerCommitments(values,generators);
    for (const int threads : {1,8,32}) {
        const auto actual=range_logup::commitIntegerValues(
            values,generators,threads);
        require(actual==expected,name+" commitment mismatch with "+
                std::to_string(threads)+" threads");
    }
}

void testLogUpIntegerCommitKernel() {
    constexpr std::size_t kLogSize=16;
    constexpr std::size_t kValueCount=static_cast<std::size_t>(1)<<kLogSize;
    constexpr std::size_t kRows=static_cast<std::size_t>(1)<<(kLogSize/2);
    constexpr std::size_t kColumns=kValueCount/kRows;
    std::vector<G1> generators(kColumns);
    range_proof_gen_gi(generators.data(),static_cast<int>(generators.size()));

    std::vector<ll> values(kValueCount,0);
    checkIntegerCommitmentCase("all-zero",values,generators);
    std::fill(values.begin(),values.end(),1);
    checkIntegerCommitmentCase("all-one",values,generators);
    std::fill(values.begin(),values.end(),511);
    checkIntegerCommitmentCase("all-511",values,generators);

    for (std::size_t i=0;i<values.size();++i)
        values[i]=static_cast<ll>((i*73+(i>>8)*19)&511);
    checkIntegerCommitmentCase("deterministic-9-bit",values,generators);

    std::fill(values.begin(),values.end(),0);
    for (std::size_t i=0;i<values.size();i+=31) values[i]=511;
    checkIntegerCommitmentCase("mostly-zero",values,generators);

    for (std::size_t column=0;column<kColumns;++column)
        for (std::size_t row=0;row<kRows;++row)
            values[row+column*kRows]=static_cast<ll>(column);
    checkIntegerCommitmentCase("one-of-each-scalar",values,generators);

    // These 512-entry cases take the ordinary-MSM fallback and ensure public
    // tables and a large multiplicity no longer reach the legacy 2^18-bucket
    // scratch path.
    std::vector<ll> table(512);
    for (std::size_t i=0;i<table.size();++i) table[i]=static_cast<ll>(i);
    checkIntegerCommitmentCase("public-table",table,generators);
    std::vector<ll> multiplicity(512,0);
    multiplicity[0]=static_cast<ll>(kValueCount);
    checkIntegerCommitmentCase("large-multiplicity",multiplicity,generators);

    auto negative=table;
    negative[0]=-1;
    requireThrows([&] {
        range_logup::commitIntegerValues(negative,generators,8);
    },"negative LogUp commitment scalar was accepted");
    std::vector<G1> short_generators(31);
    requireThrows([&] {
        range_logup::commitIntegerValues(table,short_generators,8);
    },"short LogUp generator vector was accepted");
}

}  // namespace

void testRangeLayoutBinding() {
    WitnessRegistry registry;
    registry.setShape({32, 12, 12, 64, 768, 3072});
    registry.addRange(WitnessKind::ROUNDING, "a", 0, 3, 64, true);
    registry.addRange(WitnessKind::GELU, "b", 3, 5, 32, false);
    registry.addRange(WitnessKind::SOFTMAX, "c", 8, 2, 64, true);
    const auto plan=planRangeLayout(registry);
    if (plan.queries.size()!=2 || plan.queries[0].actual_query_size!=5 ||
        plan.queries[0].padded_query_size!=8 ||
        plan.regions[2].proof_constraint_index!=0 ||
        plan.regions[2].proof_start!=3)
        throw std::logic_error("unexpected Range layout plan");
    RangePublicStatement statement;
    statement.shape=registry.shape();
    statement.regions=plan.regions;
    statement.queries=plan.queries;
    requireRangeLayoutMatchesRegistry(statement, registry);
    auto expectReject=[&](RangePublicStatement mutated, const char *what) {
        try {
            requireRangeLayoutMatchesRegistry(mutated, registry);
        } catch (const std::runtime_error &) {
            return;
        }
        throw std::logic_error(std::string("Range layout accepted: ")+what);
    };
    auto dropped=statement;
    dropped.regions.pop_back();
    expectReject(dropped, "dropped region");
    auto widened=statement;
    widened.regions[1].bits=33;
    expectReject(widened, "widened region");
    auto shrunk=statement;
    shrunk.queries[0].actual_query_size=3;
    expectReject(shrunk, "shrunk query");
}

int main(int argc, char **argv) {
    initPairing(mcl::BN254);
    testWideEncodingAndChunks();
    std::cout << "Range wide integer boundary tests passed" << std::endl;
    testBuildFromWitnessWidePath();
    std::cout << "Range buildFromWitness wide-path tests passed" << std::endl;
    testSumcheckConsistency();
    std::cout << "Range sumcheck consistency tests passed" << std::endl;
    testSerializableSumchecks();
    std::cout << "Serializable Range sumcheck tests passed" << std::endl;
    testRowMajorMleOpening();
    std::cout << "Row-major Hyrax MLE opening test passed" << std::endl;
    testCommittedWitnessCapacityLifecycle();
    std::cout << "Committed witness capacity/lifecycle tests passed" << std::endl;
    testGkrProofSerialization();
    std::cout << "GKR proof serialization test passed" << std::endl;
    testGkrPublicInstanceBinding();
    std::cout << "GKR public-instance binding test passed" << std::endl;
    testFconnTwoPointAuthentication();
    std::cout << "FCONN two-point authentication tests passed" << std::endl;
    testZkGPTArtifactSerialization();
    std::cout << "zkGPT artifact serialization test passed" << std::endl;
    testRangeLayoutBinding();
    std::cout << "Range layout binding test passed" << std::endl;
    bool prove_wide=false;
    bool commit_kernel=false;
    std::string artifact_prefix;
    for (int i=1;i<argc;++i) {
        const std::string argument=argv[i];
        if (argument=="--prove-wide") prove_wide=true;
        else if (argument=="--commit-kernel") commit_kernel=true;
        else if (argument=="--artifact-prefix" && i+1<argc)
            artifact_prefix=argv[++i];
        else throw std::invalid_argument("unknown range test argument: "+argument);
    }
    if (commit_kernel) {
        testLogUpIntegerCommitKernel();
        std::cout << "LogUp integer commitment kernel tests passed"
                  << std::endl;
    }
    if (prove_wide) {
        proveWideRangeKernel(artifact_prefix);
        testMixedSignedStageB();
        testRadixTailWidthStageB();
        testLogUpReplayStageB();
    }
    return 0;
}
