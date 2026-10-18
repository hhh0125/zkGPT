#include "lasso_memory.hpp"
#include "lasso_sumcheck.hpp"
#include "lasso_table.hpp"
#include <algorithm>
namespace route_b::lasso {
std::vector<std::vector<Fr>> memoryTables(const LassoLookupInstance &x) {
    auto t = subTables(x);
    std::vector<Fr> v(paddedSize(x.table.size()), Fr(0));
    std::fill(v.begin(), v.begin() + x.table.size(), Fr(1));
    t.push_back(v);
    return t;
}
Fr fingerprint(const Fr &a, const Fr &v, const Fr &c, const Fr &g, const Fr &t) {
    return a * g * g + v * g + c - t;
}
std::vector<Fr> memoryLeaves(const MemoryWitness &w, unsigned kind, const Fr &g, const Fr &t) {
    require(kind < 3, "memory kind");
    std::vector<Fr> v;
    if (kind == 2) {
        for (std::size_t i = 0; i < w.table.size(); ++i)
            v.push_back(fingerprint(Fr(i), w.table[i], w.final[i], g, t));
    } else
        for (std::size_t i = 0; i < w.address.size(); ++i)
            v.push_back(
                fingerprint(w.address[i], w.value[i], w.read[i] + Fr(kind == 0 ? 1 : 0), g, t));
    return v;
}
Fr initialProduct(const std::vector<Fr> &table, const Fr &g, const Fr &t) {
    Fr p = 1;
    for (std::size_t i = 0; i < table.size(); ++i)
        p *= fingerprint(Fr(i), table[i], Fr(0), g, t);
    return p;
}
void appendMemoryCommitments(Transcript &t, const LassoProof &p) {
    t.appendU64("memory.count", p.memory_commitments.size());
    for (std::size_t i = 0; i < p.memory_commitments.size(); ++i) {
        auto label = "memory." + std::to_string(i);
        auto &m = p.memory_commitments[i];
        t.appendG1(label + ".E", m.value);
        t.appendG1(label + ".read", m.read);
        t.appendG1(label + ".final", m.final);
    }
}
void appendRoots(Transcript &t, const LassoProof &p) {
    for (std::size_t i = 0; i < p.memories.size(); ++i)
        for (unsigned k = 0; k < 3; ++k)
            t.appendFr("memory." + std::to_string(i) + ".root." + std::to_string(k),
                       p.memories[i].roots[k]);
}
} // namespace route_b::lasso
