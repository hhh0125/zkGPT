#pragma once

#include "gkr_proof_builder.hpp"
#include "gkr_serialization.hpp"
#include "gkr_verifier.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace gkr_test {

// Test-only owner for the deterministic four-FCONN circuit used by the
// challenge-trace and proof-mutation suites.  It builds one observer-off and
// one observer-on proof, checks their equality, round-trips proof v3, and
// verifies the baseline with the production verifyGKR implementation.
class SmallFconnFixture {
public:
    explicit SmallFconnFixture(bool input_ports=false);
    ~SmallFconnFixture();

    SmallFconnFixture(const SmallFconnFixture &) = delete;
    SmallFconnFixture &operator=(const SmallFconnFixture &) = delete;

    prover &proverInstance() { return prover_; }
    const layeredCircuit &circuit() const { return prover_.C; }
    const GKRPublicStatement &statement() const { return statement_; }
    const GKRProof &proof() const { return proof_; }
    const std::vector<std::uint8_t> &proofBytes() const { return proof_bytes_; }
    const ChallengeTraceRecorder &proverTrace() const { return prover_trace_; }
    const ChallengeTraceRecorder &verifierTrace() const {
        return verifier_trace_;
    }
    std::size_t matrixLayerProofIndex() const { return matrix_index_; }
    std::size_t parentLayerProofIndex() const { return parent_index_; }

private:
    void cleanup() noexcept;

    static constexpr unsigned kFconnCount=4;
    prover prover_;
    GKRPublicStatement statement_;
    GKRProof proof_;
    std::vector<std::uint8_t> proof_bytes_;
    ChallengeTraceRecorder prover_trace_;
    ChallengeTraceRecorder verifier_trace_;
    std::size_t matrix_index_=0;
    std::size_t parent_index_=0;
};

}  // namespace gkr_test
