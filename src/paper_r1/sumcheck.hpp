#pragma once
#include "core.hpp"
#include <functional>
namespace zkgpt::r1 {
using Expression = std::function<Fr(const std::vector<Fr> &)>;
struct SumcheckProof {
    std::vector<std::vector<Fr>> rounds;
};
struct Endpoint {
    std::vector<Fr> point;
    Fr value = 0;
};
Fr absorbSumcheckRound(const std::vector<Fr> &msg, Fs &t, const std::string &label);
Endpoint proveSumcheck(std::vector<std::vector<Fr>> factors, unsigned degree,
                       const Expression &expr, SumcheckProof &proof, Fs &t,
                       const std::string &label);
Endpoint verifySumcheck(unsigned variables, unsigned degree, const Fr &claim,
                        const SumcheckProof &proof, Fs &t, const std::string &label);
struct ProductLayer {
    SumcheckProof sumcheck;
    Fr left = 0, right = 0;
};
struct ProductProof {
    std::vector<ProductLayer> layers;
};
Fr product(const std::vector<Fr> &v);
Endpoint proveProduct(const std::vector<Fr> &leaves, ProductProof &p, Fs &t,
                      const std::string &label);
Endpoint verifyProduct(std::size_t n, const Fr &root, const ProductProof &p, Fs &t,
                       const std::string &label);
} // namespace zkgpt::r1
