#include "lasso_commitment.hpp"
#include "lasso_memory.hpp"
#include "lasso_sumcheck.hpp"
#include "lasso_table.hpp"
#include "lasso_verifier.hpp"
namespace route_b::lasso {
bool verify(const LassoLookupInstance &x, const LassoProof &p, VerifiedLookupClaim *claim,
            std::string *error) {
    if (claim)
        *claim = VerifiedLookupClaim{};
    try {
        validateInstance(x);
        const auto n = paddedSize(x.query_length), c = x.segment_bits.size();
        auto tables = memoryTables(x);
        require(p.instance_digest == instanceDigest(x), "unapproved instance/table/layout");
        require(p.dim_commitments.size() == c && p.memory_commitments.size() == c + 1 &&
                    p.memories.size() == c + 1,
                "commitment/memory count");
        require(p.padding_openings.size() == c + 1 && p.read_evaluations.size() == c,
                "opening count");
        require(p.memory_commitments.back().value.isZero(), "validity E must be fixed to one");
        Transcript t(kDomain);
        appendCommitments(t, x, p);
        auto r = challengePoint(t, log2Exact(n), "lookup.row");
        appendMemoryCommitments(t, p);
        require(verifyMle(p.query_commitment, r, p.query_evaluation, t, "query.evaluation"),
                "query opening");
        auto pad = eqWeights(challengePoint(t, log2Exact(n), "padding.row"));
        Fr weight = 0;
        for (std::size_t i = 0; i < n; ++i) {
            if (i < x.query_length)
                pad[i] = Fr(0);
            weight += pad[i];
        }
        require(p.padding_openings[0].value == weight * x.table[0], "query padding value");
        require(verifyLinear(p.query_commitment, pad, p.padding_openings[0], t, "padding.query"),
                "query padding opening");
        for (std::size_t h = 0; h < c; ++h) {
            auto &o = p.padding_openings[h + 1];
            require(o.value.isZero(), "address padding value");
            require(
                verifyLinear(p.dim_commitments[h], pad, o, t, "padding.dim." + std::to_string(h)),
                "address padding opening");
        }
        auto sc = verifySumcheck(log2Exact(n), 2, p.query_evaluation.value, p.lookup_sumcheck, t,
                                 "lookup.sumcheck");
        std::vector<Fr> evaluations;
        for (std::size_t h = 0; h < c; ++h) {
            require(verifyMle(p.memory_commitments[h].value, sc.point, p.read_evaluations[h], t,
                              "lookup.E." + std::to_string(h)),
                    "read-value opening");
            evaluations.push_back(p.read_evaluations[h].value);
        }
        require(sc.value == eq(r, sc.point) * combineValues(x, evaluations), "lookup endpoint");
        auto gamma = t.challenge("memory.gamma"), tau = t.challenge("memory.tau");
        appendRoots(t, p);
        for (std::size_t h = 0; h <= c; ++h) {
            auto &m = p.memories[h];
            require(initialProduct(tables[h], gamma, tau) * m.roots[0] == m.roots[1] * m.roots[2],
                    "memory multiset fingerprint");
            for (unsigned kind = 0; kind < 3; ++kind) {
                auto l = "memory." + std::to_string(h) + ".product." + std::to_string(kind);
                const auto &gp = m.products[kind];
                auto end = verifyProduct(kind == 2 ? tables[h].size() : n, m.roots[kind], gp, t, l);
                auto &ops = gp.leaf_openings;
                Fr expected;
                if (kind == 2) {
                    require(ops.size() == 1, "final leaf opening count");
                    require(verifyMle(p.memory_commitments[h].final, end.point, ops[0], t,
                                      l + ".final"),
                            "final counter opening");
                    expected = fingerprint(addressEvaluation(end.point), mle(tables[h], end.point),
                                           ops[0].value, gamma, tau);
                } else {
                    require(ops.size() == (h < c ? 3 : c + 1), "read leaf opening count");
                    std::size_t index = 0;
                    Fr address = 0, value = 1;
                    if (h < c) {
                        require(
                            verifyMle(p.dim_commitments[h], end.point, ops[index], t, l + ".dim"),
                            "dim opening");
                        address = ops[index++].value;
                    } else {
                        Fr scale = 1;
                        for (std::size_t j = 0; j < c; ++j) {
                            require(verifyMle(p.dim_commitments[j], end.point, ops[index], t,
                                              l + ".dim." + std::to_string(j)),
                                    "validity dim opening");
                            address += scale * ops[index++].value;
                            scale *= Fr(std::size_t{1} << x.segment_bits[j]);
                        }
                    }
                    if (h < c) {
                        require(verifyMle(p.memory_commitments[h].value, end.point, ops[index], t,
                                          l + ".E"),
                                "memory value opening");
                        value = ops[index++].value;
                    }
                    require(verifyMle(p.memory_commitments[h].read, end.point, ops[index], t,
                                      l + ".read"),
                            "read counter opening");
                    expected = fingerprint(address, value, ops[index].value + Fr(kind == 0 ? 1 : 0),
                                           gamma, tau);
                }
                require(end.value == expected, "authenticated memory leaf mismatch");
            }
        }
        if (claim) {
            claim->instance_digest = p.instance_digest;
            claim->query_commitment = p.query_commitment;
            claim->row_point = r;
            claim->evaluation = p.query_evaluation.value;
        }
        if (error)
            error->clear();
        return true;
    } catch (const std::exception &e) {
        if (error)
            *error = e.what();
        return false;
    }
}
} // namespace route_b::lasso
