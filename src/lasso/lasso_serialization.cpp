#include "lasso_serialization.hpp"
#include "lasso_table.hpp"
#include <algorithm>
#include <fstream>
#include <limits>
namespace route_b::lasso {
void Writer::raw(const std::uint8_t *p, std::size_t n) {
    require(n <= kMaxArtifact && bytes.size() <= kMaxArtifact - n, "encoding size cap");
    bytes.insert(bytes.end(), p, p + n);
}
void Writer::u32(std::uint32_t v) {
    std::uint8_t b[4];
    for (unsigned i = 0; i < 4; ++i)
        b[i] = v >> (8 * i);
    raw(b, 4);
}
void Writer::string(const std::string &s) {
    require(s.size() <= 64, "string bound");
    u32(s.size());
    raw(reinterpret_cast<const std::uint8_t *>(s.data()), s.size());
}
void Writer::fr(const Fr &v) {
    std::array<std::uint8_t, 32> b{};
    require(v.serialize(b.data(), b.size()) == b.size(), "Fr size");
    raw(b.data(), b.size());
}
void Writer::g1(const G1 &v) {
    std::array<std::uint8_t, 32> b{};
    require(v.isValid() && v.isValidOrder(), "G1 invalid");
    require(v.serialize(b.data(), b.size()) == b.size(), "G1 size");
    raw(b.data(), b.size());
}
class Reader {
    const Bytes &b;
    std::size_t pos = 0;

