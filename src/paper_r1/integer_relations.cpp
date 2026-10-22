#include "integer_relations.hpp"
namespace zkgpt::r1 {
namespace {
#ifndef R1_VERIFY_ONLY
Fr twoPower(unsigned bits) {
    Fr out = 1;
    for (unsigned i = 0; i < bits; ++i)
        out *= Fr(2);
    return out;
}
#endif

Fr mask(std::size_t actual, const std::vector<Fr> &r) {
    auto w = weights(r);
    Fr out = 0;
    for (std::size_t i = 0; i < actual; ++i)
        out += w[i];
    return out;
}
unsigned slackBits(std::uint64_t denominator) {
    std::uint64_t bound = denominator * 2, p = 1;
    unsigned bits = 0;
    while (p < bound) {
        p *= 2;
        ++bits;
    }
    return bits;
}
RangeSpec roundRange(const RoundingSpec &s, unsigned k) {
    s.validate();
    const std::array<std::string, 4> name{"Aenc", "qenc", "S", "T"};
    unsigned bits = k == 0 ? s.numerator_bits : k == 1 ? s.quotient_bits : slackBits(s.denominator);
    return {s.id + "/" + name[k],
            PCSpec::make(s.id + "/" + name[k], s.numerator.actual, s.numerator.padded,
                         Layout::ColumnMajor),
            bits};
}
void roundStatement(const RoundingSpec &s, const Commitment &a, const Commitment &q,
                    const RoundingProof &p, Fs &t) {
    s.validate();
    require(a.spec == s.numerator && q.spec == s.quotient, "round source PC");
    t.text("integer.kind", "half-open-rounding-v1");
    t.text("integer.id", s.id);
    t.integer("integer.denominator", s.denominator);
    t.integer("integer.A_bits", s.numerator_bits);
    t.integer("integer.q_bits", s.quotient_bits);
    append(t, a);
    append(t, q);
    for (unsigned k = 0; k < 4; ++k) {
        require(p.helper[k].spec == roundRange(s, k).query, "round helper identity");
        append(t, p.helper[k]);
    }
}
RangeSpec maxRange(const MaximumSpec &s, unsigned k) {
    s.validate();
    const std::array<std::string, 3> name{"score", "maximum", "gap"};
    const auto &parent = k == 1 ? s.maximum : s.scores;
    return {s.id + "/" + name[k],
            PCSpec::make(s.id + "/" + name[k], parent.actual, parent.padded, Layout::ColumnMajor),
            s.bits};
}
void maxStatement(const MaximumSpec &s, const Commitment &scores, const Commitment &maximum,
                  const MaximumProof &p, Fs &t) {
    s.validate();
    require(scores.spec == s.scores && maximum.spec == s.maximum, "maximum source PC");
    t.text("integer.kind", "exact-active-maximum-v1");
    t.text("integer.id", s.id);
    t.integer("integer.bits", s.bits);
    append(t, scores);
    append(t, maximum);
    for (unsigned k = 0; k < 3; ++k) {
        require(p.helper[k].spec == maxRange(s, k).query, "maximum helper identity");
        append(t, p.helper[k]);
    }
}
} // namespace
void RoundingSpec::validate() const {
    numerator.validate();
    quotient.validate();
    require(!id.empty() && id.size() <= 40, "round ID");
    require(numerator.actual == quotient.actual && numerator.padded == quotient.padded,
            "round query shape");
    require(denominator > 0 && denominator <= (std::uint64_t(1) << 62), "round denominator domain");
    require(numerator_bits >= 1 && numerator_bits <= 64 && quotient_bits >= 1 &&
                quotient_bits <= 64,
            "round signed domain");
}
#ifndef R1_VERIFY_ONLY
RoundingProof proveRounding(const RoundingSpec &s, const Commitment &ca, const Commitment &cq,
                            const std::vector<Fr> &a, const std::vector<Fr> &q, Fs &t) {
    s.validate();
    require(a.size() == s.numerator.padded && q.size() == a.size(), "round witness shape");
    RoundingProof p;
    std::array<std::vector<Fr>, 4> h;
    for (auto &v : h)
        v.resize(a.size(), Fr(0));
    Fr B = Fr(s.denominator);
    for (std::size_t i = 0; i < s.numerator.actual; ++i) {
        h[0][i] = a[i] + twoPower(s.numerator_bits - 1);
        h[1][i] = q[i] + twoPower(s.quotient_bits - 1);
        h[2][i] = Fr(2) * a[i] - Fr(2) * B * q[i] + B;
        h[3][i] = Fr(2) * B - Fr(1) - h[2][i];
    }
    for (unsigned k = 0; k < 4; ++k)
        p.helper[k] = commit(roundRange(s, k).query, h[k]);
    roundStatement(s, ca, cq, p, t);
    for (unsigned k = 0; k < 4; ++k)
        p.range[k] = proveRange(roundRange(s, k), p.helper[k], h[k], t);
    p.signed_binding[0] =
        proveRangeBinding(roundRange(s, 0), s.numerator, ca, p.helper[0], a, h[0], true, t);
    p.signed_binding[1] =
        proveRangeBinding(roundRange(s, 1), s.quotient, cq, p.helper[1], q, h[1], true, t);
    auto r = point(t, log2exact(a.size()), "round.relation.row");
    p.relation[0] = open(s.numerator, ca, a, r, t, "round.A");
    p.relation[1] = open(s.quotient, cq, q, r, t, "round.q");
    p.relation[2] = open(roundRange(s, 2).query, p.helper[2], h[2], r, t, "round.S");
    p.relation[3] = open(roundRange(s, 3).query, p.helper[3], h[3], r, t, "round.T");
    return p;
}
#endif
void verifyRounding(const RoundingSpec &s, const Commitment &ca, const Commitment &cq,
                    const RoundingProof &p, Fs &t) {
    roundStatement(s, ca, cq, p, t);
    for (unsigned k = 0; k < 4; ++k)
        verifyRange(roundRange(s, k), p.helper[k], p.range[k], t);
    verifyRangeBinding(roundRange(s, 0), s.numerator, ca, p.helper[0], true, p.signed_binding[0],
                       t);
    verifyRangeBinding(roundRange(s, 1), s.quotient, cq, p.helper[1], true, p.signed_binding[1], t);
    auto r = point(t, log2exact(s.numerator.padded), "round.relation.row");
    verify(s.numerator, ca, r, p.relation[0], t, "round.A");
    verify(s.quotient, cq, r, p.relation[1], t, "round.q");
    verify(roundRange(s, 2).query, p.helper[2], r, p.relation[2], t, "round.S");
    verify(roundRange(s, 3).query, p.helper[3], r, p.relation[3], t, "round.T");
    Fr B = Fr(s.denominator), active = mask(s.numerator.actual, r);
    require(p.relation[2].value ==
                Fr(2) * p.relation[0].value - Fr(2) * B * p.relation[1].value + B * active,
            "half-open rounding slack");
    require(p.relation[3].value == (Fr(2) * B - Fr(1)) * active - p.relation[2].value,
            "half-open rounding upper endpoint");
}
void MaximumSpec::validate() const {
    scores.validate();
    maximum.validate();
    require(!id.empty() && id.size() <= 40, "maximum ID");
    require(maximum.actual == 1 && maximum.padded == 1, "maximum scalar PC");
    require(bits >= 1 && bits <= 64, "maximum signed domain");
}
#ifndef R1_VERIFY_ONLY
MaximumProof proveMaximum(const MaximumSpec &s, const Commitment &cx, const Commitment &cm,
                          const std::vector<Fr> &x, const Fr &m, Fs &t) {
    s.validate();
    require(x.size() == s.scores.padded, "maximum witness shape");
    MaximumProof p;
    std::array<std::vector<Fr>, 3> h;
    h[0] = std::vector<Fr>(x.size(), Fr(0));
    h[1] = {m + twoPower(s.bits - 1)};
    h[2] = std::vector<Fr>(x.size(), Fr(0));
    for (std::size_t i = 0; i < s.scores.actual; ++i) {
        h[0][i] = x[i] + twoPower(s.bits - 1);
        h[2][i] = m - x[i];
    }
    for (unsigned k = 0; k < 3; ++k)
        p.helper[k] = commit(maxRange(s, k).query, h[k]);
    maxStatement(s, cx, cm, p, t);
    for (unsigned k = 0; k < 3; ++k)
        p.range[k] = proveRange(maxRange(s, k), p.helper[k], h[k], t);
    p.signed_binding[0] =
        proveRangeBinding(maxRange(s, 0), s.scores, cx, p.helper[0], x, h[0], true, t);
    p.signed_binding[1] =
        proveRangeBinding(maxRange(s, 1), s.maximum, cm, p.helper[1], {m}, h[1], true, t);
    auto r = point(t, log2exact(x.size()), "maximum.relation.row");
    p.relation[0] = open(s.scores, cx, x, r, t, "maximum.scores");
    p.relation[1] = open(s.maximum, cm, {m}, {}, t, "maximum.value");
    p.relation[2] = open(maxRange(s, 2).query, p.helper[2], h[2], r, t, "maximum.gap");
    auto leaves = h[2];
    for (std::size_t i = s.scores.actual; i < leaves.size(); ++i)
        leaves[i] = Fr(1);
    require(product(leaves).isZero(), "maximum must attain an active score");
    t.scalar("maximum.product.root", Fr(0));
    auto end = proveProduct(leaves, p.zero_product, t, "maximum.zero-product");
    p.product_leaf =
        open(maxRange(s, 2).query, p.helper[2], h[2], end.point, t, "maximum.product.leaf");
    return p;
}
#endif
void verifyMaximum(const MaximumSpec &s, const Commitment &cx, const Commitment &cm,
                   const MaximumProof &p, Fs &t) {
    maxStatement(s, cx, cm, p, t);
    for (unsigned k = 0; k < 3; ++k)
        verifyRange(maxRange(s, k), p.helper[k], p.range[k], t);
    verifyRangeBinding(maxRange(s, 0), s.scores, cx, p.helper[0], true, p.signed_binding[0], t);
    verifyRangeBinding(maxRange(s, 1), s.maximum, cm, p.helper[1], true, p.signed_binding[1], t);
    auto r = point(t, log2exact(s.scores.padded), "maximum.relation.row");
    verify(s.scores, cx, r, p.relation[0], t, "maximum.scores");
    verify(s.maximum, cm, {}, p.relation[1], t, "maximum.value");
    verify(maxRange(s, 2).query, p.helper[2], r, p.relation[2], t, "maximum.gap");
    require(p.relation[2].value ==
                p.relation[1].value * mask(s.scores.actual, r) - p.relation[0].value,
            "maximum gap source binding");
    t.scalar("maximum.product.root", Fr(0));
    auto end = verifyProduct(s.scores.padded, Fr(0), p.zero_product, t, "maximum.zero-product");
    verify(maxRange(s, 2).query, p.helper[2], end.point, p.product_leaf, t, "maximum.product.leaf");
    require(end.value == p.product_leaf.value + Fr(1) - mask(s.scores.actual, end.point),
            "maximum active product leaf");
}
} // namespace zkgpt::r1
