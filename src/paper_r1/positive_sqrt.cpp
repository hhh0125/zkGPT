#include "positive_sqrt.hpp"
namespace zkgpt::r1 {
namespace {
Fr active(std::size_t n, const std::vector<Fr> &r) {
    auto w = weights(r);
    Fr v = 0;
    for (std::size_t i = 0; i < n; ++i)
        v += w[i];
    return v;
}
PCSpec helperSpec(const PositiveSqrtSpec &s, unsigned k) {
    const std::array<std::string, 3> names{"root-minus1", "S", "T"};
    require(k < 3, "sqrt helper index");
    return PCSpec::make(s.id + "/" + names[k], s.input.actual, s.input.padded, Layout::ColumnMajor);
}
RangeSpec rangeSpec(const PositiveSqrtSpec &s, unsigned k) {
    require(k < 4, "sqrt Range index");
    unsigned bits = k == 0 ? s.input_bits : k == 1 ? s.root_bits : s.root_bits + 1;
    return {s.id + "/range/" + std::to_string(k), k == 0 ? s.input : helperSpec(s, k - 1), bits};
}
void statement(const PositiveSqrtSpec &s, const Commitment &x, const Commitment &y,
               const PositiveSqrtProof &p, Fs &t) {
    s.validate();
    require(x.spec == s.input && y.spec == s.root, "sqrt source PC");
    t.text("sqrt.kind", "positive-integer-half-open-v1");
    t.text("sqrt.id", s.id);
    t.integer("sqrt.input_bits", s.input_bits);
    t.integer("sqrt.root_bits", s.root_bits);
    append(t, x);
    append(t, y);
    for (unsigned k = 0; k < 3; ++k) {
        require(p.helper[k].spec == helperSpec(s, k), "sqrt helper identity");
        append(t, p.helper[k]);
    }
}
Fr residual(unsigned kind, const Fr &x, const Fr &y, const Fr &h, const Fr &mask) {
    if (kind == 0)
        return h - x + y * y - y + mask;
    return h - y * y - y + x;
}
} // namespace
void PositiveSqrtSpec::validate() const {
    input.validate();
    root.validate();
    require(!id.empty() && id.size() <= 32, "sqrt ID");
    require(input.actual == root.actual && input.padded == root.padded, "sqrt shape");
    require(input_bits >= 1 && input_bits <= 64 && root_bits >= 1 && root_bits <= 63,
            "sqrt integer domain");
}
PCSpec positiveSqrtHelperSpec(const PositiveSqrtSpec &s, unsigned k) {
    s.validate();
    return helperSpec(s, k);
}
RangeSpec positiveSqrtRangeSpec(const PositiveSqrtSpec &s, unsigned k) {
    s.validate();
    return rangeSpec(s, k);
}
#ifndef R1_VERIFY_ONLY
PositiveSqrtProof provePositiveSqrt(const PositiveSqrtSpec &s, const Commitment &cx,
                                    const Commitment &cy, const std::vector<Fr> &x,
                                    const std::vector<Fr> &y, Fs &t) {
    s.validate();
    require(x.size() == s.input.padded && y.size() == x.size(), "sqrt witness shape");
    PositiveSqrtProof p;
    std::array<std::vector<Fr>, 3> h;
    for (auto &v : h)
        v.resize(x.size(), Fr(0));
    std::vector<Fr> mask(x.size(), Fr(0));
    for (std::size_t i = 0; i < s.input.actual; ++i) {
        mask[i] = Fr(1);
        h[0][i] = y[i] - Fr(1);
        h[1][i] = x[i] - y[i] * y[i] + y[i] - Fr(1);
        h[2][i] = y[i] * y[i] + y[i] - x[i];
    }
    for (unsigned k = 0; k < 3; ++k)
        p.helper[k] = commit(helperSpec(s, k), h[k]);
    statement(s, cx, cy, p, t);
    p.range[0] = proveRange(rangeSpec(s, 0), cx, x, t);
    for (unsigned k = 1; k < 4; ++k)
        p.range[k] = proveRange(rangeSpec(s, k), p.helper[k - 1], h[k - 1], t);
    auto r = point(t, log2exact(x.size()), "sqrt.positive.row");
    p.positive_binding[0] = open(s.root, cy, y, r, t, "sqrt.root");
    p.positive_binding[1] = open(helperSpec(s, 0), p.helper[0], h[0], r, t, "sqrt.root-minus1");
    require(p.positive_binding[0].value == p.positive_binding[1].value + active(s.input.actual, r),
            "sqrt positive root binding");
    for (unsigned kind = 0; kind < 2; ++kind) {
        auto row = point(t, log2exact(x.size()), "sqrt.row/" + std::to_string(kind));
        auto end = proveSumcheck(
            {weights(row), x, y, h[kind + 1], mask}, 3,
            [&](const auto &v) { return v[0] * residual(kind, v[1], v[2], v[3], v[4]); },
            p.constraints[kind], t, "sqrt.constraint/" + std::to_string(kind));
        p.endpoint[kind][0] = open(s.input, cx, x, end.point, t, "sqrt.constraint.x");
        p.endpoint[kind][1] = open(s.root, cy, y, end.point, t, "sqrt.constraint.y");
        p.endpoint[kind][2] = open(helperSpec(s, kind + 1), p.helper[kind + 1], h[kind + 1],
                                   end.point, t, "sqrt.constraint.slack");
    }
    return p;
}
#endif
void verifyPositiveSqrt(const PositiveSqrtSpec &s, const Commitment &cx, const Commitment &cy,
                        const PositiveSqrtProof &p, Fs &t) {
    statement(s, cx, cy, p, t);
    verifyRange(rangeSpec(s, 0), cx, p.range[0], t);
    for (unsigned k = 1; k < 4; ++k)
        verifyRange(rangeSpec(s, k), p.helper[k - 1], p.range[k], t);
    auto r = point(t, log2exact(s.input.padded), "sqrt.positive.row");
    verify(s.root, cy, r, p.positive_binding[0], t, "sqrt.root");
    verify(helperSpec(s, 0), p.helper[0], r, p.positive_binding[1], t, "sqrt.root-minus1");
    require(p.positive_binding[0].value == p.positive_binding[1].value + active(s.input.actual, r),
            "sqrt positive root binding");
    for (unsigned kind = 0; kind < 2; ++kind) {
        auto row = point(t, log2exact(s.input.padded), "sqrt.row/" + std::to_string(kind));
        auto end = verifySumcheck(log2exact(s.input.padded), 3, Fr(0), p.constraints[kind], t,
                                  "sqrt.constraint/" + std::to_string(kind));
        const auto &ops = p.endpoint[kind];
        verify(s.input, cx, end.point, ops[0], t, "sqrt.constraint.x");
        verify(s.root, cy, end.point, ops[1], t, "sqrt.constraint.y");
        verify(helperSpec(s, kind + 1), p.helper[kind + 1], end.point, ops[2], t,
               "sqrt.constraint.slack");
        require(end.value == equality(row, end.point) * residual(kind, ops[0].value, ops[1].value,
                                                                 ops[2].value,
                                                                 active(s.input.actual, end.point)),
                "sqrt constraint endpoint");
    }
}
} // namespace zkgpt::r1
