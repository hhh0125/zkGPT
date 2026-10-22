#pragma once
#include "pc.hpp"
#include "sumcheck.hpp"
namespace zkgpt::r1 {
// Complete public power-of-two table: the c=1 indexed Lasso/Surge instance.
struct MemorySpec {
    std::string id;
    PCSpec address, value;
    std::vector<Fr> table;
    void validate() const;
};
struct MemoryProduct {
    ProductProof tree;
    std::vector<OpenProof> leaf;
};
struct MemoryProof {
    Commitment read, final;
    std::array<Fr, 3> roots;
    std::array<MemoryProduct, 3> products;
};
MemoryProof proveMemory(const MemorySpec &s, const Commitment &ca, const Commitment &cv,
                        const std::vector<Fr> &address, const std::vector<Fr> &value, Fs &t);
void verifyMemory(const MemorySpec &s, const Commitment &ca, const Commitment &cv,
                  const MemoryProof &p, Fs &t);
PCSpec readSpec(const MemorySpec &s);
PCSpec finalSpec(const MemorySpec &s);
} // namespace zkgpt::r1
