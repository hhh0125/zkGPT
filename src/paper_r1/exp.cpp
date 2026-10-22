#include "exp.hpp"
namespace zkgpt::r1 {
namespace {
#include "exp_table.inc"
constexpr std::uint32_t K = 8191, Scale = 1048576;
PCSpec helperSpec(const ExpSpec &s, unsigned k) {
    const std::array<std::string, 4> name{"x", "selector", "delta", "native-E"};
    require(k < 4, "Exp helper index");
    return PCSpec::make(s.id + "/" + name[k], s.index.actual, s.index.padded, Layout::ColumnMajor);
}
RangeSpec rangeSpec(const ExpSpec &s, unsigned k) {
    const std::array<std::string, 4> name{"t", "x", "selector", "delta"};
    const std::array<unsigned, 4> bits{32, 13, 1, 32};
    require(k < 4, "Exp Range index");
    return {s.id + "/range/" + name[k], k == 0 ? s.index : helperSpec(s, k - 1), bits[k]};
}
MemorySpec memorySpec(const ExpSpec &s) {
    return {s.id + "/indexed", helperSpec(s, 0), helperSpec(s, 3), expTable(s.profile)};
}
Fr active(std::size_t n, const std::vector<Fr> &r) {
    auto w = weights(r);
    Fr v = 0;
    for (std::size_t i = 0; i < n; ++i)
        v += w[i];
    return v;
}
Fr relation(unsigned kind, const Fr &x, const Fr &selector, const Fr &t, const Fr &mask) {
    if (kind == 0)
        return x - selector * t - Fr(K) * (mask - selector);
    return x - selector * (Fr(K - 1) - t) - (mask - selector) * (t - Fr(K));
}
void statement(const ExpSpec &s, const Commitment &ct, const Commitment &ce, const ExpProof &p,
               Fs &t) {
    s.validate();
    require(ct.spec == s.index && ce.spec == s.value, "Exp source PC");
    t.text("exp.instance", s.profile==ExpProfile::Unclamped ?
        "unclamped-u32-St2^-9-Se2^-20-v1" : "legacy-clamp1-u32-St2^-9-Se2^-20-v1");
    t.text("exp.id", s.id);
    const auto digest = expTableDigest(s.profile);
    t.bytes("exp.table.digest", Bytes(digest.begin(), digest.end()));
    append(t, ct);
    append(t, ce);
    for (unsigned k = 0; k < 4; ++k) {
        require(p.helper[k].spec == helperSpec(s, k), "Exp helper PC");
        append(t, p.helper[k]);
    }
}
} // namespace
void ExpSpec::validate() const {
    require(profile==ExpProfile::Unclamped || profile==ExpProfile::LegacyClampOne,"unsupported Exp profile");
    index.validate();
    value.validate();
    require(!id.empty() && id.size() <= 35, "Exp ID");
    require(index.actual == value.actual && index.padded == value.padded, "Exp shape");
}
PCSpec expHelperSpec(const ExpSpec &s, unsigned k) {
    s.validate();
    return helperSpec(s, k);
}
RangeSpec expRangeSpec(const ExpSpec &s, unsigned k) {
    s.validate();
    return rangeSpec(s, k);
}
MemorySpec expMemorySpec(const ExpSpec &s) {
    s.validate();
    return memorySpec(s);
}
std::uint32_t expValue(std::uint32_t t,ExpProfile profile) {
    require(profile==ExpProfile::Unclamped || profile==ExpProfile::LegacyClampOne,"unsupported Exp profile");
    auto v=kUnclampedExp[t<K?t:K];return profile==ExpProfile::LegacyClampOne && v==0 ? 1 : v;
}
const std::vector<Fr> &expTable(ExpProfile profile) {
    require(profile==ExpProfile::Unclamped || profile==ExpProfile::LegacyClampOne,"unsupported Exp profile");
    static const std::array<std::vector<Fr>,2> tables=[] {
        std::array<std::vector<Fr>,2> v;
        for (unsigned p=0;p<2;++p)for(unsigned t=0;t<=K;++t)v[p].emplace_back(expValue(t,static_cast<ExpProfile>(p)));
        return v;
    }();return tables[unsigned(profile)];
}
Digest expTableDigest(ExpProfile profile) {
    require(profile==ExpProfile::Unclamped || profile==ExpProfile::LegacyClampOne,"unsupported Exp profile");
    static const std::array<Digest,2> digests=[] {
        std::array<Digest,2> d;
        for (unsigned p=0;p<2;++p){Bytes raw;raw.reserve(8192*4);
            for(unsigned t=0;t<=K;++t){auto x=expValue(t,static_cast<ExpProfile>(p));for(unsigned b=0;b<4;++b)raw.push_back(std::uint8_t(x>>(8*b)));}
            d[p]=hash(raw);
        }return d;
    }();return digests[unsigned(profile)];
}
#ifndef R1_VERIFY_ONLY
ExpProof proveExp(const ExpSpec &s, const Commitment &ct, const Commitment &ce,
                  const std::vector<Fr> &index, const std::vector<Fr> &value, Fs &t) {
    s.validate();
    require(index.size() == s.index.padded && value.size() == index.size(), "Exp witness shape");
    ExpProof p;
    std::array<std::vector<Fr>, 4> h;
    for (auto &v : h)
        v.resize(index.size(), Fr(0));
    for (std::size_t i = 0; i < index.size(); ++i) {
        if (i >= s.index.actual) {
            h[3][i] = value[i] + Fr(Scale);
            continue;
        }
        auto bytes = encode(index[i]);
        for (unsigned b = 4; b < 32; ++b)
            require(!bytes[b], "Exp index outside u32");
        std::uint32_t raw = 0;
        for (unsigned b = 0; b < 4; ++b)
            raw |= std::uint32_t(bytes[b]) << (b * 8);
        h[0][i] = Fr(raw < K ? raw : K);
        h[1][i] = Fr(raw < K ? 1 : 0);
        h[2][i] = Fr(raw < K ? K - 1 - raw : raw - K);
        h[3][i] = value[i];
    }
    for (unsigned k = 0; k < 4; ++k)
        p.helper[k] = commit(helperSpec(s, k), h[k]);
    statement(s, ct, ce, p, t);
    p.range[0] = proveRange(rangeSpec(s, 0), ct, index, t);
    for (unsigned k = 1; k < 4; ++k)
        p.range[k] = proveRange(rangeSpec(s, k), p.helper[k - 1], h[k - 1], t);
    std::vector<Fr> mask(index.size(), Fr(0));
    for (std::size_t i = 0; i < s.index.actual; ++i)
        mask[i] = Fr(1);
    for (unsigned kind = 0; kind < 2; ++kind) {
        auto row = point(t, log2exact(index.size()), "exp.relation/" + std::to_string(kind));
        unsigned x = kind == 0 ? 0 : 2;
        auto end = proveSumcheck(
            {weights(row), h[x], h[1], index, mask}, 3,
            [&](const auto &v) { return v[0] * relation(kind, v[1], v[2], v[3], v[4]); },
            p.constraints[kind], t, "exp.branch/" + std::to_string(kind));
        p.endpoint[kind][0] = open(s.index, ct, index, end.point, t, "exp.branch.t");
        p.endpoint[kind][1] =
            open(helperSpec(s, 1), p.helper[1], h[1], end.point, t, "exp.branch.selector");
        p.endpoint[kind][2] =
            open(helperSpec(s, x), p.helper[x], h[x], end.point, t, "exp.branch.helper");
    }
    auto r = point(t, log2exact(index.size()), "exp.value.binding");
    p.value_binding[0] = open(s.value, ce, value, r, t, "exp.original-E");
    p.value_binding[1] = open(helperSpec(s, 3), p.helper[3], h[3], r, t, "exp.native-E");
    p.memory = proveMemory(memorySpec(s), p.helper[0], p.helper[3], h[0], h[3], t);
    return p;
}
#endif
void verifyExp(const ExpSpec &s, const Commitment &ct, const Commitment &ce, const ExpProof &p,
               Fs &t) {
    statement(s, ct, ce, p, t);
    verifyRange(rangeSpec(s, 0), ct, p.range[0], t);
    for (unsigned k = 1; k < 4; ++k)
        verifyRange(rangeSpec(s, k), p.helper[k - 1], p.range[k], t);
    for (unsigned kind = 0; kind < 2; ++kind) {
        auto row = point(t, log2exact(s.index.padded), "exp.relation/" + std::to_string(kind));
        auto end = verifySumcheck(log2exact(s.index.padded), 3, Fr(0), p.constraints[kind], t,
                                  "exp.branch/" + std::to_string(kind));
        unsigned x = kind == 0 ? 0 : 2;
        const auto &ops = p.endpoint[kind];
        verify(s.index, ct, end.point, ops[0], t, "exp.branch.t");
        verify(helperSpec(s, 1), p.helper[1], end.point, ops[1], t, "exp.branch.selector");
        verify(helperSpec(s, x), p.helper[x], end.point, ops[2], t, "exp.branch.helper");
        require(end.value == equality(row, end.point) * relation(kind, ops[2].value, ops[1].value,
                                                                 ops[0].value,
                                                                 active(s.index.actual, end.point)),
                "Exp branch endpoint");
    }
    auto r = point(t, log2exact(s.index.padded), "exp.value.binding");
    verify(s.value, ce, r, p.value_binding[0], t, "exp.original-E");
    verify(helperSpec(s, 3), p.helper[3], r, p.value_binding[1], t, "exp.native-E");
    require(p.value_binding[1].value ==
                p.value_binding[0].value + Fr(Scale) * (Fr(1) - active(s.index.actual, r)),
            "Exp original/native padding binding");
    verifyMemory(memorySpec(s), p.helper[0], p.helper[3], p.memory, t);
}
} // namespace zkgpt::r1
