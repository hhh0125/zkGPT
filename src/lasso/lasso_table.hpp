#pragma once
#include "lasso_types.hpp"
namespace route_b::lasso {
LassoLookupInstance makeInstance(const std::vector<Fr> &table, std::size_t query_length,
                                 const std::vector<unsigned> &bits, bool additive,
                                 const std::string &profile);
void validateInstance(const LassoLookupInstance &instance);
Digest instanceDigest(const LassoLookupInstance &instance);
std::vector<std::vector<Fr>> subTables(const LassoLookupInstance &instance);
LassoSelector makeSelector(const LassoLookupInstance &instance,
                           const std::vector<std::size_t> &addresses);
ProverWitness makeWitness(const LassoLookupInstance &instance, const std::vector<Fr> &query);
Fr addressEvaluation(const std::vector<Fr> &point);
Fr combineValues(const LassoLookupInstance &instance, const std::vector<Fr> &values);
} // namespace route_b::lasso
