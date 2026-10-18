#pragma once
#include "lasso_types.hpp"
namespace route_b::lasso {
SumcheckResult proveSumcheck(std::vector<std::vector<Fr>> factors, unsigned degree,
                             const Expression &expression, Sumcheck &proof, Transcript &t,
                             const std::string &label);
SumcheckResult verifySumcheck(unsigned variables, unsigned degree, const Fr &claim,
                              const Sumcheck &proof, Transcript &t, const std::string &label);
struct ProductEndpoint {
    std::vector<Fr> point;
    Fr value = 0;
};
Fr product(const std::vector<Fr> &values);
ProductEndpoint proveProduct(const std::vector<Fr> &leaves, ProductProof &proof, Transcript &t,
                             const std::string &label);
ProductEndpoint verifyProduct(std::size_t leaf_count, const Fr &root, const ProductProof &proof,
                              Transcript &t, const std::string &label);
} // namespace route_b::lasso
