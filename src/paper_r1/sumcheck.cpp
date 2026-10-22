#include "sumcheck.hpp"
namespace zkgpt::r1 {
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
Fr absorbSumcheckRound(const std::vector<Fr> &msg, Fs &t, const std::string &l) {
    t.text("sumcheck.round", l);
    t.integer("sumcheck.degree", msg.size() - 1);
    for (auto &x : msg)
        t.scalar("sumcheck.message", x);
    return t.challenge("sumcheck.challenge");
}
#ifndef R1_VERIFY_ONLY
Endpoint proveSumcheck(std::vector<std::vector<Fr>> f, unsigned degree, const Expression &expr,
                       SumcheckProof &p, Fs &t, const std::string &label) {
    require(!f.empty() && degree >= 1 && degree <= 3 && p.rounds.empty(), "sumcheck arguments");
    auto n = f[0].size();
    auto d = log2exact(n);
    for (auto &v : f)
        require(v.size() == n, "sumcheck factors");
    Endpoint end;
    for (unsigned bit = 0; bit < d; ++bit) {
        std::vector<Fr> msg(degree + 1, Fr(0)), args(f.size());
        for (unsigned j = 0; j <= degree; ++j)
            for (std::size_t i = 0; i < n / 2; ++i) {
                for (std::size_t k = 0; k < f.size(); ++k)
                    args[k] = f[k][2 * i] + Fr(j) * (f[k][2 * i + 1] - f[k][2 * i]);
                msg[j] += expr(args);
            }
        auto r = absorbSumcheckRound(msg, t, label + "/" + std::to_string(bit));
        p.rounds.push_back(msg);
        end.point.push_back(r);
        for (auto &v : f) {
            for (std::size_t i = 0; i < n / 2; ++i)
                v[i] = v[2 * i] + r * (v[2 * i + 1] - v[2 * i]);
            v.resize(n / 2);
        }
        n /= 2;
    }
    std::vector<Fr> args;
    for (auto &v : f)
        args.push_back(v[0]);
    end.value = expr(args);
    return end;
}
#endif

Endpoint verifySumcheck(unsigned d, unsigned degree, const Fr &claim, const SumcheckProof &p, Fs &t,
                        const std::string &label) {
    require(d <= 56 && degree >= 1 && degree <= 3 && p.rounds.size() == d, "sumcheck shape");
    Endpoint end;
    end.value = claim;
    for (unsigned bit = 0; bit < d; ++bit) {
        const auto &msg = p.rounds[bit];
        require(msg.size() == degree + 1, "sumcheck degree");
        require(msg[0] + msg[1] == end.value, "sumcheck recurrence");
        auto r = absorbSumcheckRound(msg, t, label + "/" + std::to_string(bit));
        end.point.push_back(r);
        end.value = interpolate(msg, r);
    }
    return end;
}
Fr product(const std::vector<Fr> &v) {
    Fr x = 1;
    for (auto &f : v)
        x *= f;
    return x;
}
#ifndef R1_VERIFY_ONLY
Endpoint proveProduct(const std::vector<Fr> &leaves, ProductProof &p, Fs &t,
                      const std::string &label) {
    auto d = log2exact(leaves.size());
    require(p.layers.empty(), "product proof must be empty");
    std::vector<std::vector<Fr>> tree{leaves};
    for (unsigned i = 0; i < d; ++i) {
        const auto &v = tree.back();
        std::vector<Fr> next(v.size() / 2);
        for (std::size_t j = 0; j < next.size(); ++j)
            next[j] = v[2 * j] * v[2 * j + 1];
        tree.push_back(std::move(next));
    }
    Endpoint out;
    out.value = tree.back()[0];
    for (unsigned layer = 0; layer < d; ++layer) {
        const auto &child = tree[d - layer - 1];
        std::vector<Fr> a(child.size() / 2), b(child.size() / 2);
        for (std::size_t j = 0; j < a.size(); ++j) {
            a[j] = child[2 * j];
            b[j] = child[2 * j + 1];
        }
        ProductLayer proof;
        auto l = label + "/" + std::to_string(layer);
        auto end = proveSumcheck(
            {weights(out.point), a, b}, 3, [](const auto &v) { return v[0] * v[1] * v[2]; },
            proof.sumcheck, t, l);
        proof.left = mle(a, end.point);
        proof.right = mle(b, end.point);
        t.scalar("product.left", proof.left);
        t.scalar("product.right", proof.right);
        auto r = t.challenge("product.fold");
        out.point = end.point;
        out.point.insert(out.point.begin(), r);
        out.value = proof.left + r * (proof.right - proof.left);
        p.layers.push_back(std::move(proof));
    }
    return out;
}
#endif

Endpoint verifyProduct(std::size_t n, const Fr &root, const ProductProof &p, Fs &t,
                       const std::string &label) {
    auto d = log2exact(n);
    require(p.layers.size() == d, "product layers");
    Endpoint out;
    out.value = root;
    for (unsigned layer = 0; layer < d; ++layer) {
        const auto &q = p.layers[layer];
        auto end =
            verifySumcheck(layer, 3, out.value, q.sumcheck, t, label + "/" + std::to_string(layer));
        require(end.value == equality(out.point, end.point) * q.left * q.right,
                "product layer endpoint");
        t.scalar("product.left", q.left);
        t.scalar("product.right", q.right);
        auto r = t.challenge("product.fold");
        out.point = end.point;
        out.point.insert(out.point.begin(), r);
        out.value = q.left + r * (q.right - q.left);
    }
    return out;
}
} // namespace zkgpt::r1
