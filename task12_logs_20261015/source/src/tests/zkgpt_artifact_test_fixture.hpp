#pragma once

#include "models.hpp"
#include "zkgpt_serialization.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace gkr_test {

// Test-only owner for a one-block, GPT-compatible zkGPT artifact.  Unlike the
// serialization-only fixture in main_range_tests, this object builds Range and
// GKR proofs over the same committed val[0] witness and requires the unchanged
// production verifyZkGPT implementation to accept the canonical round-trip.
class SmallZkGPTArtifactFixture {
public:
    SmallZkGPTArtifactFixture();
    ~SmallZkGPTArtifactFixture();

    SmallZkGPTArtifactFixture(const SmallZkGPTArtifactFixture &) = delete;
    SmallZkGPTArtifactFixture &operator=(
        const SmallZkGPTArtifactFixture &) = delete;

    const ZkGPTPublicStatement &statement() const { return statement_; }
    const ZkGPTProof &proof() const { return proof_; }
    const std::vector<std::uint8_t> &statementBytes() const {
        return statement_bytes_;
    }
    const std::vector<std::uint8_t> &proofBytes() const {
        return proof_bytes_;
    }

    void writeArtifact(const std::string &prefix) const;

private:
    static constexpr unsigned kDepth=1;
    ZkGPTPublicStatement statement_;
    ZkGPTProof proof_;
    std::vector<std::uint8_t> statement_bytes_;
    std::vector<std::uint8_t> proof_bytes_;
};

}  // namespace gkr_test
