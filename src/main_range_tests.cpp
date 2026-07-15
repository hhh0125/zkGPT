#include "range_prover.hpp"
#include "range_wide.hpp"

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
    const G1 val0_commitments[]={prover.g[0], prover.g[1]};
    const auto statement=prover.makePublicStatement(3, val0_commitments, 2);
    const auto proof=prover.proveStageB(statement);

    range_verifier verifier;
    std::string error;
    require(verifier.verifyReconstruction(statement, proof, &error),
            "honest reconstruction proof failed: "+error);
    require(!verifier.verify(statement, proof, &error),
            "full Stage B verification succeeded without opening proofs");
    require(error.find("commitment opening proofs")!=std::string::npos,
            "full Stage B verification failed for the wrong reason");

    auto bad_chunk_commitment=proof;
    bad_chunk_commitment.chunk_commitments[0][0][0]+=prover.GG;
    require(!verifier.verifyReconstruction(
                statement, bad_chunk_commitment, &error),
            "modified chunk commitment was accepted");

    auto bad_sumcheck=proof;
    bad_sumcheck.reconstruction_proofs[0].rounds[0].sum0+=Fr(1);
    require(!verifier.verifyReconstruction(statement, bad_sumcheck, &error),
            "modified reconstruction sumcheck round was accepted");

    auto bad_evaluation=proof;
    bad_evaluation.reconstruction_proofs[0].chunk_evaluations[0]+=Fr(1);
    require(!verifier.verifyReconstruction(statement, bad_evaluation, &error),
            "modified reconstruction evaluation was accepted");

    auto bad_commitment_statement=statement;
    bad_commitment_statement.val0_commitment[0]+=prover.GG;
    require(!verifier.verifyReconstruction(
                bad_commitment_statement, proof, &error),
            "modified val[0] commitment was accepted");

    auto bad_region_statement=statement;
    ++bad_region_statement.regions[0].bits;
    require(!verifier.verifyReconstruction(bad_region_statement, proof, &error),
            "modified range region metadata was accepted");

    auto bad_transcript=proof;
    bad_transcript.transcript_binding+=Fr(1);
    require(!verifier.verifyReconstruction(statement, bad_transcript, &error),
            "modified Fiat-Shamir transcript binding was accepted");

    std::cout << "126-bit Range Proof kernel test passed" << std::endl;
    std::cout << "Stage B reconstruction negative tests passed" << std::endl;
    std::cout << "Stage B commitment opening binding remains incomplete"
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

}  // namespace

int main(int argc, char **argv) {
    initPairing(mcl::BN254);
    testWideEncodingAndChunks();
    std::cout << "Range wide integer boundary tests passed" << std::endl;
    testBuildFromWitnessWidePath();
    std::cout << "Range buildFromWitness wide-path tests passed" << std::endl;
    testSumcheckConsistency();
    std::cout << "Range sumcheck consistency tests passed" << std::endl;
    if (argc==2 && std::string(argv[1])=="--prove-wide")
        proveWideRangeKernel();
    return 0;
}
