#pragma once
#include "lasso_types.hpp"
namespace route_b::lasso {
std::vector<std::vector<Fr>> memoryTables(const LassoLookupInstance &x);
Fr fingerprint(const Fr &a, const Fr &v, const Fr &count, const Fr &gamma, const Fr &tau);
std::vector<Fr> memoryLeaves(const MemoryWitness &w, unsigned kind, const Fr &gamma, const Fr &tau);
Fr initialProduct(const std::vector<Fr> &table, const Fr &gamma, const Fr &tau);
void appendMemoryCommitments(Transcript &t, const LassoProof &p);
void appendRoots(Transcript &t, const LassoProof &p);
} // namespace route_b::lasso
