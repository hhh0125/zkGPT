#pragma once
#include "range.hpp"
namespace zkgpt::r1 {
// Exact integer predicates, with explicit domains preventing modular aliases.
// These modules do not select a GPT-2 quantization profile.
struct RoundingSpec {
    std::string id;
    PCSpec numerator, quotient;
    std::uint64_t denominator = 0;
    unsigned numerator_bits = 0, quotient_bits = 0;
    void validate() const;
};
struct RoundingProof {
    std::array<Commitment, 4> helper;
    std::array<RangeProof, 4> range;
    std::array<RangeBindingProof, 2> signed_binding;
    std::array<OpenProof, 4> relation;
};
RoundingProof proveRounding(const RoundingSpec &s, const Commitment &a, const Commitment &q,
                            const std::vector<Fr> &numerator, const std::vector<Fr> &quotient,
                            Fs &t);
void verifyRounding(const RoundingSpec &s, const Commitment &a, const Commitment &q,
                    const RoundingProof &p, Fs &t);
struct MaximumSpec {
    std::string id;
    PCSpec scores, maximum;
    unsigned bits = 0;
    void validate() const;
};
struct MaximumProof {
    std::array<Commitment, 3> helper;
    std::array<RangeProof, 3> range;
    std::array<RangeBindingProof, 2> signed_binding;
    std::array<OpenProof, 3> relation;
    ProductProof zero_product;
    OpenProof product_leaf;
};
MaximumProof proveMaximum(const MaximumSpec &s, const Commitment &scores, const Commitment &maximum,
                          const std::vector<Fr> &x, const Fr &m, Fs &t);
void verifyMaximum(const MaximumSpec &s, const Commitment &scores, const Commitment &maximum,
                   const MaximumProof &p, Fs &t);
} // namespace zkgpt::r1