  public:
    explicit Reader(const Bytes &v) : b(v) {
        require(v.size() <= kMaxArtifact, "artifact size cap");
    }
    void raw(std::uint8_t *p, std::size_t n) {
        require(n <= b.size() - pos, "truncated artifact");
        std::copy_n(b.data() + pos, n, p);
        pos += n;
    }
    std::uint32_t u32() {
        std::uint8_t d[4];
        raw(d, 4);
        std::uint32_t v = 0;
        for (unsigned i = 0; i < 4; ++i)
            v |= std::uint32_t(d[i]) << (8 * i);
        return v;
    }
    std::size_t count(std::size_t max) {
        auto n = u32();
        require(n <= max, "length cap");
        return n;
    }
    void tag(std::uint32_t v) { require(u32() == v, "missing, duplicate, or reordered field"); }
    void magic(const char *s) {
        std::array<std::uint8_t, 8> d{};
        raw(d.data(), d.size());
        require(std::equal(d.begin(), d.end(), s), "magic mismatch");
    }
    std::string string() {
        auto n = count(64);
        std::string s(n, ' ');
        raw(reinterpret_cast<std::uint8_t *>(s.data()), n);
        return s;
    }
    Fr fr() {
        std::array<std::uint8_t, 32> d{};
        raw(d.data(), d.size());
        Fr v;
        require(v.deserialize(d.data(), d.size()) == d.size(), "invalid Fr encoding");
        std::array<std::uint8_t, 32> c{};
        require(v.serialize(c.data(), c.size()) == c.size() && c == d, "noncanonical Fr");
        return v;
    }
    G1 g1() {
        std::array<std::uint8_t, 32> d{};
        raw(d.data(), d.size());
        G1 v;
        require(v.deserialize(d.data(), d.size()) == d.size(), "invalid G1 encoding");
        require(v.isValid() && v.isValidOrder(), "G1 subgroup/curve");
        std::array<std::uint8_t, 32> c{};
        require(v.serialize(c.data(), c.size()) == c.size() && c == d, "noncanonical G1");
        return v;
    }
    void end() { require(pos == b.size(), "trailing bytes"); }
};
static void writeOpening(Writer &w, const Opening &o) {
    require(o.ipa.rounds.size() <= kMaxBits, "IPA bound");
    w.fr(o.value);
    w.u32(o.ipa.rounds.size());
    for (auto &r : o.ipa.rounds) {
        w.g1(r.left);
        w.g1(r.right);
    }
    w.fr(o.ipa.final_witness);
}
static Opening readOpening(Reader &r) {
    Opening o;
    o.value = r.fr();
    auto n = r.count(kMaxBits);
    for (std::size_t i = 0; i < n; ++i) {
        HyraxIpaRound x;
        x.left = r.g1();
        x.right = r.g1();
        o.ipa.rounds.push_back(x);
    }
    o.ipa.final_witness = r.fr();
    return o;
}
static void writeSumcheck(Writer &w, const Sumcheck &s) {
    require(s.rounds.size() <= kMaxBits, "sumcheck bound");
    w.u32(s.rounds.size());
    for (auto &v : s.rounds) {
        require(v.size() <= 4, "degree bound");
        w.u32(v.size());
        for (auto &x : v)
            w.fr(x);
    }
}
static Sumcheck readSumcheck(Reader &r) {
    Sumcheck s;
    auto n = r.count(kMaxBits);
    for (std::size_t i = 0; i < n; ++i) {
        auto k = r.count(4);
        std::vector<Fr> v;
        for (std::size_t j = 0; j < k; ++j)
            v.push_back(r.fr());
        s.rounds.push_back(v);
    }
    return s;
}
static void writeOpenings(Writer &w, const std::vector<Opening> &o) {
    require(o.size() <= kMaxSegments + 2, "opening list bound");
    w.u32(o.size());
    for (auto &v : o)
        writeOpening(w, v);
}
static std::vector<Opening> readOpenings(Reader &r) {
    auto n = r.count(kMaxSegments + 2);
    std::vector<Opening> o;
    for (std::size_t i = 0; i < n; ++i)
        o.push_back(readOpening(r));
    return o;
}
Bytes encodeInstance(const LassoLookupInstance &x) {
    validateInstance(x);
    Writer w;
    w.raw(reinterpret_cast<const std::uint8_t *>("ZGLSINS1"), 8);
    w.u32(x.version);
    w.u32(x.field_id);
    w.u32(x.curve_id);
    w.u32(x.backend);
    w.u32(x.padding_id);
    w.u32(x.query_length);
    w.u32(x.table.size());
    w.u32(x.additive ? 1 : 0);
    w.string(x.profile);
    w.u32(x.segment_bits.size());
    for (auto b : x.segment_bits)
        w.u32(b);
    w.raw(x.table_digest.data(), 32);
    for (auto &v : x.table)
        w.fr(v);
    return w.bytes;
}
LassoLookupInstance decodeInstance(const Bytes &b) {
    Reader r(b);
    r.magic("ZGLSINS1");
    LassoLookupInstance x;
    x.version = r.u32();
    x.field_id = r.u32();
    x.curve_id = r.u32();
    x.backend = r.u32();
    x.padding_id = r.u32();
    x.query_length = r.u32();
    auto n = r.count(kMaxSize);
    auto add = r.u32();
    require(add <= 1, "noncanonical boolean");
    x.additive = add;
    x.profile = r.string();
    auto c = r.count(kMaxSegments);
    for (std::size_t i = 0; i < c; ++i)
        x.segment_bits.push_back(r.u32());
    r.raw(x.table_digest.data(), 32);
    for (std::size_t i = 0; i < n; ++i)
        x.table.push_back(r.fr());
    r.end();
    validateInstance(x);
    return x;
}
Bytes encodeProof(const LassoProof &p) {
    Writer w;
    w.raw(reinterpret_cast<const std::uint8_t *>("ZGLSPRF1"), 8);
    w.u32(1);
    w.tag(1);
    w.raw(p.instance_digest.data(), 32);
    w.g1(p.query_commitment);
    require(p.dim_commitments.size() <= kMaxSegments, "dim bound");
    w.u32(p.dim_commitments.size());
    for (auto &c : p.dim_commitments)
        w.g1(c);
    w.tag(2);
    require(p.memory_commitments.size() <= kMaxSegments + 1, "memory bound");
    w.u32(p.memory_commitments.size());
    for (auto &m : p.memory_commitments) {
        w.g1(m.value);
        w.g1(m.read);
        w.g1(m.final);
    }
    w.tag(3);
    writeOpening(w, p.query_evaluation);
    writeOpenings(w, p.padding_openings);
    w.tag(4);
    writeSumcheck(w, p.lookup_sumcheck);
    writeOpenings(w, p.read_evaluations);
    w.tag(5);
    require(p.memories.size() <= kMaxSegments + 1, "memory bound");
    w.u32(p.memories.size());
    for (auto &m : p.memories) {
        for (auto &v : m.roots)
            w.fr(v);
        for (auto &g : m.products) {
            require(g.layers.size() <= kMaxBits, "tree bound");
            w.u32(g.layers.size());
            for (auto &l : g.layers) {
                writeSumcheck(w, l.sumcheck);
                w.fr(l.left);
                w.fr(l.right);
            }
            writeOpenings(w, g.leaf_openings);
        }
    }
    w.tag(6);
    return w.bytes;
}
LassoProof decodeProof(const Bytes &b) {
    Reader r(b);
    r.magic("ZGLSPRF1");
    require(r.u32() == 1, "proof version");
    LassoProof p;
    r.tag(1);
    r.raw(p.instance_digest.data(), 32);
    p.query_commitment = r.g1();
    auto c = r.count(kMaxSegments);
    for (std::size_t i = 0; i < c; ++i)
        p.dim_commitments.push_back(r.g1());
    r.tag(2);
    auto n = r.count(kMaxSegments + 1);
    for (std::size_t i = 0; i < n; ++i) {
        MemoryCommitments m;
        m.value = r.g1();
        m.read = r.g1();
        m.final = r.g1();
        p.memory_commitments.push_back(m);
    }
    r.tag(3);
    p.query_evaluation = readOpening(r);
    p.padding_openings = readOpenings(r);
    r.tag(4);
    p.lookup_sumcheck = readSumcheck(r);
    p.read_evaluations = readOpenings(r);
    r.tag(5);
    n = r.count(kMaxSegments + 1);
    for (std::size_t i = 0; i < n; ++i) {
        MemoryProof m;
        for (auto &v : m.roots)
            v = r.fr();
        for (auto &g : m.products) {
            auto k = r.count(kMaxBits);
            for (std::size_t j = 0; j < k; ++j) {
                ProductLayer l;
                l.sumcheck = readSumcheck(r);
                l.left = r.fr();
                l.right = r.fr();
                g.layers.push_back(l);
            }
            g.leaf_openings = readOpenings(r);
        }
        p.memories.push_back(m);
    }
    r.tag(6);
    r.end();
    return p;
}
Bytes readFile(const std::string &p) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    require(bool(f), "file open");
    auto n = f.tellg();
    require(n >= 0 && static_cast<std::uint64_t>(n) <= kMaxArtifact, "file size cap");
    Bytes b(static_cast<std::size_t>(n));
    f.seekg(0);
    f.read(reinterpret_cast<char *>(b.data()), b.size());
    require(bool(f), "file read");
    return b;
}
void writeFile(const std::string &p, const Bytes &b) {
    require(b.size() <= kMaxArtifact, "file size cap");
    std::ofstream f(p, std::ios::binary);
    f.write(reinterpret_cast<const char *>(b.data()), b.size());
    require(bool(f), "file write");
}
} // namespace route_b::lasso
