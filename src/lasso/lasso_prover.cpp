#include "lasso_commitment.hpp"
#include "lasso_memory.hpp"
#include "lasso_prover.hpp"
#include "lasso_sumcheck.hpp"
#include "tests/adversarial_prover.hpp"
namespace route_b::lasso {
LassoProof adversarial::proveWitness(const LassoLookupInstance &x, const ProverWitness &w) {
    validateInstance(x);
    const auto n = paddedSize(x.query_length), c = x.segment_bits.size();
    require(w.query.size() == n && w.selector.dim.size() == c && w.memories.size() == c + 1,
            "witness shape");
    LassoProof p;
    p.instance_digest = instanceDigest(x);
    p.query_commitment = commit(w.query);
    for (auto &v : w.selector.dim) {
        require(v.size() == n, "dim shape");
        p.dim_commitments.push_back(commit(v));
    }
    Transcript t(kDomain);
    appendCommitments(t, x, p);
    auto r = challengePoint(t, log2Exact(n), "lookup.row");
    for (std::size_t h = 0; h <= c; ++h) {
        auto &m = w.memories[h];
        require(m.address.size() == n && m.value.size() == n && m.read.size() == n &&
                    m.table.size() == m.final.size(),
                "memory witness shape");
        MemoryCommitments cm;
        if (h < c)
            cm.value = commit(m.value);
        else
            cm.value.clear();
        cm.read = commit(m.read);
        cm.final = commit(m.final);
        p.memory_commitments.push_back(cm);
    }
    appendMemoryCommitments(t, p);
    p.query_evaluation = openMle(w.query, p.query_commitment, r, t, "query.evaluation");
    auto pad = eqWeights(challengePoint(t, log2Exact(n), "padding.row"));
    for (std::size_t i = 0; i < x.query_length; ++i)
        pad[i] = Fr(0);
    p.padding_openings.push_back(openLinear(w.query, p.query_commitment, pad, t, "padding.query"));
    for (std::size_t h = 0; h < c; ++h)
        p.padding_openings.push_back(openLinear(w.selector.dim[h], p.dim_commitments[h], pad, t,
                                                "padding.dim." + std::to_string(h)));
    std::vector<std::vector<Fr>> f{eqWeights(r)};
    for (std::size_t h = 0; h < c; ++h)
        f.push_back(w.memories[h].value);
    auto sc = proveSumcheck(
        f, 2,
        [&](const auto &v) {
            return v[0] * combineValues(x, {v.begin() + 1, v.end()});
        },
        p.lookup_sumcheck, t, "lookup.sumcheck");
    for (std::size_t h = 0; h < c; ++h)
        p.read_evaluations.push_back(openMle(w.memories[h].value, p.memory_commitments[h].value,
                                             sc.point, t, "lookup.E." + std::to_string(h)));
    auto gamma = t.challenge("memory.gamma"), tau = t.challenge("memory.tau");
    p.memories.resize(c + 1);
    for (std::size_t h = 0; h <= c; ++h)
        for (unsigned k = 0; k < 3; ++k)
            p.memories[h].roots[k] = product(memoryLeaves(w.memories[h], k, gamma, tau));
    appendRoots(t, p);
    for (std::size_t h = 0; h <= c; ++h)
        for (unsigned kind = 0; kind < 3; ++kind) {
            auto l = "memory." + std::to_string(h) + ".product." + std::to_string(kind);
            auto &gp = p.memories[h].products[kind];
            auto end = proveProduct(memoryLeaves(w.memories[h], kind, gamma, tau), gp, t, l);
            auto &m = w.memories[h];
            if (kind == 2)
                gp.leaf_openings.push_back(
                    openMle(m.final, p.memory_commitments[h].final, end.point, t, l + ".final"));
            else {
                if (h < c)
                    gp.leaf_openings.push_back(
                        openMle(w.selector.dim[h], p.dim_commitments[h], end.point, t, l + ".dim"));
                else
                    for (std::size_t j = 0; j < c; ++j)
                        gp.leaf_openings.push_back(openMle(w.selector.dim[j], p.dim_commitments[j],
                                                           end.point, t,
                                                           l + ".dim." + std::to_string(j)));
                if (h < c)
                    gp.leaf_openings.push_back(
                        openMle(m.value, p.memory_commitments[h].value, end.point, t, l + ".E"));
                gp.leaf_openings.push_back(
                    openMle(m.read, p.memory_commitments[h].read, end.point, t, l + ".read"));
            }
        }
    return p;
}
LassoProof prove(const LassoLookupInstance &x, const std::vector<Fr> &q) {
    return adversarial::proveWitness(x, makeWitness(x, q));
}
} // namespace route_b::lasso
