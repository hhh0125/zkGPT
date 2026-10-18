#include "lasso_sumcheck.hpp"
#include "lasso_table.hpp"
namespace route_b::lasso {
static Fr interpolate(const std::vector<Fr> &v, const Fr &r) {
    Fr out = 0;
    for (std::size_t i = 0; i < v.size(); ++i) {
        Fr w = 1;
        for (std::size_t j = 0; j < v.size(); ++j)
            if (i != j)
                w *= (r - Fr(j)) / (Fr(i) - Fr(j));
        out += w * v[i];
    }
    return out;
}
static Fr appendRound(const std::vector<Fr> &v, Transcript &t, const std::string &l) {
    t.appendU64(l + ".degree", v.size() - 1);
    for (auto &f : v)
        t.appendFr(l + ".message", f);
    return t.challenge(l + ".challenge");
}
SumcheckResult proveSumcheck(std::vector<std::vector<Fr>> f, unsigned degree,
                             const Expression &expr, Sumcheck &p, Transcript &t,
                             const std::string &label) {
    require(!f.empty(), "empty factors");
    auto n = f[0].size();
    auto bits = log2Exact(n);
    require(n <= kMaxSize && degree >= 1 && degree <= 3, "sumcheck bounds");
    for (auto &v : f)
        require(v.size() == n, "factor size");
    SumcheckResult out;
    for (unsigned bit = 0; bit < bits; ++bit) {
        std::vector<Fr> msg(degree + 1, Fr(0)), args(f.size());
        for (unsigned j = 0; j <= degree; ++j)
            for (std::size_t i = 0; i < n / 2; ++i) {
                for (std::size_t k = 0; k < f.size(); ++k)
                    args[k] = f[k][2 * i] + Fr(j) * (f[k][2 * i + 1] - f[k][2 * i]);
                msg[j] += expr(args);
            }
        Fr r = appendRound(msg, t, label + "." + std::to_string(bit));
        p.rounds.push_back(msg);
        out.point.push_back(r);
        for (auto &v : f) {
            for (std::size_t i = 0; i < n / 2; ++i)
                v[i] = v[2 * i] + r * (v[2 * i + 1] - v[2 * i]);
            v.resize(n / 2);
        }
        n /= 2;
    }
    std::vector<Fr> a;
    for (auto &v : f)
        a.push_back(v[0]);
    out.value = expr(a);
    return out;
}
SumcheckResult verifySumcheck(unsigned variables, unsigned degree, const Fr &claim,
                              const Sumcheck &p, Transcript &t, const std::string &label) {
    require(variables <= kMaxBits && degree >= 1 && degree <= 3, "sumcheck bounds");
    require(p.rounds.size() == variables, "sumcheck round count");
    SumcheckResult out;
    out.value = claim;
    for (unsigned bit = 0; bit < variables; ++bit) {
        auto &v = p.rounds[bit];
        require(v.size() == degree + 1, "sumcheck degree");
        require(v[0] + v[1] == out.value, "sumcheck recurrence");
        auto r = appendRound(v, t, label + "." + std::to_string(bit));
        out.point.push_back(r);
        out.value = interpolate(v, r);
    }
    return out;
}
Fr product(const std::vector<Fr> &v) {
    Fr p = 1;
    for (auto &e : v)
        p *= e;
    return p;
}
ProductEndpoint proveProduct(const std::vector<Fr> &leaves, ProductProof &p, Transcript &t,
                             const std::string &label) {
    auto bits = log2Exact(leaves.size());
    require(leaves.size() <= kMaxSize, "tree size");
    std::vector<std::vector<Fr>> tree{leaves};
    for (unsigned i = 0; i < bits; ++i) {
        const auto &v = tree.back();
        std::vector<Fr> n;
        for (std::size_t j = 0; j < v.size(); j += 2)
            n.push_back(v[j] * v[j + 1]);
        tree.push_back(n);
    }
    ProductEndpoint out;
    out.value = tree.back()[0];
    for (unsigned layer = 0; layer < bits; ++layer) {
        auto &child = tree[bits - layer - 1];
        std::vector<Fr> a, b;
        for (std::size_t i = 0; i < child.size(); i += 2) {
            a.push_back(child[i]);
            b.push_back(child[i + 1]);
        }
        ProductLayer proof;
        auto l = label + ".layer." + std::to_string(layer);
        auto sc = proveSumcheck(
            {eqWeights(out.point), a, b}, 3, [](const auto &v) { return v[0] * v[1] * v[2]; },
            proof.sumcheck, t, l);
        proof.left = mle(a, sc.point);
        proof.right = mle(b, sc.point);
        t.appendFr(l + ".left", proof.left);
        t.appendFr(l + ".right", proof.right);
        auto r = t.challenge(l + ".fold");
        out.point = sc.point;
        out.point.insert(out.point.begin(), r);
        out.value = proof.left + r * (proof.right - proof.left);
        p.layers.push_back(proof);
    }
    return out;
}
ProductEndpoint verifyProduct(std::size_t n, const Fr &root, const ProductProof &p, Transcript &t,
                              const std::string &label) {
    auto bits = log2Exact(n);
    require(n <= kMaxSize && p.layers.size() == bits, "tree shape");
    ProductEndpoint out;
    out.value = root;
    for (unsigned layer = 0; layer < bits; ++layer) {
        const auto &proof = p.layers[layer];
        auto l = label + ".layer." + std::to_string(layer);
        auto sc = verifySumcheck(layer, 3, out.value, proof.sumcheck, t, l);
        require(sc.value == eq(out.point, sc.point) * proof.left * proof.right,
                "tree layer endpoint");
        t.appendFr(l + ".left", proof.left);
        t.appendFr(l + ".right", proof.right);
        auto r = t.challenge(l + ".fold");
        out.point = sc.point;
        out.point.insert(out.point.begin(), r);
        out.value = proof.left + r * (proof.right - proof.left);
    }
    return out;
}
} // namespace route_b::lasso
