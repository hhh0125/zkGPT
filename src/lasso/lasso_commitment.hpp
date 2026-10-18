#pragma once
#include "lasso_types.hpp"
namespace route_b::lasso {
const GeneratorSet &lassoGenerators(std::size_t count);
G1 commit(const std::vector<Fr> &values);
Opening openLinear(const std::vector<Fr> &values, const G1 &commitment,
                   const std::vector<Fr> &coefficients, Transcript &t, const std::string &label);
bool verifyLinear(const G1 &commitment, const std::vector<Fr> &coefficients, const Opening &opening,
                  Transcript &t, const std::string &label);
Opening openMle(const std::vector<Fr> &values, const G1 &commitment, const std::vector<Fr> &point,
                Transcript &t, const std::string &label);
bool verifyMle(const G1 &commitment, const std::vector<Fr> &point, const Opening &opening,
               Transcript &t, const std::string &label);
void appendCommitments(Transcript &t, const LassoLookupInstance &x, const LassoProof &p);
std::vector<Fr> challengePoint(Transcript &t, unsigned bits, const std::string &label);
} // namespace route_b::lasso
