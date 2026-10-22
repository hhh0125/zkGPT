#pragma once
#include "range.hpp"
namespace zkgpt::r1 {
// Block order X,W,aux,L. Local bits first; block number b+2*a.
// context must include approved circuit/profile/model identity and public Y.
struct InputSpec {
    std::string context;
    std::vector<Fr> x;
    std::array<PCSpec, 3> spec;
    std::array<Commitment, 3> commitment;
    void validate() const;
};
struct InputClaim {
    std::vector<Fr> point;
    Fr value = 0;
};
struct InputProof {
    SumcheckProof local, selectors;
    std::array<OpenProof, 3> opening;
    std::array<LinearProof, 3> padding;
};
void appendInputStatement(const InputSpec &s, Fs &t);
InputProof proveInput(const InputSpec &s, const std::array<std::vector<Fr>, 3> &w,
                      const std::vector<InputClaim> &claims, Fs &t);
void verifyInput(const InputSpec &s, const std::vector<InputClaim> &claims, const InputProof &p,
                 Fs &t);
} // namespace zkgpt::r1
