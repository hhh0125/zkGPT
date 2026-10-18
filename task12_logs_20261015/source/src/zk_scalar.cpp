#include "zk_scalar.hpp"
#include <algorithm>
#include <cerrno>
#include <map>
#include <mutex>
#include <sys/random.h>

namespace zkgpt::zk {
namespace {
void suite() {
    if (Fr::getModulo() !=
            "16798108731015832284940804142231733909759579603404752749028378864165570215949" ||
        mcl::bn::Fp::getModulo() !=
            "16798108731015832284940804142231733909889187121439069848933715426072753864723" ||
        Fr::getByteSize() != 32 || G1::getSerializedByteSize() != 32)
        throw std::invalid_argument("foundation requires MCL BN254 default suite");
}
const Parameters &derivedParameters(const std::string &domain, std::size_t n) {
    suite();
    if (!n || n > kMaxColumns)
        throw std::invalid_argument("generator dimensions");
    static std::mutex mutex;
    static std::map<std::pair<std::string, std::size_t>, Parameters> cache;
    std::lock_guard<std::mutex> lock(mutex);
    const auto key = std::make_pair(domain, n);
    const auto found = cache.find(key);
    if (found != cache.end())
        return found->second;
    Parameters p;
    p.vector_domain = domain;
    mcl::bn::hashAndMapToG1(p.U, p.domain + "/scalar/U");
    mcl::bn::hashAndMapToG1(p.H, p.domain + "/common/H");
    if (domain == std::string(kGeneratorDomain) + "/vector") {
        p.G.resize(n);
        for (std::size_t i = 0; i < n; ++i)
            mcl::bn::hashAndMapToG1(p.G[i], p.domain + "/vector/G/" + std::to_string(i));
    } else {
        // getGeneratorSet restricts the descriptor to the established domains.
        p.G = getGeneratorSet({domain, static_cast<std::uint32_t>(n)}).generators;
    }
    std::vector<G1> all = p.G;
    all.push_back(p.U);
    all.push_back(p.H);
    std::vector<std::string> enc;
    enc.reserve(all.size() * 2);
    for (const auto &g : all) {
        requirePoint(g);
        if (g.isZero())
            throw std::runtime_error("identity generator");
        std::array<std::uint8_t, 32> bytes{};
        g.serialize(bytes.data(), bytes.size());
        enc.emplace_back(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        const auto negative = -g;
        negative.serialize(bytes.data(), bytes.size());
        enc.emplace_back(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    }
    std::sort(enc.begin(), enc.end());
    if (std::adjacent_find(enc.begin(), enc.end()) != enc.end())
        throw std::runtime_error("generator collision");
    return cache.emplace(key, std::move(p)).first->second;
}
void validateScalarBases(const Parameters &p) {
    if (p.domain != kGeneratorDomain)
        throw std::invalid_argument("scalar generator domain");
    const auto &expected = derivedParameters(std::string(kGeneratorDomain) + "/vector", 1);
    if (p.U != expected.U || p.H != expected.H)
        throw std::invalid_argument("non-derived scalar generators");
}
void count(std::size_t n) {
    if (!n || n > kMaxBatch)
        throw std::invalid_argument("invalid scalar batch size");
}
bool fail(std::string *e, const std::exception &x) {
    if (e)
        *e = x.what();
    return false;
}
void statements(const Parameters &p, const std::vector<G1> &c) {
    validateScalarBases(p);
    count(c.size());
    for (const auto &v : c)
        requirePoint(v);
}
void appendPoints(Transcript &t, const std::string &label, const std::vector<G1> &c) {
    t.appendU64(label + ".count", c.size());
    for (const auto &v : c)
        t.appendG1(label, v);
}
G1 equalityStatement(const Parameters &p, const std::vector<G1> &c, const std::vector<Fr> &alpha,
                     const Fr &k, Transcript &t, const std::string &label, const std::string &ctx) {
    statements(p, c);
    if (c.size() != alpha.size())
        throw std::invalid_argument("equality dimensions");
    requireScalar(k);
    for (const auto &a : alpha)
        requireScalar(a);
    bindContext(t, p, "equality", label, ctx);
    appendPoints(t, "D", c);
    t.appendFr("public-value", k);
    G1 E = -(p.U * k);
    for (size_t i = 0; i < c.size(); ++i) {
        t.appendFr("alpha", alpha[i]);
        E += c[i] * alpha[i];
    }
    t.appendG1("E", E);
    return E;
}
void productStatement(const Parameters &p, const G1 &x, const G1 &y, const G1 &z, Transcript &t,
                      const std::string &label, const std::string &ctx) {
    validateScalarBases(p);
    requirePoint(x);
    requirePoint(y);
    requirePoint(z);
    bindContext(t, p, "product", label, ctx);
    t.appendG1("Cx", x);
    t.appendG1("Cy", y);
    t.appendG1("Cz", z);
}
void witnessMatches(const Parameters &p, const G1 &c, const ScalarWitness &w) {
    if (scalarCommit(p, w) != c)
        throw std::invalid_argument("scalar witness does not open commitment");
}
Fr productChallenge(Transcript &t, const ProductProof &proof) {
    requirePoint(proof.A);
    requirePoint(proof.B);
    t.appendG1("A", proof.A);
    t.appendG1("B", proof.B);
    return t.challengeUniform("product-e", false);
}
} // namespace
bool validPoint(const G1 &x) { return x.isValid() && x.isValidOrder(); }
void requirePoint(const G1 &x) {
    if (!validPoint(x))
        throw std::invalid_argument("invalid G1 point/subgroup");
}
void requireScalar(const Fr &x) {
    if (!x.isValid())
        throw std::invalid_argument("invalid Fr scalar");
}
Parameters makeParameters(std::size_t n) {
    return derivedParameters(std::string(kGeneratorDomain) + "/vector", n);
}
Parameters makeProductionParameters(const GeneratorDomain &descriptor) {
    if (descriptor.domain != "zkGPT/main" && descriptor.domain != "zkGPT/range" &&
        descriptor.domain != "zkGPT/exp-lookup/v4")
        throw std::invalid_argument("unsupported production generator domain");
    return derivedParameters(descriptor.domain, descriptor.count);
}
void validateParameters(const Parameters &p) {
    validateScalarBases(p);
    const auto &expected = derivedParameters(p.vector_domain, p.G.size());
    if (p.G != expected.G)
        throw std::invalid_argument("non-derived vector generators");
}
bool scalarCandidate(const std::array<uint8_t, 32> &raw, Fr &out) {
    out = 0;
    Fr c = 0;
    if (c.deserialize(raw.data(), raw.size()) != raw.size() || !c.isValid())
        return false;
    std::array<uint8_t, 32> b{};
    if (c.serialize(b.data(), b.size()) != b.size() || b != raw)
        return false;
    out = c;
    return true;
}
Fr sampleScalar() {
    suite();
    for (size_t attempt = 0; attempt < 4096; ++attempt) {
        std::array<uint8_t, 32> raw{};
        size_t done = 0;
        while (done < raw.size()) {
            auto n = ::getrandom(raw.data() + done, raw.size() - done, 0);
            if (n < 0 && errno == EINTR)
                continue;
            if (n <= 0)
                throw std::runtime_error("getrandom failed");
            done += size_t(n);
        }
        Fr x;
        if (scalarCandidate(raw, x))
            return x;
    }
    throw std::runtime_error("entropy rejection exhausted");
}
G1 scalarCommit(const Parameters &p, const ScalarWitness &w) {
    validateScalarBases(p);
    requireScalar(w.value);
    requireScalar(w.blinder);
    return p.U * w.value + p.H * w.blinder;
}
void bindContext(Transcript &t, const Parameters &p, const std::string &protocol,
                 const std::string &label, const std::string &ctx) {
    if (label.empty() || label.size() > kMaxContextBytes || ctx.empty() ||
        ctx.size() > kMaxContextBytes || t.domain().empty() ||
        t.domain().size() > kMaxContextBytes || t.instance().size() > kMaxContextBytes)
        throw std::invalid_argument("invalid foundation session/label/context");
    t.appendString("zk.suite", p.domain);
    t.appendU64("zk.version", 1);
    t.appendString("zk.protocol", protocol);
    t.appendString("zk.label", label);
    t.appendString("zk.context", ctx);
    t.appendString("zk.session", t.instance());
    t.appendString("zk.vector-domain", p.vector_domain);
    t.appendU64("zk.vector-columns", p.G.size());
}
RepresentationProof proveRepresentation(const Parameters &p, const std::vector<G1> &c,
                                        const std::vector<ScalarWitness> &w, Transcript &t,
                                        const std::string &label, const std::string &ctx) {
    statements(p, c);
    if (c.size() != w.size())
        throw std::invalid_argument("representation dimensions");
    for (size_t i = 0; i < c.size(); ++i)
        witnessMatches(p, c[i], w[i]);
    RepresentationProof proof;
    std::vector<ScalarWitness> nonce(w.size());
    for (auto &a : nonce) {
        a = {sampleScalar(), sampleScalar()};
        proof.A.push_back(scalarCommit(p, a));
    }
    bindContext(t, p, "representation", label, ctx);
    appendPoints(t, "D", c);
    appendPoints(t, "A", proof.A);
    Fr e = t.challengeUniform("representation-e", false);
    for (size_t i = 0; i < w.size(); ++i) {
        proof.z_value.push_back(nonce[i].value + e * w[i].value);
        proof.z_blind.push_back(nonce[i].blinder + e * w[i].blinder);
    }
    for (size_t i = 0; i < w.size(); ++i) {
        t.appendFr("representation.z-value", proof.z_value[i]);
        t.appendFr("representation.z-blind", proof.z_blind[i]);
    }
    return proof;
}
bool verifyRepresentation(const Parameters &p, const std::vector<G1> &c,
                          const RepresentationProof &proof, Transcript &t, const std::string &label,
                          const std::string &ctx, std::string *error) {
    try {
        statements(p, c);
        if (proof.A.size() != c.size() || proof.z_value.size() != c.size() ||
            proof.z_blind.size() != c.size())
            throw std::invalid_argument("representation response dimensions");
        for (size_t i = 0; i < c.size(); ++i) {
            requirePoint(proof.A[i]);
            requireScalar(proof.z_value[i]);
            requireScalar(proof.z_blind[i]);
        }
        bindContext(t, p, "representation", label, ctx);
        appendPoints(t, "D", c);
        appendPoints(t, "A", proof.A);
        Fr e = t.challengeUniform("representation-e", false);
        for (size_t i = 0; i < c.size(); ++i)
            if (p.U * proof.z_value[i] + p.H * proof.z_blind[i] != proof.A[i] + c[i] * e)
                throw std::invalid_argument("representation equation");
        for (size_t i = 0; i < c.size(); ++i) {
            t.appendFr("representation.z-value", proof.z_value[i]);
            t.appendFr("representation.z-blind", proof.z_blind[i]);
        }
        return true;
    } catch (const std::exception &x) {
        return fail(error, x);
    }
}
EqualityProof proveEquality(const Parameters &p, const std::vector<G1> &c,
                            const std::vector<Fr> &alpha, const Fr &k,
                            const std::vector<ScalarWitness> &w, Transcript &t,
                            const std::string &label, const std::string &ctx) {
    statements(p, c);
    if (c.size() != w.size() || alpha.size() != c.size())
        throw std::invalid_argument("equality witness dimensions");
    Fr value = 0, r = 0;
    for (size_t i = 0; i < w.size(); ++i) {
        requireScalar(alpha[i]);
        witnessMatches(p, c[i], w[i]);
        value += alpha[i] * w[i].value;
        r += alpha[i] * w[i].blinder;
    }
    if (value != k)
        throw std::invalid_argument("false equality witness");
    equalityStatement(p, c, alpha, k, t, label, ctx);
    Fr nonce = sampleScalar();
    EqualityProof proof;
    proof.A = p.H * nonce;
    t.appendG1("A", proof.A);
    proof.z = nonce + t.challengeUniform("equality-e", false) * r;
    t.appendFr("equality.z", proof.z);
    return proof;
}
bool verifyEquality(const Parameters &p, const std::vector<G1> &c, const std::vector<Fr> &alpha,
                    const Fr &k, const EqualityProof &proof, Transcript &t,
                    const std::string &label, const std::string &ctx, std::string *error) {
    try {
        requirePoint(proof.A);
        requireScalar(proof.z);
        auto E = equalityStatement(p, c, alpha, k, t, label, ctx);
        t.appendG1("A", proof.A);
        Fr e = t.challengeUniform("equality-e", false);
        if (p.H * proof.z != proof.A + E * e)
            throw std::invalid_argument("equality equation");
        t.appendFr("equality.z", proof.z);
        return true;
    } catch (const std::exception &x) {
        return fail(error, x);
    }
}
ProductProof proveProduct(const Parameters &p, const G1 &X, const G1 &Y, const G1 &Z,
                          const ScalarWitness &x, const ScalarWitness &y, const ScalarWitness &z,
                          Transcript &t, const std::string &label, const std::string &ctx) {
    witnessMatches(p, X, x);
    witnessMatches(p, Y, y);
    witnessMatches(p, Z, z);
    if (z.value != x.value * y.value)
        throw std::invalid_argument("false product witness");
    productStatement(p, X, Y, Z, t, label, ctx);
    Fr a = sampleScalar(), b = sampleScalar(), c = sampleScalar();
    ProductProof proof;
    proof.A = p.U * a + p.H * b;
    proof.B = Y * a + p.H * c;
    Fr e = productChallenge(t, proof);
    proof.sx = a + e * x.value;
    proof.sr = b + e * x.blinder;
    proof.st = c + e * (z.blinder - x.value * y.blinder);
    t.appendFr("product.sx", proof.sx);
    t.appendFr("product.sr", proof.sr);
    t.appendFr("product.st", proof.st);
    return proof;
}
bool verifyProduct(const Parameters &p, const G1 &X, const G1 &Y, const G1 &Z,
                   const ProductProof &proof, Transcript &t, const std::string &label,
                   const std::string &ctx, std::string *error) {
    try {
        requireScalar(proof.sx);
        requireScalar(proof.sr);
        requireScalar(proof.st);
        requirePoint(proof.A);
        requirePoint(proof.B);
        productStatement(p, X, Y, Z, t, label, ctx);
        Fr e = productChallenge(t, proof);
        if (p.U * proof.sx + p.H * proof.sr != proof.A + X * e ||
            Y * proof.sx + p.H * proof.st != proof.B + Z * e)
            throw std::invalid_argument("product equation");
        t.appendFr("product.sx", proof.sx);
        t.appendFr("product.sr", proof.sr);
        t.appendFr("product.st", proof.st);
        return true;
    } catch (const std::exception &x) {
        return fail(error, x);
    }
}
void Writer::raw(const uint8_t *b, size_t n) {
    if (n > limit_ - data_.size())
        throw std::invalid_argument("codec write limit");
    data_.insert(data_.end(), b, b + n);
}
void Writer::bytes(const Bytes& bytes) {
    if (!bytes.empty())raw(bytes.data(),bytes.size());
}
void Writer::u64(uint64_t x) {
    uint8_t b[8];
    for (unsigned i = 0; i < 8; ++i)
        b[i] = uint8_t(x >> (8 * i));
    raw(b, 8);
}
void Writer::fr(const Fr &x) {
    requireScalar(x);
    std::array<uint8_t, 32> b{};
    if (x.serialize(b.data(), b.size()) != b.size())
        throw std::invalid_argument("Fr serialization");
    raw(b.data(), b.size());
}
void Writer::point(const G1 &x) {
    requirePoint(x);
    std::array<uint8_t, 32> b{};
    if (x.serialize(b.data(), b.size()) != b.size())
        throw std::invalid_argument("G1 serialization");
    raw(b.data(), b.size());
}
Reader::Reader(const Bytes &b, size_t exact) : data_(b) {
    if (b.size() != exact)
        throw std::invalid_argument("codec exact size");
}
void Reader::need(size_t n) const {
    if (n > data_.size() - pos_)
        throw std::invalid_argument("codec truncated");
}
uint64_t Reader::u64() {
    need(8);
    uint64_t x = 0;
    for (unsigned i = 0; i < 8; ++i)
        x |= uint64_t(data_[pos_++]) << (8 * i);
    return x;
}
Fr Reader::fr() {
    need(32);
    std::array<uint8_t, 32> b{};
    std::copy_n(data_.data() + pos_, 32, b.data());
    Fr x;
    if (!scalarCandidate(b, x))
        throw std::invalid_argument("noncanonical Fr");
    pos_ += 32;
    return x;
}
G1 Reader::point() {
    need(32);
    G1 x;
    if (x.deserialize(data_.data() + pos_, 32) != 32 || !validPoint(x))
        throw std::invalid_argument("invalid encoded G1");
    std::array<uint8_t, 32> b{};
    if (x.serialize(b.data(), b.size()) != 32 ||
        !std::equal(b.begin(), b.end(), data_.begin() + pos_))
        throw std::invalid_argument("noncanonical G1");
    pos_ += 32;
    return x;
}
void Reader::finish() const {
    if (pos_ != data_.size())
        throw std::invalid_argument("codec trailing bytes");
}
Bytes Reader::bytes(std::size_t count) {
    need(count);
    Bytes result(data_.begin()+pos_,data_.begin()+pos_+count);pos_+=count;return result;
}
Bytes encode(const RepresentationProof &p) {
    count(p.A.size());
    if (p.A.size() != p.z_value.size() || p.A.size() != p.z_blind.size())
        throw std::invalid_argument("representation codec dimensions");
    Writer w(8 + 96 * p.A.size());
    w.u64(p.A.size());
    for (size_t i = 0; i < p.A.size(); ++i) {
        w.point(p.A[i]);
        w.fr(p.z_value[i]);
        w.fr(p.z_blind[i]);
    }
    return w.finish();
}
RepresentationProof decodeRepresentation(const Bytes &b, size_t n) {
    count(n);
    Reader r(b, 8 + 96 * n);
    if (r.u64() != n)
        throw std::invalid_argument("representation codec count");
    RepresentationProof p;
    p.A.reserve(n);
    p.z_value.reserve(n);
    p.z_blind.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        p.A.push_back(r.point());
        p.z_value.push_back(r.fr());
        p.z_blind.push_back(r.fr());
    }
    r.finish();
    return p;
}
Bytes encode(const EqualityProof &p) {
    Writer w(64);
    w.point(p.A);
    w.fr(p.z);
    return w.finish();
}
EqualityProof decodeEquality(const Bytes &b) {
    Reader r(b, 64);
    EqualityProof p;
    p.A = r.point();
    p.z = r.fr();
    r.finish();
    return p;
}
Bytes encode(const ProductProof &p) {
    Writer w(160);
    w.point(p.A);
    w.point(p.B);
    w.fr(p.sx);
    w.fr(p.sr);
    w.fr(p.st);
    return w.finish();
}
ProductProof decodeProduct(const Bytes &b) {
    Reader r(b, 160);
    ProductProof p;
    p.A = r.point();
    p.B = r.point();
    p.sx = r.fr();
    p.sr = r.fr();
    p.st = r.fr();
    r.finish();
    return p;
}
} // namespace zkgpt::zk
