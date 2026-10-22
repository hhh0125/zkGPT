#pragma once
#include "gkr.hpp"
namespace zkgpt::r1 {
// Foundation composition, not the unresolved GPT-2 implementation profile.
// The verifier's externally selected statement fixes W, X, Y, circuit and table.
struct BundleStatement {
    InputSpec input;
    GkrSpec gkr;
    RangeSpec range;
    MemorySpec indexed;
    void validate() const;
};
struct BundleProof {
    RangeProof range;
    OpenProof indexed_value;
    MemoryProof memory;
    GkrProof gkr;
    InputProof input;
};
BundleProof proveBundle(const BundleStatement &s, const std::array<std::vector<Fr>, 3> &w);
void verifyBundle(const BundleStatement &s, const BundleProof &p);
} // namespace zkgpt::r1
