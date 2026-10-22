#include "input.hpp"
namespace zkgpt::r1 {
void InputSpec::validate() const {
    log2exact(x.size());
    require(!context.empty() && context.size() <= 4096, "input public context");
    for (unsigned b = 0; b < 3; ++b) {
        spec[b].validate();
        require(spec[b].padded == x.size() && commitment[b].spec == spec[b],
                "input block identity/size");
    }
}
void appendInputStatement(const InputSpec &s, Fs &t) {
    s.validate();
    t.text("input.protocol", "four-block-MLE-algebraic-correction-v1");
    t.text("input.context", s.context);
    t.integer("input.N", s.x.size());
    for (auto &f : s.x)
        t.scalar("input.public.X", f);
    for (auto &c : s.commitment)
        append(t, c);
}
static std::vector<Fr> batch(const InputSpec &s, const std::vector<InputClaim> &claims, Fs &t) {
    appendInputStatement(s, t);
    require(!claims.empty() && claims.size() <= 4096, "input claim count");
    auto d = log2exact(s.x.size());
    t.integer("input.claims", claims.size());
    for (auto &q : claims) {
        require(q.point.size() == d + 2, "input claim dimension");
        for (auto &r : q.point)
            t.scalar("input.claim.point", r);
        t.scalar("input.claim.value", q.value);
    }
    std::vector<Fr> lambda;
    for (std::size_t j = 0; j < claims.size(); ++j)
        lambda.push_back(t.challenge("input.batch/" + std::to_string(j)));
    return lambda;
}
static Fr selector(unsigned block, const Fr &b, const Fr &a) {
    return (block & 1 ? b : Fr(1) - b) * (block & 2 ? a : Fr(1) - a);
}
#ifndef R1_VERIFY_ONLY
InputProof proveInput(const InputSpec &s, const std::array<std::vector<Fr>, 3> &w,
                      const std::vector<InputClaim> &claims, Fs &t) {
    auto lambda = batch(s, claims, t);
    auto n = s.x.size();
    auto d = log2exact(n);
    std::vector<std::vector<Fr>> factors(8, std::vector<Fr>(n, Fr(0)));
    for (unsigned block = 0; block < 4; ++block) {
        if (block)
            require(w[block - 1].size() == n, "input witness size");
        factors[2 * block + 1] = block ? w[block - 1] : s.x;
    }
    for (std::size_t j = 0; j < claims.size(); ++j) {
        const auto &r = claims[j].point;
        auto e = weights({r.begin(), r.begin() + d});
        for (unsigned block = 0; block < 4; ++block) {
            auto c = lambda[j] * selector(block, r[d], r[d + 1]);
            for (std::size_t i = 0; i < n; ++i)
                factors[2 * block][i] += c * e[i];
        }
    }
    Fr expected = 0, actual = 0;
    for (std::size_t j = 0; j < claims.size(); ++j)
        expected += lambda[j] * claims[j].value;
    for (unsigned b = 0; b < 4; ++b)
        actual += dot(factors[2 * b], factors[2 * b + 1]);
    require(expected == actual, "input witness claims");
    InputProof p;
    auto end = proveSumcheck(
        factors, 2,
        [](const auto &v) {
            Fr out = 0;
            for (unsigned b = 0; b < 4; ++b)
                out += v[2 * b] * v[2 * b + 1];
            return out;
        },
        p.local, t, "input.local");
    std::vector<Fr> k(4), v(4);
    for (unsigned b = 0; b < 4; ++b) {
        k[b] = mle(factors[2 * b], end.point);
        v[b] = mle(factors[2 * b + 1], end.point);
    }
    proveSumcheck(
        {k, v}, 2, [](const auto &q) { return q[0] * q[1]; }, p.selectors, t, "input.selectors");
    for (unsigned b = 0; b < 3; ++b)
        p.opening[b] = open(s.spec[b], s.commitment[b], w[b], end.point, t,
                            "input.final/" + std::to_string(b));
    for (unsigned b = 0; b < 3; ++b)
        p.padding[b] = proveZeroTail(s.spec[b], s.commitment[b], w[b], t);
    return p;
}
#endif

void verifyInput(const InputSpec &s, const std::vector<InputClaim> &claims, const InputProof &p,
                 Fs &t) {
    auto lambda = batch(s, claims, t);
    Fr initial = 0;
    for (std::size_t j = 0; j < claims.size(); ++j)
        initial += lambda[j] * claims[j].value;
    auto local = verifySumcheck(log2exact(s.x.size()), 2, initial, p.local, t, "input.local");
    auto end = verifySumcheck(2, 2, local.value, p.selectors, t, "input.selectors");
    std::array<Fr, 4> value;
    value[0] = mle(s.x, local.point);
    for (unsigned b = 0; b < 3; ++b) {
        verify(s.spec[b], s.commitment[b], local.point, p.opening[b], t,
               "input.final/" + std::to_string(b));
        value[b + 1] = p.opening[b].value;
    }
    std::vector<Fr> full = local.point;
    full.insert(full.end(), end.point.begin(), end.point.end());
    Fr kernel = 0;
    for (std::size_t j = 0; j < claims.size(); ++j)
        kernel += lambda[j] * equality(claims[j].point, full);
    Fr input = 0;
    for (unsigned b = 0; b < 4; ++b)
        input += selector(b, end.point[0], end.point[1]) * value[b];
    require(end.value == kernel * input, "four-block input endpoint");
    for (unsigned b = 0; b < 3; ++b)
        verifyZeroTail(s.spec[b], s.commitment[b], p.padding[b], t);
}
} // namespace zkgpt::r1
