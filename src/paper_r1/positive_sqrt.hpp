#pragma once
#include "range.hpp"
namespace zkgpt::r1 {
// Positive integer domain, as used by the official LayerNorm B>=1 / sigma>0.
// Does not define a zero-input sqrt extension of the printed paper inequality.
struct PositiveSqrtSpec {
    std::string id;
    PCSpec input, root;
    unsigned input_bits = 0, root_bits = 0; // 1<=root<=2^root_bits
    void validate() const;
};
struct PositiveSqrtProof {
    std::array<Commitment, 3> helper;
    std::array<RangeProof, 4> range;
    std::array<OpenProof, 2> positive_binding;
    std::array<SumcheckProof, 2> constraints;
    std::array<std::array<OpenProof, 3>, 2> endpoint;
};
PCSpec positiveSqrtHelperSpec(const PositiveSqrtSpec &s, unsigned k);
RangeSpec positiveSqrtRangeSpec(const PositiveSqrtSpec &s, unsigned k);
PositiveSqrtProof provePositiveSqrt(const PositiveSqrtSpec &s, const Commitment &input,
                                    const Commitment &root, const std::vector<Fr> &x,
                                    const std::vector<Fr> &y, Fs &t);
void verifyPositiveSqrt(const PositiveSqrtSpec &s, const Commitment &input, const Commitment &root,
                        const PositiveSqrtProof &p, Fs &t);
} // namespace zkgpt::r1
