#include "division.hpp"
namespace zkgpt::r1 {
namespace {
#ifndef R1_VERIFY_ONLY
Fr powerOfTwo(unsigned b) {
    Fr v = 1;
    for (unsigned i = 0; i < b; ++i)
        v *= Fr(2);
    return v;
}
#endif
Fr active(std::size_t n, const std::vector<Fr> &r) {
    auto w = weights(r);
    Fr v = 0;
    for (std::size_t i = 0; i < n; ++i)
        v += w[i];
    return v;
}
RangeSpec helperRange(const DivisionSpec &s, unsigned k) {
    const std::array<std::string, 5> names{"Aenc", "qenc", "Bminus1", "S", "T"};
    require(k < 5, "division helper index");
    unsigned bits = k == 0   ? s.numerator_bits
                    : k == 1 ? s.quotient_bits
                    : k == 2 ? s.denominator_bits
                             : s.denominator_bits + 1;
    auto id = s.id + "/" + names[k];
    return {id, PCSpec::make(id, s.numerator.actual, s.numerator.padded, Layout::ColumnMajor),
            bits};
}
void statement(const DivisionSpec &s, const Commitment &a, const Commitment &b, const Commitment &q,
               const DivisionProof &p, Fs &t) {
    s.validate();
    require(a.spec == s.numerator && b.spec == s.denominator && q.spec == s.quotient,
            "division source PC");
    t.text("division.kind", "committed-positive-half-open-v1");
    t.text("division.id", s.id);
    t.integer("division.A_bits", s.numerator_bits);
    t.integer("division.q_bits", s.quotient_bits);
    t.integer("division.B_bits", s.denominator_bits);
    append(t, a);
    append(t, b);
    append(t, q);
    for (unsigned k = 0; k < 5; ++k) {
        require(p.helper[k].spec == helperRange(s, k).query, "division helper identity");
        append(t, p.helper[k]);
    }
}
Fr residual(unsigned kind, const Fr &a, const Fr &b, const Fr &q, const Fr &S, const Fr &T,
            const Fr &m) {
    if (kind == 0)
        return S - Fr(2) * a + Fr(2) * b * q - b;
    return T - Fr(2) * b + m + S;
}
} // namespace
void DivisionSpec::validate() const {
    numerator.validate();
    denominator.validate();
    quotient.validate();
    require(!id.empty() && id.size() <= 32, "division ID");
    require(numerator.actual == denominator.actual && numerator.actual == quotient.actual &&
                numerator.padded == denominator.padded && numerator.padded == quotient.padded,
            "division shape");
    require(numerator_bits >= 1 && numerator_bits <= 64 && quotient_bits >= 1 &&
                quotient_bits <= 64,
            "division signed domain");
    require(denominator_bits >= 1 && denominator_bits <= 62, "division denominator domain");
}
RangeSpec divisionHelperRange(const DivisionSpec &s, unsigned k) {
    s.validate();
    return helperRange(s, k);
}
#ifndef R1_VERIFY_ONLY
DivisionProof proveDivision(const DivisionSpec &s, const Commitment &ca, const Commitment &cb,
                            const Commitment &cq, const std::vector<Fr> &a,
                            const std::vector<Fr> &b, const std::vector<Fr> &q, Fs &t) {
    s.validate();
    require(a.size() == s.numerator.padded && b.size() == a.size() && q.size() == a.size(),
            "division witness shape");
    DivisionProof p;
    std::array<std::vector<Fr>, 5> h;
    for (auto &v : h)
        v.resize(a.size(), Fr(0));
    std::vector<Fr> mask(a.size(), Fr(0));
    for (std::size_t i = 0; i < s.numerator.actual; ++i) {
        mask[i] = Fr(1);
        h[0][i] = a[i] + powerOfTwo(s.numerator_bits - 1);
        h[1][i] = q[i] + powerOfTwo(s.quotient_bits - 1);
        h[2][i] = b[i] - Fr(1);
        h[3][i] = Fr(2) * a[i] - Fr(2) * b[i] * q[i] + b[i];
        h[4][i] = Fr(2) * b[i] - Fr(1) - h[3][i];
    }
    for (unsigned k = 0; k < 5; ++k)
        p.helper[k] = commit(helperRange(s, k).query, h[k]);
    statement(s, ca, cb, cq, p, t);
    for (unsigned k = 0; k < 5; ++k)
        p.range[k] = proveRange(helperRange(s, k), p.helper[k], h[k], t);
    p.signed_binding[0] =
        proveRangeBinding(helperRange(s, 0), s.numerator, ca, p.helper[0], a, h[0], true, t);
    p.signed_binding[1] =
        proveRangeBinding(helperRange(s, 1), s.quotient, cq, p.helper[1], q, h[1], true, t);
    auto r = point(t, log2exact(a.size()), "division.positive.row");
    p.positive_binding[0] = open(s.denominator, cb, b, r, t, "division.positive.B");
    p.positive_binding[1] =
        open(helperRange(s, 2).query, p.helper[2], h[2], r, t, "division.positive.Bminus1");
    require(p.positive_binding[0].value ==
                p.positive_binding[1].value + active(s.numerator.actual, r),
            "division positive denominator binding");
    for (unsigned kind = 0; kind < 2; ++kind) {
        auto row = point(t, log2exact(a.size()), "division.row/" + std::to_string(kind));
        auto end = proveSumcheck(
            {weights(row), a, b, q, h[3], h[4], mask}, 3,
            [&](const auto &v) {
                return v[0] * residual(kind, v[1], v[2], v[3], v[4], v[5], v[6]);
            },
            p.constraints[kind], t, "division.constraint/" + std::to_string(kind));
        auto &ops = p.endpoint[kind];
        ops[0] = open(s.numerator, ca, a, end.point, t, "division.A");
        ops[1] = open(s.denominator, cb, b, end.point, t, "division.B");
        ops[2] = open(s.quotient, cq, q, end.point, t, "division.q");
        ops[3] = open(helperRange(s, 3).query, p.helper[3], h[3], end.point, t, "division.S");
        ops[4] = open(helperRange(s, 4).query, p.helper[4], h[4], end.point, t, "division.T");
    }
    return p;
}
#endif
void verifyDivision(const DivisionSpec &s, const Commitment &ca, const Commitment &cb,
                    const Commitment &cq, const DivisionProof &p, Fs &t) {
    statement(s, ca, cb, cq, p, t);
    for (unsigned k = 0; k < 5; ++k)
        verifyRange(helperRange(s, k), p.helper[k], p.range[k], t);
    verifyRangeBinding(helperRange(s, 0), s.numerator, ca, p.helper[0], true, p.signed_binding[0],
                       t);
    verifyRangeBinding(helperRange(s, 1), s.quotient, cq, p.helper[1], true, p.signed_binding[1],
                       t);
    auto r = point(t, log2exact(s.numerator.padded), "division.positive.row");
    verify(s.denominator, cb, r, p.positive_binding[0], t, "division.positive.B");
    verify(helperRange(s, 2).query, p.helper[2], r, p.positive_binding[1], t,
           "division.positive.Bminus1");
    require(p.positive_binding[0].value ==
                p.positive_binding[1].value + active(s.numerator.actual, r),
            "division positive denominator binding");
    for (unsigned kind = 0; kind < 2; ++kind) {
        auto row = point(t, log2exact(s.numerator.padded), "division.row/" + std::to_string(kind));
        auto end = verifySumcheck(log2exact(s.numerator.padded), 3, Fr(0), p.constraints[kind], t,
                                  "division.constraint/" + std::to_string(kind));
        const auto &ops = p.endpoint[kind];
        verify(s.numerator, ca, end.point, ops[0], t, "division.A");
        verify(s.denominator, cb, end.point, ops[1], t, "division.B");
        verify(s.quotient, cq, end.point, ops[2], t, "division.q");
        verify(helperRange(s, 3).query, p.helper[3], end.point, ops[3], t, "division.S");
        verify(helperRange(s, 4).query, p.helper[4], end.point, ops[4], t, "division.T");
        require(end.value ==
                    equality(row, end.point) * residual(kind, ops[0].value, ops[1].value,
                                                        ops[2].value, ops[3].value, ops[4].value,
                                                        active(s.numerator.actual, end.point)),
                "division constraint endpoint");
    }
}
} // namespace zkgpt::r1
