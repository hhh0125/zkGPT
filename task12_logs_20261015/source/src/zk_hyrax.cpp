#include "thread_joiner.hpp"
#include "zk_hyrax.hpp"
#include <algorithm>
#include <thread>

namespace zkgpt::zk {
namespace {
void requireLayout(Layout l) {
    if (l != Layout::RowMajor && l != Layout::ColumnMajor)
        throw std::invalid_argument("invalid matrix layout");
}
std::size_t offset(const MatrixShape &s, Layout l, size_t r, size_t c) {
    return l == Layout::RowMajor ? r * s.columns + c : r + c * s.rows;
}
template <class Body> void parallelColumns(size_t count, Body body) {
    const size_t threads = std::min<size_t>(count < 64 ? 1 : zkgpt::workerThreadCount(), count);
    if (threads <= 1) {
        body(0, count);
        return;
    }
    std::vector<std::thread> workers;
    zkgpt::ThreadJoiner joiner(workers);
    workers.reserve(threads);
    for (size_t i = 0; i < threads; ++i)
        workers.emplace_back(body, count * i / threads, count * (i + 1) / threads);
    joiner.join();
}
G1 msm(const std::vector<G1> &g, const std::vector<Fr> &w) {
    if (g.size() != w.size() || w.empty())
        throw std::invalid_argument("MSM dimensions");
    G1 c;
    G1::mulVec(c, const_cast<G1 *>(g.data()), w.data(), w.size());
    return c;
}
Fr dot(const std::vector<Fr> &a, const std::vector<Fr> &b) {
    if (a.size() != b.size())
        throw std::invalid_argument("dot dimensions");
    Fr v = 0;
    for (size_t i = 0; i < a.size(); ++i)
        v += a[i] * b[i];
    return v;
}
struct PublicReduction {
    MatrixShape shape;
    G1 C;
    std::vector<Fr> rows, mu;
};
PublicReduction reduction(const Parameters &p, const MatrixCommitment &c,
                          const std::vector<Fr> &point) {
    auto shape = matrixShape(c.size);
    requireLayout(c.layout);
    if (c.rows.size() != shape.rows || point.size() != shape.log_size ||
        p.G.size() != shape.columns)
        throw std::invalid_argument("Hyrax statement dimensions");
    for (const auto &x : point)
        requireScalar(x);
    for (const auto &row : c.rows)
        requirePoint(row);
    auto rows = hyraxMleEqualityWeights(
        point, c.layout == Layout::RowMajor ? shape.log_size - shape.log_size / 2 : 0,
        shape.log_size / 2);
    auto mu = hyraxMleEqualityWeights(point, c.layout == Layout::RowMajor ? 0 : shape.log_size / 2,
                                      shape.log_size - shape.log_size / 2);
    auto C = msm(c.rows, rows);
    return {shape, C, std::move(rows), std::move(mu)};
}
void bindMle(const Parameters &p, const MleStatement &s, const PublicReduction &r, Transcript &t,
             const std::string &label) {
    requirePoint(s.evaluation);
    bindContext(t, p, "hyrax-mle", label, s.context);
    t.appendU64("size", s.commitment.size);
    t.appendU64("rows", r.shape.rows);
    t.appendU64("columns", r.shape.columns);
    t.appendU64("layout", static_cast<uint8_t>(s.commitment.layout));
    t.appendU64("point.length", s.point.size());
    for (const auto &a : s.point)
        t.appendFr("point", a);
    t.appendU64("rows.length", s.commitment.rows.size());
    for (const auto &a : s.commitment.rows)
        t.appendG1("row", a);
    t.appendG1("C*", r.C);
    t.appendG1("Dy", s.evaluation);
}
void bindLinear(const Parameters &p, const detail::LinearStatement &s, Transcript &t,
                const std::string &label) {
    validateParameters(p);
    if (s.coefficients.size() != p.G.size())
        throw std::invalid_argument("linear coefficient dimensions");
    requirePoint(s.commitment);
    requirePoint(s.evaluation);
    for (const auto &x : s.coefficients)
        requireScalar(x);
    bindContext(t, p, "hyrax-collapsed-linear", label, s.context);
    t.appendU64("columns", s.coefficients.size());
    t.appendG1("C*", s.commitment);
    t.appendG1("Dy", s.evaluation);
    for (const auto &x : s.coefficients)
        t.appendFr("mu", x);
}
Fr linearChallenge(Transcript &t, const LinearProof &p) {
    requirePoint(p.A_C);
    requirePoint(p.A_y);
    t.appendG1("A_C", p.A_C);
    t.appendG1("A_y", p.A_y);
    return t.challengeUniform("hyrax-e", false);
}
std::vector<Fr> collapse(HyraxFrWitnessView w, Layout layout, const PublicReduction &r) {
    if (w.size() != r.shape.size)
        throw std::invalid_argument("matrix witness size");
    for (size_t i = 0; i < w.size(); ++i)
        requireScalar(w[i]);
    std::vector<Fr> c(r.shape.columns, Fr(0));
    parallelColumns(c.size(), [&](size_t begin, size_t end) {
        for (size_t row = 0; row < r.shape.rows; ++row)
            for (size_t col = begin; col < end; ++col)
                c[col] += r.rows[row] * w[offset(r.shape, layout, row, col)];
    });
    return c;
}
} // namespace
MatrixShape matrixShape(size_t n) {
    if (!n || (n & (n - 1)) || n > (size_t(1) << 30))
        throw std::invalid_argument("matrix size outside power-of-two limits");
    size_t log = 0;
    for (auto v = n; v > 1; v >>= 1)
        ++log;
    size_t rows = size_t(1) << (log / 2), columns = n / rows;
    if (columns > kMaxColumns)
        throw std::invalid_argument("matrix columns limit");
    return {n, log, rows, columns};
}
MatrixCommitment commitMatrix(const Parameters &p, HyraxFrWitnessView values, Layout layout,
                              std::vector<Fr> &blinders) {
    validateParameters(p);
    requireLayout(layout);
    auto shape = matrixShape(values.size());
    if (p.G.size() != shape.columns)
        throw std::invalid_argument("matrix generator dimensions");
    for (size_t i = 0; i < values.size(); ++i)
        requireScalar(values[i]);
    MatrixCommitment c{values.size(), layout, {}};
    c.rows.reserve(shape.rows);
    blinders.clear();
    blinders.reserve(shape.rows);
    std::vector<Fr> row(shape.columns);
    for (size_t r = 0; r < shape.rows; ++r) {
        for (size_t j = 0; j < shape.columns; ++j)
            row[j] = values[offset(shape, layout, r, j)];
        Fr rho = sampleScalar();
        c.rows.push_back(msm(p.G, row) + p.H * rho);
        blinders.push_back(rho);
    }
    return c;
}
MatrixCommitResult commitMatrix(const Parameters &p, const std::vector<Fr> &values, Layout layout) {
    MatrixCommitResult r;
    r.witness.values = values;
    r.public_commitment = commitMatrix(p, HyraxFrWitnessView(values.data(), values.size()), layout,
                                       r.witness.row_blinders);
    return r;
}
LinearProof detail::proveLinear(const Parameters &p, const LinearStatement &s,
                                const LinearWitness &w, const Fr &blind, Transcript &t,
                                const std::string &label) {
    validateParameters(p);
    if (w.values.size() != p.G.size() || s.coefficients.size() != p.G.size())
        throw std::invalid_argument("linear witness dimensions");
    for (const auto &v : w.values)
        requireScalar(v);
    requireScalar(w.blinder);
    requireScalar(blind);
    if (msm(p.G, w.values) + p.H * w.blinder != s.commitment ||
        scalarCommit(p, {dot(s.coefficients, w.values), blind}) != s.evaluation)
        throw std::invalid_argument("linear witness does not open statement");
    bindLinear(p, s, t, label);
    std::vector<Fr> d(w.values.size());
    for (auto &v : d)
        v = sampleScalar();
    Fr a = sampleScalar(), b = sampleScalar();
    LinearProof proof;
    proof.A_C = msm(p.G, d) + p.H * a;
    proof.A_y = scalarCommit(p, {dot(s.coefficients, d), b});
    Fr e = linearChallenge(t, proof);
    proof.z.resize(d.size());
    for (size_t i = 0; i < d.size(); ++i)
        proof.z[i] = d[i] + e * w.values[i];
    proof.zrho = a + e * w.blinder;
    proof.zs = b + e * blind;
    for (const auto &z : proof.z)
        t.appendFr("hyrax.z", z);
    t.appendFr("hyrax.zrho", proof.zrho);
    t.appendFr("hyrax.zs", proof.zs);
    return proof;
}
bool detail::verifyLinear(const Parameters &p, const LinearStatement &s, const LinearProof &proof,
                          Transcript &t, const std::string &label, std::string *error) {
    try {
        if (proof.z.size() != p.G.size())
            throw std::invalid_argument("linear response dimensions");
        for (const auto &v : proof.z)
            requireScalar(v);
        requireScalar(proof.zrho);
        requireScalar(proof.zs);
        requirePoint(proof.A_C);
        requirePoint(proof.A_y);
        bindLinear(p, s, t, label);
        Fr e = linearChallenge(t, proof);
        if (msm(p.G, proof.z) + p.H * proof.zrho != proof.A_C + s.commitment * e ||
            scalarCommit(p, {dot(s.coefficients, proof.z), proof.zs}) !=
                proof.A_y + s.evaluation * e)
            throw std::invalid_argument("hidden linear equation");
        for (const auto &z : proof.z)
            t.appendFr("hyrax.z", z);
        t.appendFr("hyrax.zrho", proof.zrho);
        t.appendFr("hyrax.zs", proof.zs);
        return true;
    } catch (const std::exception &x) {
        if (error)
            *error = x.what();
        return false;
    }
}
LinearProof proveMle(const Parameters &p, const MleStatement &s, MatrixWitnessView w,
                     const Fr &blind, Transcript &t, const std::string &label) {
    validateParameters(p);
    auto r = reduction(p, s.commitment, s.point);
    if (w.row_blinders.size() != r.shape.rows)
        throw std::invalid_argument("row blinder dimensions");
    for (const auto &a : w.row_blinders)
        requireScalar(a);
    auto values = collapse(w.values, s.commitment.layout, r);
    Fr rho = dot(w.row_blinders, r.rows);
    detail::LinearStatement linear{r.C, s.evaluation, r.mu, s.context};
    bindMle(p, s, r, t, label);
    return detail::proveLinear(p, linear, {std::move(values), rho}, blind, t, label);
}
LinearProof proveMle(const Parameters &p, const MleStatement &s, const MatrixWitness &w,
                     const Fr &blind, Transcript &t, const std::string &label) {
    return proveMle(p, s, {HyraxFrWitnessView(w.values.data(), w.values.size()), w.row_blinders},
                    blind, t, label);
}
bool verifyMle(const Parameters &p, const MleStatement &s, const LinearProof &proof, Transcript &t,
               const std::string &label, std::string *error) {
    try {
        validateParameters(p);
        auto r = reduction(p, s.commitment, s.point);
        if (proof.z.size() != r.shape.columns)
            throw std::invalid_argument("MLE response dimensions");
        for (const auto &v : proof.z)
            requireScalar(v);
        requireScalar(proof.zrho);
        requireScalar(proof.zs);
        requirePoint(proof.A_C);
        requirePoint(proof.A_y);
        bindMle(p, s, r, t, label);
        return detail::verifyLinear(p, {r.C, s.evaluation, r.mu, s.context}, proof, t, label,
                                    error);
    } catch (const std::exception &x) {
        if (error)
            *error = x.what();
        return false;
    }
}
Fr evaluateMle(const std::vector<Fr> &values, const std::vector<Fr> &point, Layout layout) {
    requireLayout(layout);
    auto shape = matrixShape(values.size());
    if (point.size() != shape.log_size)
        throw std::invalid_argument("MLE point dimensions");
    for (const auto &x : point)
        requireScalar(x);
    auto rows = hyraxMleEqualityWeights(
        point, layout == Layout::RowMajor ? shape.log_size - shape.log_size / 2 : 0,
        shape.log_size / 2);
    auto mu = hyraxMleEqualityWeights(point, layout == Layout::RowMajor ? 0 : shape.log_size / 2,
                                      shape.log_size - shape.log_size / 2);
    PublicReduction r{shape, G1(), rows, mu};
    auto c = collapse(HyraxFrWitnessView(values.data(), values.size()), layout, r);
    return dot(c, mu);
}
Bytes encode(const LinearProof &p) {
    if (p.z.empty() || p.z.size() > kMaxColumns)
        throw std::invalid_argument("linear codec dimensions");
    Writer w(8 + 32 * (4 + p.z.size()));
    w.u64(p.z.size());
    w.point(p.A_C);
    w.point(p.A_y);
    for (const auto &x : p.z)
        w.fr(x);
    w.fr(p.zrho);
    w.fr(p.zs);
    return w.finish();
}
LinearProof decodeLinear(const Bytes &b, size_t columns) {
    if (!columns || columns > kMaxColumns)
        throw std::invalid_argument("linear decode dimensions");
    Reader r(b, 8 + 32 * (4 + columns));
    if (r.u64() != columns)
        throw std::invalid_argument("linear codec count");
    LinearProof p;
    p.A_C = r.point();
    p.A_y = r.point();
    p.z.reserve(columns);
    for (size_t i = 0; i < columns; ++i)
        p.z.push_back(r.fr());
    p.zrho = r.fr();
    p.zs = r.fr();
    r.finish();
    return p;
}
} // namespace zkgpt::zk
