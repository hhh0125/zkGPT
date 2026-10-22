#include "gkr.hpp"
namespace zkgpt::r1 {
void GkrSpec::validate() const {
    log2exact(input_size);
    require(!context.empty() && context.size() <= 4096 && !layers.empty() && layers.size() <= 4096,
            "GKR context/layers");
    auto n = input_size;
    for (auto &layer : layers) {
        log2exact(layer.size());
        for (auto &gate : layer)
            require((gate.kind == GateKind::Add || gate.kind == GateKind::Multiply) &&
                        gate.left < n && gate.right < n,
                    "GKR gate");
        n = layer.size();
    }
    require(output.empty() || output.size() == n, "GKR public output size");
}
#ifndef R1_VERIFY_ONLY
std::vector<std::vector<Fr>> evaluate(const GkrSpec &s, const std::vector<Fr> &input) {
    s.validate();
    require(input.size() == s.input_size, "GKR input size");
    std::vector<std::vector<Fr>> v{input};
    for (auto &layer : s.layers) {
        std::vector<Fr> next;
        const auto &old = v.back();
        for (auto &g : layer)
            next.push_back(g.coefficient * (g.kind == GateKind::Add ? old[g.left] + old[g.right]
                                                                    : old[g.left] * old[g.right]));
        v.push_back(std::move(next));
    }
    return v;
}
#endif

void appendGkrStatement(const GkrSpec &s, Fs &t) {
    s.validate();
    require(s.output.size() == s.layers.back().size(), "GKR requires complete public output");
    t.text("gkr.kind", "sparse-arithmetic-GKR-v1");
    t.text("gkr.context", s.context);
    t.integer("gkr.input_size", s.input_size);
    t.integer("gkr.layers", s.layers.size());
    for (auto &layer : s.layers) {
        t.integer("gkr.layer.size", layer.size());
        for (auto &g : layer) {
            t.integer("gkr.gate.kind", unsigned(g.kind));
            t.integer("gkr.gate.left", g.left);
            t.integer("gkr.gate.right", g.right);
            t.scalar("gkr.gate.coefficient", g.coefficient);
        }
    }
    for (auto &y : s.output)
        t.scalar("gkr.public.Y", y);
}
static std::vector<Fr> gateWeights(std::size_t n, const std::vector<InputClaim> &claims,
                                   const std::vector<Fr> &lambda) {
    std::vector<Fr> out(n, Fr(0));
    for (std::size_t j = 0; j < claims.size(); ++j) {
        auto e = weights(claims[j].point);
        require(e.size() == n, "GKR carried point");
        for (std::size_t i = 0; i < n; ++i)
            out[i] += lambda[j] * e[i];
    }
    return out;
}
#ifndef R1_VERIFY_ONLY
static void fold(std::vector<Fr> &w, const Fr &r) {
    for (std::size_t i = 0; i < w.size() / 2; ++i)
        w[i] = w[2 * i] + r * (w[2 * i + 1] - w[2 * i]);
    w.resize(w.size() / 2);
}
#endif

#ifndef R1_VERIFY_ONLY
static Fr eqBit(const Fr &r, bool bit) { return bit ? r : Fr(1) - r; }
#endif

#ifndef R1_VERIFY_ONLY
static GkrLayerProof proveLayer(const std::vector<Gate> &gates, const std::vector<Fr> &input,
                                const std::vector<Fr> &gw, Fs &t, const std::string &label,
                                std::vector<Fr> &coordinates) {
    auto d = log2exact(input.size());
    std::vector<Fr> coeff(gates.size());
    for (std::size_t j = 0; j < gates.size(); ++j)
        coeff[j] = gw[j] * gates[j].coefficient;
    GkrLayerProof p;
    std::vector<Fr> folded = input;
    for (unsigned bit = 0; bit < d; ++bit) {
        std::vector<Fr> msg(3, Fr(0));
        for (std::size_t j = 0; j < gates.size(); ++j) {
            auto &g = gates[j];
            auto a = g.left >> bit;
            for (unsigned x = 0; x < 3; ++x) {
                Fr u =
                    folded[2 * (a / 2)] + Fr(x) * (folded[2 * (a / 2) + 1] - folded[2 * (a / 2)]);
                Fr op = g.kind == GateKind::Add ? u + input[g.right] : u * input[g.right];
                msg[x] += coeff[j] * eqBit(Fr(x), a & 1) * op;
            }
        }
        auto r = absorbSumcheckRound(msg, t, label + "/" + std::to_string(bit));
        p.sumcheck.rounds.push_back(msg);
        coordinates.push_back(r);
        for (std::size_t j = 0; j < gates.size(); ++j)
            coeff[j] *= eqBit(r, (gates[j].left >> bit) & 1);
        fold(folded, r);
    }
    p.left = folded[0];
    folded = input;
    for (unsigned bit = 0; bit < d; ++bit) {
        std::vector<Fr> msg(3, Fr(0));
        for (std::size_t j = 0; j < gates.size(); ++j) {
            auto &g = gates[j];
            auto b = g.right >> bit;
            for (unsigned x = 0; x < 3; ++x) {
                Fr v =
                    folded[2 * (b / 2)] + Fr(x) * (folded[2 * (b / 2) + 1] - folded[2 * (b / 2)]);
                Fr op = g.kind == GateKind::Add ? p.left + v : p.left * v;
                msg[x] += coeff[j] * eqBit(Fr(x), b & 1) * op;
            }
        }
        auto r = absorbSumcheckRound(msg, t, label + "/" + std::to_string(d + bit));
        p.sumcheck.rounds.push_back(msg);
        coordinates.push_back(r);
        for (std::size_t j = 0; j < gates.size(); ++j)
            coeff[j] *= eqBit(r, (gates[j].right >> bit) & 1);
        fold(folded, r);
    }
    p.right = folded[0];
    return p;
}
#endif

static void endpoints(Fs &t, const GkrLayerProof &p) {
    t.scalar("gkr.left", p.left);
    t.scalar("gkr.right", p.right);
}
#ifndef R1_VERIFY_ONLY
GkrProof proveGkr(const GkrSpec &s, const std::vector<Fr> &input, Fs &t) {
    appendGkrStatement(s, t);
    auto v = evaluate(s, input);
    require(v.back() == s.output, "GKR public output not computation");
    auto r = point(t, log2exact(s.output.size()), "gkr.output.row");
    std::vector<InputClaim> claims{{r, mle(s.output, r)}};
    std::vector<Fr> lambda{Fr(1)};
    GkrProof p;
    for (std::size_t i = s.layers.size(); i > 0; --i) {
        auto gw = gateWeights(s.layers[i - 1].size(), claims, lambda);
        auto label = "gkr/layer/" + std::to_string(i);
        std::vector<Fr> coordinates;
        auto layer = proveLayer(s.layers[i - 1], v[i - 1], gw, t, label, coordinates);
        p.layers.push_back(std::move(layer));
        endpoints(t, p.layers.back());
        auto d = log2exact(v[i - 1].size());
        claims = {{{coordinates.begin(), coordinates.begin() + d}, p.layers.back().left},
                  {{coordinates.begin() + d, coordinates.end()}, p.layers.back().right}};
        if (i > 1) {
            auto mix = t.challenge("gkr.mix");
            lambda = {mix, Fr(1) - mix};
        }
    }
    return p;
}
#endif

std::vector<InputClaim> verifyGkr(const GkrSpec &s, const GkrProof &p, Fs &t) {
    appendGkrStatement(s, t);
    require(p.layers.size() == s.layers.size(), "GKR proof layer count");
    auto r = point(t, log2exact(s.output.size()), "gkr.output.row");
    std::vector<InputClaim> claims{{r, mle(s.output, r)}};
    std::vector<Fr> lambda{Fr(1)};
    for (std::size_t i = s.layers.size(), pi = 0; i > 0; --i, ++pi) {
        auto n = i == 1 ? s.input_size : s.layers[i - 2].size();
        auto d = log2exact(n);
        auto gw = gateWeights(s.layers[i - 1].size(), claims, lambda);
        Fr claim = 0;
        for (std::size_t j = 0; j < claims.size(); ++j)
            claim += lambda[j] * claims[j].value;
        auto end = verifySumcheck(2 * d, 2, claim, p.layers[pi].sumcheck, t,
                                  "gkr/layer/" + std::to_string(i));
        std::vector<Fr> u(end.point.begin(), end.point.begin() + d),
            v(end.point.begin() + d, end.point.end());
        auto eu = weights(u), ev = weights(v);
        Fr add = 0, mul = 0;
        for (std::size_t j = 0; j < s.layers[i - 1].size(); ++j) {
            auto &g = s.layers[i - 1][j];
            auto c = gw[j] * g.coefficient * eu[g.left] * ev[g.right];
            (g.kind == GateKind::Add ? add : mul) += c;
        }
        auto &layer = p.layers[pi];
        require(end.value == add * (layer.left + layer.right) + mul * layer.left * layer.right,
                "GKR gate endpoint");
        endpoints(t, layer);
        claims = {{u, layer.left}, {v, layer.right}};
        if (i > 1) {
            auto mix = t.challenge("gkr.mix");
            lambda = {mix, Fr(1) - mix};
        }
    }
    return claims;
}
} // namespace zkgpt::r1
