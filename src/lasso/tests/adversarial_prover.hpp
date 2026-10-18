#pragma once
#include "lasso_types.hpp"
namespace route_b::lasso::adversarial {
// Research test seam. Does not enforce a valid witness or call the verifier.
// It regenerates EVERY commitment and Fiat-Shamir message from supplied data.
LassoProof proveWitness(const LassoLookupInstance &, const ProverWitness &);
} // namespace route_b::lasso::adversarial
