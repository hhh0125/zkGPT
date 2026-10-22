#include "pc.hpp"
#include <map>
#include <mutex>
#include <cctype>
namespace zkgpt::r1 {
PCSpec PCSpec::make(const std::string &role, std::size_t a, std::size_t n, Layout layout) {
    auto d = log2exact(n);
    PCSpec s{role, a, n, std::size_t(1) << (d / 2), std::size_t(1) << (d - d / 2), layout};
    s.validate();
    return s;
}
void PCSpec::validate() const {
    auto d = log2exact(padded);
    require(actual > 0 && actual <= padded, "PC actual length");
    require(rows == (std::size_t(1) << (d / 2)) && columns == padded / rows, "PC balanced shape");
    require(layout == Layout::ColumnMajor || layout == Layout::RowMajor, "PC layout");
    require(!role.empty() && role.size() <= 128, "PC role length");
    for (unsigned char c : role)
        require((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                    c == '/' || c == '-' || c == '_' || c == '.',
                "PC role character");
    require(Fr::getModulo() == kModulus, "PC field");
}
bool PCSpec::operator==(const PCSpec &b) const {
    return role == b.role && actual == b.actual && padded == b.padded && rows == b.rows &&
           columns == b.columns && layout == b.layout;
}
const Generators &generators(const PCSpec &s) {
    s.validate();
    static std::mutex mu;
    static std::map<std::pair<std::string, std::size_t>, Generators> cache;
    std::lock_guard<std::mutex> lock(mu);
    auto key = std::make_pair(s.role, s.columns);
    auto it = cache.find(key);
    if (it != cache.end())
        return it->second;
    Generators out;
    out.g.resize(s.columns);
    std::string domain =
        "zkGPT.paper-r1.pc.v1/role/" + std::to_string(s.role.size()) + "/" + s.role;
    for (std::size_t i = 0; i < s.columns; ++i) {
        mcl::bn::hashAndMapToG1(out.g[i], domain + "/g/" + std::to_string(i));
        require(out.g[i].isValid() && out.g[i].isValidOrder() && !out.g[i].isZero(),
                "PC generator");
    }
    mcl::bn::hashAndMapToG1(out.u, domain + "/u");
    require(out.u.isValid() && out.u.isValidOrder() && !out.u.isZero(), "PC U");
    return cache.emplace(key, std::move(out)).first->second;
}
void append(Fs &t, const PCSpec &s) {
    s.validate();
    t.text("pc.scheme", "balanced-Hyrax-IPA-nonhiding-v1");
    t.text("pc.derivation", "MCL-hashAndMapToG1/zkGPT.paper-r1.pc.v1");
    t.text("pc.role", s.role);
    t.integer("pc.actual", s.actual);
    t.integer("pc.padded", s.padded);
    t.integer("pc.rows", s.rows);
    t.integer("pc.columns", s.columns);
    t.integer("pc.layout", unsigned(s.layout));
}
static void shape(const PCSpec &s, const Commitment &c) {
    s.validate();
    require(s == c.spec && c.rows.size() == s.rows, "unexpected PC identity/shape");
    for (auto &g : c.rows)
        require(g.isValid() && g.isValidOrder(), "invalid PC group");
}
void append(Fs &t, const Commitment &c) {
    shape(c.spec, c);
    append(t, c.spec);
    for (auto &g : c.rows)
        t.group("pc.row", g);
}
#ifndef R1_VERIFY_ONLY
static std::size_t index(const PCSpec &s, std::size_t r, std::size_t c) {
    return s.layout == Layout::ColumnMajor ? r + c * s.rows : r * s.columns + c;
}
#endif

#ifndef R1_VERIFY_ONLY
Commitment commit(const PCSpec &s, const std::vector<Fr> &w) {
    s.validate();
    require(w.size() == s.padded, "PC witness length");
    const auto &g = generators(s);
    Commitment c{s, {}};
    std::vector<Fr> row(s.columns);
    for (std::size_t r = 0; r < s.rows; ++r) {
        for (std::size_t j = 0; j < s.columns; ++j)
            row[j] = w[index(s, r, j)];
        c.rows.push_back(msm(g.g, row));
    }
    return c;
}
#endif

struct Reduction {
    G1 c;
    std::vector<Fr> a, row;
};
static Reduction reduce(const PCSpec &s, const Commitment &c, const std::vector<Fr> &r) {
    shape(s, c);
    auto d = log2exact(s.padded);
    require(r.size() == d, "PC point dimension");
    unsigned rb = d / 2, cb = d - rb;
    std::vector<Fr> rp, cp;
    if (s.layout == Layout::ColumnMajor) {
        rp = {r.begin(), r.begin() + rb};
        cp = {r.begin() + rb, r.end()};
    } else {
        cp = {r.begin(), r.begin() + cb};
        rp = {r.begin() + cb, r.end()};
    }
    auto rw = weights(rp);
    return {msm(c.rows, rw), weights(cp), std::move(rw)};
}
static void statement(Fs &t, const PCSpec &s, const Commitment &c, const std::vector<Fr> &r,
                      const Fr &v, const std::string &l) {
    t.text("pc.open.label", l);
    append(t, c);
    require(s == c.spec, "PC expected spec");
    t.integer("pc.point.size", r.size());
    for (auto &x : r)
        t.scalar("pc.point", x);
    t.scalar("pc.value", v);
}
#ifndef R1_VERIFY_ONLY
OpenProof open(const PCSpec &s, const Commitment &c, const std::vector<Fr> &w,
               const std::vector<Fr> &r, Fs &t, const std::string &l) {
    require(w.size() == s.padded, "PC witness length");
    auto reduced = reduce(s, c, r);
    std::vector<Fr> a(s.columns, Fr(0));
    for (std::size_t j = 0; j < s.columns; ++j)
        for (std::size_t i = 0; i < s.rows; ++i)
            a[j] += reduced.row[i] * w[index(s, i, j)];
    auto b = reduced.a;
    auto g = generators(s).g;
    const G1 u = generators(s).u;
    require(msm(g, a) == reduced.c, "PC witness commitment mismatch");
    OpenProof out;
    out.value = dot(a, b);
    statement(t, s, c, r, out.value, l);
    while (a.size() > 1) {
        auto h = a.size() / 2;
        std::vector<Fr> al(a.begin(), a.begin() + h), ar(a.begin() + h, a.end()),
            bl(b.begin(), b.begin() + h), br(b.begin() + h, b.end());
        std::vector<G1> gl(g.begin(), g.begin() + h), gr(g.begin() + h, g.end());
        IpaRound q{msm(gr, al) + u * dot(al, br), msm(gl, ar) + u * dot(ar, bl)};
        out.rounds.push_back(q);
        t.group("ipa.L", q.left);
        t.group("ipa.R", q.right);
        Fr x = t.challenge("ipa.fold", true), xi;
        Fr::inv(xi, x);
        for (std::size_t i = 0; i < h; ++i) {
            a[i] = x * al[i] + xi * ar[i];
            b[i] = xi * bl[i] + x * br[i];
            g[i] = gl[i] * xi + gr[i] * x;
        }
        a.resize(h);
        b.resize(h);
        g.resize(h);
    }
    out.final = a[0];
    t.scalar("ipa.final", out.final);
    return out;
}
#endif

void verify(const PCSpec &s, const Commitment &c, const std::vector<Fr> &r, const OpenProof &p,
            Fs &t, const std::string &l) {
    auto reduced = reduce(s, c, r);
    require(p.rounds.size() == log2exact(s.columns), "IPA round count");
    statement(t, s, c, r, p.value, l);
    auto g = generators(s).g;
    auto b = reduced.a;
    G1 acc = reduced.c + generators(s).u * p.value;
    for (const auto &q : p.rounds) {
        t.group("ipa.L", q.left);
        t.group("ipa.R", q.right);
        Fr x = t.challenge("ipa.fold", true), xi;
        Fr::inv(xi, x);
        acc += q.left * (x * x) + q.right * (xi * xi);
        auto h = g.size() / 2;
        for (std::size_t i = 0; i < h; ++i) {
            g[i] = g[i] * xi + g[i + h] * x;
            b[i] = b[i] * xi + b[i + h] * x;
        }
        g.resize(h);
        b.resize(h);
    }
    t.scalar("ipa.final", p.final);
    require(acc == g[0] * p.final + generators(s).u * (p.final * b[0]), "IPA final equation");
}
} // namespace zkgpt::r1
