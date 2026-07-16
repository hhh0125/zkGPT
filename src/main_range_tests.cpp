#include "range_prover.hpp"
#include "range_wide.hpp"
#include "hyrax.hpp"
#include "range_sumcheck.hpp"

#include <chrono>
#include <iostream>
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

void proveWideRangeKernel() {
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
    const auto proof=prover.proveStageB(statement);

    range_verifier verifier;
    std::string error;
    require(verifier.verifyReconstruction(statement, proof, &error),
            "honest reconstruction proof failed: "+error);
    const auto verify_start=std::chrono::steady_clock::now();
    require(verifier.verify(statement, proof, &error),
            "full Stage B verification failed: "+error);
    const double verifier_seconds=std::chrono::duration<double>(
        std::chrono::steady_clock::now()-verify_start).count();
    std::cout << "Stage B metrics: proof_bytes="
              << estimateRangeProofSizeBytes(proof)
              << ", prover_seconds=" << proof.totalProverTime()
              << ", verifier_seconds=" << verifier_seconds << std::endl;

    std::cout << "Commitment opening soundness tests:" << std::endl;

    auto bad_chunk_commitment=proof;
    bad_chunk_commitment.chunk_commitments[0][0][0]+=prover.GG;
    requireRejected("modified chunk commitment", [&] {
        return verifier.verify(statement, bad_chunk_commitment, &error);
    }, error, "LogUp value commitment mismatch");

    auto bad_sumcheck=proof;
    bad_sumcheck.reconstruction_proofs[0].rounds[0].sum0+=Fr(1);
    requireRejected("modified reconstruction sumcheck round", [&] {
        return verifier.verifyReconstruction(statement, bad_sumcheck, &error);
    }, error, "reconstruction sumcheck round failed");

    auto bad_chunk_opening=proof;
    bad_chunk_opening.chunk_openings[0][0].opening.ipa.final_witness+=Fr(1);
    requireRejected("modified chunk MLE opening", [&] {
        return verifier.verifyReconstruction(statement, bad_chunk_opening,
                                             &error);
    }, error, "chunk MLE opening failed");

    auto bad_sparse_opening=proof;
    bad_sparse_opening.val0_openings[0].patterns[0].opening.final_witness+=Fr(1);
    requireRejected("modified val[0] sparse opening", [&] {
        return verifier.verifyReconstruction(statement, bad_sparse_opening,
                                             &error);
    }, error, "val[0] sparse opening failed");

    auto bad_evaluation=proof;
    bad_evaluation.reconstruction_proofs[0].chunk_evaluations[0]+=Fr(1);
    requireRejected("modified chunk evaluation", [&] {
        return verifier.verifyReconstruction(statement, bad_evaluation, &error);
    }, error, "reconstruction opening evaluations disagree");

    auto bad_encoded_evaluation=proof;
    bad_encoded_evaluation.reconstruction_proofs[0].encoded_evaluation+=Fr(1);
    requireRejected("modified encoded evaluation", [&] {
        return verifier.verifyReconstruction(statement, bad_encoded_evaluation,
                                             &error);
    }, error, "reconstruction opening evaluations disagree");

    auto equal_sum_different_evaluations=proof;
    Fr inverse_512;
    Fr::inv(inverse_512, Fr(512));
    equal_sum_different_evaluations.reconstruction_proofs[0]
        .chunk_evaluations[0]+=Fr(1);
    equal_sum_different_evaluations.reconstruction_proofs[0]
        .chunk_evaluations[1]-=inverse_512;
    requireRejected("equal-sum different chunk evaluations", [&] {
        return verifier.verifyReconstruction(
            statement, equal_sum_different_evaluations, &error);
    }, error, "chunk opening metadata mismatch");

    auto bad_commitment_statement=statement;
    bad_commitment_statement.val0_commitment[0]+=prover.GG;
    requireRejected("modified val[0] commitment", [&] {
        return verifier.verifyReconstruction(bad_commitment_statement, proof,
                                             &error);
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
        return verifier.verifyReconstruction(bad_region_statement, proof,
                                             &error);
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
        return verifier.verifyReconstruction(statement, bad_transcript, &error);
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
    const auto proof=prover.proveStageB(statement);
    range_verifier verifier;
    std::string error;
    require(verifier.verify(statement, proof, &error),
            "mixed signed/unsigned Stage B proof failed: "+error);
    require(!proof.val0_openings[0].signed_bias_evaluation.isZero(),
            "mixed signed query produced a zero signed bias");

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
    std::cout << "Mixed signed/unsigned/padding Stage B test passed"
              << std::endl;
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

}  // namespace

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
    if (argc==2 && std::string(argv[1])=="--prove-wide")
    {
        proveWideRangeKernel();
        testMixedSignedStageB();
    }
    return 0;
}
