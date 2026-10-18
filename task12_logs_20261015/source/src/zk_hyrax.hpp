#pragma once
#include "hyrax_opening.hpp"
#include "zk_scalar.hpp"

namespace zkgpt::zk {
enum class Layout : std::uint8_t { RowMajor = 0, ColumnMajor = 1 };
struct MatrixShape {
    std::size_t size, log_size, rows, columns;
};
MatrixShape matrixShape(std::size_t size);
struct MatrixCommitment {
    std::size_t size = 0;
    Layout layout = Layout::RowMajor;
    std::vector<G1> rows;
};
struct MatrixWitness {
    std::vector<Fr> values, row_blinders;
};
// Borrowed values/blinders must outlive the synchronous call; no retained refs.
struct MatrixWitnessView {
    HyraxFrWitnessView values;
    const std::vector<Fr> &row_blinders;
};
struct MatrixCommitResult {
    MatrixCommitment public_commitment;
    MatrixWitness witness;
};
MatrixCommitResult commitMatrix(const Parameters &, const std::vector<Fr> &, Layout);
// Production overload: never copies the full witness. Fresh row_blinders output.
MatrixCommitment commitMatrix(const Parameters &, HyraxFrWitnessView, Layout,
                              std::vector<Fr> &row_blinders);
struct MleStatement {
    MatrixCommitment commitment;
    std::vector<Fr> point;
    G1 evaluation;
    std::string context;
};
struct LinearProof {
    G1 A_C, A_y;
    std::vector<Fr> z;
    Fr zrho = Fr(0), zs = Fr(0);
};
LinearProof proveMle(const Parameters &, const MleStatement &, const MatrixWitness &,
                     const Fr &evaluation_blinder, Transcript &, const std::string &label);
LinearProof proveMle(const Parameters &, const MleStatement &, MatrixWitnessView,
                     const Fr &evaluation_blinder, Transcript &, const std::string &label);
bool verifyMle(const Parameters &, const MleStatement &, const LinearProof &, Transcript &,
               const std::string &label, std::string *error = nullptr);
Fr evaluateMle(const std::vector<Fr> &, const std::vector<Fr> &, Layout);
namespace detail {
// INTERNAL row-aggregated linear opening seam for Sparse/model integration.
// Caller must first bind the original public matrix/layout/coordinates and
// derive this aggregated commitment and coefficients from that public input.
struct LinearStatement {
    G1 commitment, evaluation;
    std::vector<Fr> coefficients;
    std::string context;
};
struct LinearWitness {
    std::vector<Fr> values;
    Fr blinder = Fr(0);
};
LinearProof proveLinear(const Parameters &, const LinearStatement &, const LinearWitness &,
                        const Fr &evaluation_blinder, Transcript &, const std::string &label);
bool verifyLinear(const Parameters &, const LinearStatement &, const LinearProof &, Transcript &,
                  const std::string &label, std::string *error = nullptr);
} // namespace detail
Bytes encode(const LinearProof &);
LinearProof decodeLinear(const Bytes &, std::size_t expected_columns);
} // namespace zkgpt::zk
