#include "matrix.hpp"
namespace zkgpt::r1 {
void MatrixSpec::validate() const {
    log2exact(rows);
    log2exact(inner);
    log2exact(columns);
    require(rows <= kMaxVector / inner && inner <= kMaxVector / columns &&
                rows <= kMaxVector / columns,
            "matrix size overflow/bound");
    a.validate();
    b.validate();
    require(a.padded == rows * inner && b.padded == inner * columns && a.actual == a.padded &&
                b.actual == b.padded,
            "matrix PC dimensions/full rectangular domain");
    require(actual_rows > 0 && actual_rows <= rows && actual_inner > 0 && actual_inner <= inner &&
                actual_columns > 0 && actual_columns <= columns,
            "matrix actual dimensions");
    require(!id.empty() && id.size() <= 128, "matrix ID");
}
static void statement(const MatrixSpec &s, const Commitment &ca, const Commitment &cb,
                      const std::vector<Fr> &y, Fs &t) {
    s.validate();
    require(ca.spec == s.a && cb.spec == s.b && y.size() == s.rows * s.columns,
            "matrix public statement");
    for (std::size_t i = 0; i < s.rows; ++i)
        for (std::size_t j = 0; j < s.columns; ++j)
            if (i >= s.actual_rows || j >= s.actual_columns)
                require(y[i * s.columns + j].isZero(), "matrix public output padding");
    t.text("matrix.kind", "dedicated-matrix-sumcheck-v1");
    t.text("matrix.id", s.id);
    for (auto n : {s.rows, s.inner, s.columns, s.actual_rows, s.actual_inner, s.actual_columns})
        t.integer("matrix.dimension", n);
    append(t, ca);
    append(t, cb);
    for (auto &f : y)
        t.scalar("matrix.public.C", f);
}
static std::vector<Fr> padWeights(const MatrixSpec &s, unsigned which, Fs &t) {
    auto n = which ? s.b.padded : s.a.padded;
    auto c = weights(point(t, log2exact(n), "matrix.padding/" + std::to_string(which)));
    for (std::size_t i = 0; i < n; ++i) {
        bool active = which ? (i / s.columns < s.actual_inner && i % s.columns < s.actual_columns)
                            : (i / s.inner < s.actual_rows && i % s.inner < s.actual_inner);
        if (active)
            c[i] = Fr(0);
    }
    return c;
}
#ifndef R1_VERIFY_ONLY
MatrixProof proveMatrix(const MatrixSpec &s, const Commitment &ca, const Commitment &cb,
                        const std::vector<Fr> &a, const std::vector<Fr> &b,
                        const std::vector<Fr> &y, Fs &t) {
    statement(s, ca, cb, y, t);
    require(a.size() == s.a.padded && b.size() == s.b.padded, "matrix witness shape");
    MatrixProof p;
    p.padding[0] = proveLinear(s.a, ca, a, padWeights(s, 0, t), Fr(0), t, "matrix.padding.A");
    p.padding[1] = proveLinear(s.b, cb, b, padWeights(s, 1, t), Fr(0), t, "matrix.padding.B");
    auto col = point(t, log2exact(s.columns), "matrix.output.columns"),
         row = point(t, log2exact(s.rows), "matrix.output.rows");
    auto rw = weights(row), cw = weights(col);
    std::vector<Fr> av(s.inner, Fr(0)), bv(s.inner, Fr(0));
    for (std::size_t k = 0; k < s.inner; ++k) {
        for (std::size_t i = 0; i < s.rows; ++i)
            av[k] += rw[i] * a[i * s.inner + k];
        for (std::size_t j = 0; j < s.columns; ++j)
            bv[k] += cw[j] * b[k * s.columns + j];
    }
    auto r = col;
    r.insert(r.end(), row.begin(), row.end());
    require(dot(av, bv) == mle(y, r), "matrix output computation");
    auto end = proveSumcheck(
        {av, bv}, 2, [](const auto &v) { return v[0] * v[1]; }, p.sumcheck, t, "matrix.inner");
    auto ar = end.point;
    ar.insert(ar.end(), row.begin(), row.end());
    auto br = col;
    br.insert(br.end(), end.point.begin(), end.point.end());
    p.a = open(s.a, ca, a, ar, t, "matrix.A.endpoint");
    p.b = open(s.b, cb, b, br, t, "matrix.B.endpoint");
    return p;
}
#endif
void verifyMatrix(const MatrixSpec &s, const Commitment &ca, const Commitment &cb,
                  const std::vector<Fr> &y, const MatrixProof &p, Fs &t) {
    statement(s, ca, cb, y, t);
    verifyLinear(s.a, ca, padWeights(s, 0, t), Fr(0), p.padding[0], t, "matrix.padding.A");
    verifyLinear(s.b, cb, padWeights(s, 1, t), Fr(0), p.padding[1], t, "matrix.padding.B");
    auto col = point(t, log2exact(s.columns), "matrix.output.columns"),
         row = point(t, log2exact(s.rows), "matrix.output.rows");
    auto r = col;
    r.insert(r.end(), row.begin(), row.end());
    auto end = verifySumcheck(log2exact(s.inner), 2, mle(y, r), p.sumcheck, t, "matrix.inner");
    auto ar = end.point;
    ar.insert(ar.end(), row.begin(), row.end());
    auto br = col;
    br.insert(br.end(), end.point.begin(), end.point.end());
    verify(s.a, ca, ar, p.a, t, "matrix.A.endpoint");
    verify(s.b, cb, br, p.b, t, "matrix.B.endpoint");
    require(end.value == p.a.value * p.b.value, "matrix sumcheck endpoint");
}
} // namespace zkgpt::r1
