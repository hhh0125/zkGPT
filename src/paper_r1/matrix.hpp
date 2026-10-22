#pragma once
#include "range.hpp"
namespace zkgpt::r1 {
struct MatrixSpec {
    std::string id;
    std::size_t rows = 0, inner = 0, columns = 0;
    std::size_t actual_rows = 0, actual_inner = 0, actual_columns = 0;
    PCSpec a, b;
    void validate() const;
};
struct MatrixProof {
    std::array<LinearProof, 2> padding;
    SumcheckProof sumcheck;
    OpenProof a, b;
};
MatrixProof proveMatrix(const MatrixSpec &s, const Commitment &ca, const Commitment &cb,
                        const std::vector<Fr> &a, const std::vector<Fr> &b,
                        const std::vector<Fr> &output, Fs &t);
void verifyMatrix(const MatrixSpec &s, const Commitment &ca, const Commitment &cb,
                  const std::vector<Fr> &output, const MatrixProof &p, Fs &t);
} // namespace zkgpt::r1
