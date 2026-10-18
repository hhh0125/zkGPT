#pragma once

#include "range_protocol.hpp"

#include "zk_scalar.hpp"
#include <string>
#include <vector>

Degree1SumcheckProof proveDegree1Sumcheck(const std::vector<Fr> &values, const Fr &initial_claim,
                                          Transcript &transcript, const std::string &label,
                                          std::vector<Fr> *opening_point = nullptr);

bool verifyDegree1Sumcheck(const Fr &initial_claim, const Degree1SumcheckProof &proof,
                           Transcript &transcript, const std::string &label,
                           std::vector<Fr> *opening_point = nullptr, std::string *error = nullptr);

Degree3SumcheckProof proveDegree3Sumcheck(const std::vector<Fr> &equality_point,
                                          const std::vector<Fr> &f_values,
                                          const std::vector<Fr> &g_values, const Fr &initial_claim,
                                          Transcript &transcript, const std::string &label,
                                          std::vector<Fr> *opening_point = nullptr);

bool verifyDegree3Sumcheck(const std::vector<Fr> &equality_point, const Fr &initial_claim,
                           const Degree3SumcheckProof &proof, Transcript &transcript,
                           const std::string &label, std::vector<Fr> *opening_point = nullptr,
                           std::string *error = nullptr);

namespace zkgpt::zk {
// Conditional sumcheck: callers MUST bind the original vector commitments,
// layout, generator descriptors and complete public polynomial definition into
// this same parent transcript BEFORE constructing a mask session. They must
// then authenticate returned hidden endpoints against those SAME commitments.
// An opening to a commitment chosen after the point does not establish the
// intended polynomial identity. No raw endpoint is in these proofs.
struct MaskedSumcheckProof {
    std::vector<G1> coefficients;
    RepresentationProof coefficient_knowledge;
    Fr masked_total = Fr(0);
    EqualityProof initial_relation;
    std::vector<std::array<Fr, 4>> rounds;
    EqualityProof terminal_relation;
};
class MaskedSumcheckProver {
  public:
    MaskedSumcheckProver(const Parameters &, std::size_t variables, unsigned degree,
                         const G1 &initial, const ScalarWitness &, MaskedSumcheckProof &,
                         Transcript &, std::string label, std::string context);
    Fr round(const std::vector<Fr> &raw_values_at_integers);
    void finish(const G1 &terminal, const ScalarWitness &);
    const std::vector<Fr> &point() const { return point_; }

  private:
    const Parameters &pp_;
    std::size_t variables_;
    unsigned degree_;
    MaskedSumcheckProof &proof_;
    Transcript &transcript_;
    std::string label_, context_;
    std::vector<ScalarWitness> coefficients_;
    std::vector<Fr> point_;
    Fr rho_, masked_claim_, raw_claim_;
    bool finished_ = false;
};
class MaskedSumcheckVerifier {
  public:
    MaskedSumcheckVerifier(const Parameters &, std::size_t variables, unsigned degree,
                           const G1 &initial, const MaskedSumcheckProof &, Transcript &,
                           std::string label, std::string context);
    Fr round();
    void finish(const G1 &terminal);
    const std::vector<Fr> &point() const { return point_; }

  private:
    const Parameters &pp_;
    std::size_t variables_;
    unsigned degree_;
    const MaskedSumcheckProof &proof_;
    Transcript &transcript_;
    std::string label_, context_;
    std::vector<Fr> point_;
    Fr rho_, masked_claim_;
    bool finished_ = false;
};
Fr equalityPolynomial(const std::vector<Fr> &, const std::vector<Fr> &);
struct Degree1ZkProof {
    MaskedSumcheckProof masked;
    G1 terminal;
    RepresentationProof terminal_knowledge;
};
struct Degree1ZkResult {
    Degree1ZkProof proof;
    ScalarWitness terminal_witness;
    std::vector<Fr> point;
};
struct Degree3ZkProof {
    MaskedSumcheckProof masked;
    G1 f_terminal, g_terminal, product_commitment;
    RepresentationProof terminal_knowledge;
    ProductProof product;
};
struct Degree3ZkResult {
    Degree3ZkProof proof;
    ScalarWitness f_witness, g_witness;
    std::vector<Fr> point;
};
Degree1ZkResult proveDegree1SumcheckZk(const Parameters &, const std::vector<Fr> &,
                                       const G1 &initial, const ScalarWitness &, Transcript &,
                                       const std::string &label, const std::string &context);
bool verifyDegree1SumcheckZk(const Parameters &, std::size_t variables, const G1 &initial,
                             const Degree1ZkProof &, Transcript &, const std::string &label,
                             const std::string &context, std::vector<Fr> *point = nullptr,
                             std::string *error = nullptr);
Degree3ZkResult proveDegree3SumcheckZk(const Parameters &, const std::vector<Fr> &equality_point,
                                       const std::vector<Fr> &f, const std::vector<Fr> &g,
                                       const G1 &initial, const ScalarWitness &, Transcript &,
                                       const std::string &label, const std::string &context);
bool verifyDegree3SumcheckZk(const Parameters &, const std::vector<Fr> &equality_point,
                             const G1 &initial, const Degree3ZkProof &, Transcript &,
                             const std::string &label, const std::string &context,
                             std::vector<Fr> *point = nullptr, std::string *error = nullptr);
Bytes encodeMaskedSumcheck(const MaskedSumcheckProof &, std::size_t variables, unsigned degree);
std::size_t maskedSumcheckEncodedSize(std::size_t variables,unsigned degree);
MaskedSumcheckProof decodeMaskedSumcheck(const Bytes &, std::size_t variables, unsigned degree);
Bytes encodeDegree1Zk(const Degree1ZkProof &, std::size_t variables);
Degree1ZkProof decodeDegree1Zk(const Bytes &, std::size_t variables);
Bytes encodeDegree3Zk(const Degree3ZkProof &, std::size_t variables);
Degree3ZkProof decodeDegree3Zk(const Bytes &, std::size_t variables);
} // namespace zkgpt::zk
