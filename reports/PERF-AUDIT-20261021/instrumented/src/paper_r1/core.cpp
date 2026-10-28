#include "/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/PERF-AUDIT-20261021/audit_timer.hpp"
#include "core.hpp"
#include <cybozu/sha2.hpp>
#include <limits>
namespace zkgpt::r1 {
void require(bool ok, const std::string &why) {
    if (!ok)
        throw std::runtime_error(why);
}
void init() {
    mcl::bn::initPairing(mcl::BN254);
    require(Fr::getModulo() == kModulus, "unsupported scalar field");
}
unsigned log2exact(std::size_t n) {
    require(n && !(n & (n - 1)) && n <= kMaxVector, "vector dimension bound");
    unsigned d = 0;
    while (n > 1) {
        ++d;
        n >>= 1;
    }
    return d;
}
Bytes encode(const Fr &x) {
    Bytes b(32);
    require(x.serialize(b.data(), b.size()) == 32, "Fr encoding");
    return b;
}
Bytes encode(const G1 &x) { perf_audit::Scope audit_scope("core.encodeG1");
    require(x.isValid() && x.isValidOrder(), "invalid G1");
    Bytes b(G1::getSerializedByteSize());
    require(x.serialize(b.data(), b.size()) == b.size(), "G1 encoding");
    return b;
}
Fr decodeFr(const Bytes &b) {
    require(b.size() == 32, "Fr size");
    Fr f;
    require(f.deserialize(b.data(), b.size()) == b.size(), "noncanonical Fr");
    require(encode(f) == b, "noncanonical Fr roundtrip");
    return f;
}
G1 decodeG1(const Bytes &b) { perf_audit::Scope audit_scope("core.decodeG1");
    require(b.size() == G1::getSerializedByteSize(), "G1 size");
    G1 g;
    require(g.deserialize(b.data(), b.size()) == b.size() && g.isValid() && g.isValidOrder(),
            "noncanonical G1");
    require(encode(g) == b, "noncanonical G1 roundtrip");
    return g;
}
Digest hash(const Bytes &b) {
    Digest h;
    cybozu::Sha256 s;
    require(s.digest(h.data(), h.size(), b.data(), b.size()) == 32, "SHA256");
    return h;
}
static void u64(Bytes &b, std::uint64_t x) {
    for (unsigned j = 0; j < 8; ++j)
        b.push_back(std::uint8_t(x >> (8 * j)));
}
static void frame(Bytes &b, const std::string &l, const Bytes &v) {
    require(l.size() <= 4096 && v.size() <= 64 * 1024 * 1024, "transcript frame bound");
    u64(b, l.size());
    b.insert(b.end(), l.begin(), l.end());
    u64(b, v.size());
    b.insert(b.end(), v.begin(), v.end());
}
Fs::Fs(const std::string &d) {
    text("protocol", "zkGPT.paper-r1.fs.v1");
    text("domain", d);
    text("field", kModulus);
    text("curve", "MCL-BN254");
}
void Fs::bytes(const std::string &l, const Bytes &v) {
    Bytes b(state_.begin(), state_.end());
    frame(b, l, v);
    state_ = hash(b);
}
void Fs::text(const std::string &l, const std::string &s) { bytes(l, Bytes(s.begin(), s.end())); }
void Fs::integer(const std::string &l, std::uint64_t v) {
    Bytes b;
    u64(b, v);
    bytes(l, b);
}
void Fs::scalar(const std::string &l, const Fr &v) { bytes(l, encode(v)); }
void Fs::group(const std::string &l, const G1 &v) { bytes(l, encode(v)); }
Fr Fs::challenge(const std::string &l, bool nonzero) {
    require(counter_ != std::numeric_limits<std::uint64_t>::max(), "challenge counter overflow");
    for (std::uint64_t attempt = 0;; ++attempt) {
        Bytes b(state_.begin(), state_.end()), meta;
        u64(meta, counter_);
        u64(meta, attempt);
        meta.push_back(nonzero ? 1 : 0);
        frame(b, "sample.label", Bytes(l.begin(), l.end()));
        frame(b, "sample.counter", meta);
        auto h = hash(b);
        Fr f;
        if (f.deserialize(h.data(), h.size()) == h.size() &&
            encode(f) == Bytes(h.begin(), h.end()) && (!nonzero || !f.isZero())) {
            bytes("challenge.request", b);
            scalar("challenge.value", f);
            ++counter_;
            return f;
        }
        require(attempt != std::numeric_limits<std::uint64_t>::max(),
                "challenge attempts overflow");
    }
}
std::vector<Fr> point(Fs &t, unsigned d, const std::string &l) {
    require(d <= 28, "point bound");
    std::vector<Fr> r;
    for (unsigned i = 0; i < d; ++i)
        r.push_back(t.challenge(l + "/" + std::to_string(i)));
    return r;
}
std::vector<Fr> weights(const std::vector<Fr> &r) {
    require(r.size() <= 28, "weights bound");
    std::vector<Fr> w(std::size_t(1) << r.size());
    w[0] = 1;
    std::size_t n = 1;
    for (auto &x : r) {
        for (std::size_t i = 0; i < n; ++i) {
            w[i + n] = w[i] * x;
            w[i] *= Fr(1) - x;
        }
        n *= 2;
    }
    return w;
}
Fr mle(std::vector<Fr> v, const std::vector<Fr> &r) {
    require(log2exact(v.size()) == r.size(), "MLE shape");
    for (auto &x : r) {
        for (std::size_t i = 0; i < v.size() / 2; ++i)
            v[i] = v[2 * i] + x * (v[2 * i + 1] - v[2 * i]);
        v.resize(v.size() / 2);
    }
    return v[0];
}
Fr equality(const std::vector<Fr> &r, const std::vector<Fr> &s) {
    require(r.size() == s.size(), "eq shape");
    Fr v = 1;
    for (std::size_t i = 0; i < r.size(); ++i)
        v *= (Fr(1) - r[i]) * (Fr(1) - s[i]) + r[i] * s[i];
    return v;
}
Fr dot(const std::vector<Fr> &a, const std::vector<Fr> &b) {
    require(a.size() == b.size(), "dot shape");
    Fr x = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        x += a[i] * b[i];
    return x;
}
G1 msm(const std::vector<G1> &g, const std::vector<Fr> &a) { perf_audit::Scope audit_scope("core.msm");
    require(g.size() == a.size() && !g.empty(), "MSM shape");
    G1 x;
    G1::mulVec(x, const_cast<G1 *>(g.data()), a.data(), a.size());
    return x;
}
} // namespace zkgpt::r1
