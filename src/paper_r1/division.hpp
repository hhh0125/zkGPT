#pragma once
#include "range.hpp"
namespace zkgpt::r1 {
// Coordinatewise A/B rounded toward +infinity at half ties. B is committed,
// positive on active coordinates, and zero on the padded tail.
struct DivisionSpec {
    std::string id;
    PCSpec numerator, denominator, quotient;
    unsigned numerator_bits = 0, quotient_bits = 0, denominator_bits = 0;
    void validate() const;
};
struct DivisionProof {
    std::array<Commitment, 5> helper;
    std::array<RangeProof, 5> range;
    std::array<RangeBindingProof, 2> signed_binding;
    std::array<OpenProof, 2> positive_binding;
    std::array<SumcheckProof, 2> constraints;
    std::array<std::array<OpenProof, 5>, 2> endpoint;
};
RangeSpec divisionHelperRange(const DivisionSpec &s, unsigned k);
DivisionProof proveDivision(const DivisionSpec &s, const Commitment &a, const Commitment &b,
                            const Commitment &q, const std::vector<Fr> &numerator,
                            const std::vector<Fr> &denominator, const std::vector<Fr> &quotient,
                            Fs &t);
void verifyDivision(const DivisionSpec &s, const Commitment &a, const Commitment &b,
                    const Commitment &q, const DivisionProof &p, Fs &t);
} // namespace zkgpt::r1
