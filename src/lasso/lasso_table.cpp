#include "lasso_serialization.hpp"
#include "lasso_table.hpp"
#include <algorithm>
#include <cybozu/sha2.hpp>
#include <stdexcept>
namespace route_b::lasso {
void require(bool ok, const std::string &why) {
    if (!ok)
        throw std::runtime_error(why);
}
unsigned log2Exact(std::size_t n) {
    require(n && !(n & (n - 1)), "non power of two");
    unsigned b = 0;
    while (n > 1) {
        n >>= 1;
        ++b;
    }
    return b;
}
std::size_t paddedSize(std::size_t n) {
    require(n && n <= kMaxSize, "size outside [1,256]");
    std::size_t p = 1;
    while (p < n)
        p <<= 1;
    return p;
}
std::vector<Fr> eqWeights(const std::vector<Fr> &p) {
    require(p.size() <= kMaxBits, "point too large");
    std::vector<Fr> w(1, Fr(1));
    for (const auto &r : p) {
        auto n = w.size();
        w.resize(2 * n);
        for (std::size_t i = 0; i < n; ++i) {
            auto x = w[i];
            w[i] = x * (Fr(1) - r);
            w[i + n] = x * r;
        }
    }
    return w;
}
Fr mle(std::vector<Fr> v, const std::vector<Fr> &p) {
    require(p.size() <= kMaxBits, "MLE point bound");
    require(v.size() == (std::size_t{1} << p.size()), "MLE dimension mismatch");
    for (auto &r : p) {
        for (std::size_t i = 0; i < v.size() / 2; ++i)
            v[i] = v[2 * i] + r * (v[2 * i + 1] - v[2 * i]);
        v.resize(v.size() / 2);
    }
    return v.at(0);
}
Fr eq(const std::vector<Fr> &a, const std::vector<Fr> &b) {
    require(a.size() == b.size(), "eq dimensions");
    Fr v = 1;
    for (std::size_t i = 0; i < a.size(); ++i)
        v *= a[i] * b[i] + (Fr(1) - a[i]) * (Fr(1) - b[i]);
    return v;
}
Fr addressEvaluation(const std::vector<Fr> &p) {
    Fr v = 0, s = 1;
    for (auto &r : p) {
        v += s * r;
        s += s;
    }
    return v;
}
static Digest hash(const Bytes &b) {
    Digest d{};
    cybozu::Sha256 h;
    require(h.digest(d.data(), d.size(), b.data(), b.size()) == d.size(), "SHA256");
    return d;
}
static Digest tableDigest(const std::vector<Fr> &t) {
    Writer w;
    w.string("zkGPT.route-b.lasso.table.v1");
    w.u32(t.size());
    for (auto &v : t)
        w.fr(v);
    return hash(w.bytes);
}
LassoLookupInstance makeInstance(const std::vector<Fr> &t, std::size_t m,
                                 const std::vector<unsigned> &b, bool add, const std::string &p) {
    paddedSize(m);
    paddedSize(t.size());
    require(!b.empty() && b.size() <= kMaxSegments, "segment count");
    LassoLookupInstance x;
    x.table = t;
    x.query_length = m;
    x.segment_bits = b;
    x.additive = add;
    x.profile = p;
    x.table_digest = tableDigest(t);
    validateInstance(x);
    return x;
}
void validateInstance(const LassoLookupInstance &x) {
    require(Fr::getModulo() == kFieldModulus, "runtime field is not project MCL BN254");
    require(x.version == 1 && x.field_id == 1 && x.curve_id == 1 && x.backend == 1 &&
                x.padding_id == 1,
            "unsupported version/field/curve/backend/padding");
    paddedSize(x.query_length);
    auto n = paddedSize(x.table.size());
    require(!x.profile.empty() && x.profile.size() <= 64, "profile length");
    for (unsigned char c : x.profile)
        require(c >= 33 && c <= 126, "profile ASCII");
    require(!x.segment_bits.empty() && x.segment_bits.size() <= kMaxSegments, "segment count");
    unsigned sum = 0;
    for (auto b : x.segment_bits) {
        require(b <= kMaxBits, "segment width");
        require(b || (x.segment_bits.size() == 1 && n == 1), "zero segment width");
        sum += b;
    }
    require(sum == log2Exact(n), "segment layout mismatch");
    require(x.additive || x.segment_bits.size() == 1, "explicit table requires c=1");
    require(x.table_digest == tableDigest(x.table), "table digest mismatch");
    if (x.additive) {
        auto tables = subTables(x);
        for (std::size_t a = 0; a < x.table.size(); ++a) {
            Fr expected = x.table[0];
            unsigned off = 0;
            for (std::size_t h = 0; h < tables.size(); ++h) {
                expected += tables[h][(a >> off) & (tables[h].size() - 1)];
                off += x.segment_bits[h];
            }
            require(expected == x.table[a], "table is not additive SOS");
        }
    }
}
Digest instanceDigest(const LassoLookupInstance &x) { return hash(encodeInstance(x)); }
std::vector<std::vector<Fr>> subTables(const LassoLookupInstance &x) {
    if (!x.additive) {
        auto t = x.table;
        t.resize(paddedSize(t.size()), t[0]);
        return {t};
    }
    std::vector<std::vector<Fr>> out;
    unsigned offset = 0;
    for (auto b : x.segment_bits) {
        std::vector<Fr> t(std::size_t{1} << b);
        Fr scale = 0;
        auto a = std::size_t{1} << offset;
        if (a < x.table.size())
            scale = x.table[a] - x.table[0];
        for (std::size_t j = 0; j < t.size(); ++j)
            t[j] = scale * Fr(j);
        out.push_back(t);
        offset += b;
    }
    return out;
}
Fr combineValues(const LassoLookupInstance &x, const std::vector<Fr> &v) {
    require(v.size() == x.segment_bits.size(), "read arity");
    if (!x.additive)
        return v[0];
    Fr s = x.table[0];
    for (auto &e : v)
        s += e;
    return s;
}
LassoSelector makeSelector(const LassoLookupInstance &x, const std::vector<std::size_t> &a) {
    validateInstance(x);
    require(a.size() == x.query_length, "address length");
    LassoSelector s;
    s.dim.resize(x.segment_bits.size(), std::vector<Fr>(paddedSize(a.size()), Fr(0)));
    for (std::size_t i = 0; i < a.size(); ++i) {
        require(a[i] < x.table.size(), "invalid table address");
        unsigned off = 0;
        for (std::size_t h = 0; h < s.dim.size(); ++h) {
            s.dim[h][i] = Fr((a[i] >> off) & ((std::size_t{1} << x.segment_bits[h]) - 1));
            off += x.segment_bits[h];
        }
    }
    return s;
}
ProverWitness makeWitness(const LassoLookupInstance &x, const std::vector<Fr> &q) {
    validateInstance(x);
    require(q.size() == x.query_length, "query length");
    std::vector<std::size_t> a;
    for (auto &v : q) {
        auto it = std::find(x.table.begin(), x.table.end(), v);
        require(it != x.table.end(), "query not in table");
        a.push_back(it - x.table.begin());
    }
    ProverWitness w;
    w.selector = makeSelector(x, a);
    w.query = q;
    w.query.resize(paddedSize(q.size()), x.table[0]);
    a.resize(w.query.size(), 0);
    auto tables = subTables(x);
    // Last memory is a public validity table; its read value is fixed to 1.
    std::vector<Fr> valid(paddedSize(x.table.size()), Fr(0));
    std::fill(valid.begin(), valid.begin() + x.table.size(), Fr(1));
    tables.push_back(valid);
    for (std::size_t h = 0; h < tables.size(); ++h) {
        MemoryWitness mem;
        mem.table = tables[h];
        mem.final.assign(mem.table.size(), Fr(0));
        for (std::size_t i = 0; i < w.query.size(); ++i) {
            std::size_t address = a[i];
            if (h < w.selector.dim.size()) {
                unsigned off = 0;
                for (std::size_t j = 0; j < h; ++j)
                    off += x.segment_bits[j];
                address = (a[i] >> off) & (mem.table.size() - 1);
            }
            mem.address.push_back(Fr(address));
            mem.value.push_back(mem.table[address]);
            mem.read.push_back(mem.final[address]);
            mem.final[address] += Fr(1);
        }
        w.memories.push_back(std::move(mem));
    }
    return w;
}
} // namespace route_b::lasso
