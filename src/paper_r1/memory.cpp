#include "memory.hpp"
namespace zkgpt::r1 {
void MemorySpec::validate() const {
    address.validate();
    value.validate();
    require(address.padded == value.padded && address.actual == value.actual, "memory query shape");
    log2exact(table.size());
    require(table.size() <= (std::size_t(1) << 20), "complete table bound");
    require(!id.empty() && id.size() <= 80, "memory ID");
    PCSpec::make(id + "/read", address.padded, address.padded, address.layout);
}
PCSpec readSpec(const MemorySpec &s) {
    return PCSpec::make(s.id + "/read", s.address.padded, s.address.padded, s.address.layout);
}
PCSpec finalSpec(const MemorySpec &s) {
    return PCSpec::make(s.id + "/final", s.table.size(), s.table.size(), s.address.layout);
}
static void identity(const MemorySpec &s, const Commitment &ca, const Commitment &cv,
                     const MemoryProof &p, Fs &t) {
    s.validate();
    require(ca.spec == s.address && cv.spec == s.value, "memory expected query PC");
    require(p.read.spec == readSpec(s) && p.final.spec == finalSpec(s),
            "memory expected counter PC");
    t.text("lasso.kind", "complete-indexed-c1-v1");
    t.text("lasso.id", s.id);
    t.integer("lasso.table.size", s.table.size());
    for (auto &v : s.table)
        t.scalar("lasso.table", v);
    append(t, ca);
    append(t, cv);
    append(t, p.read);
    append(t, p.final);
}
static Fr fp(const Fr &a, const Fr &v, const Fr &c, const Fr &gamma, const Fr &tau) {
    return gamma * gamma * a + gamma * v + c - tau;
}
#ifndef R1_VERIFY_ONLY
static std::size_t asIndex(const Fr &a, std::size_t bound) {
    auto b = encode(a);
    std::uint64_t n = 0;
    for (unsigned i = 0; i < 8; ++i)
        n |= std::uint64_t(b[i]) << (8 * i);
    for (unsigned i = 8; i < 32; ++i)
        require(!b[i], "address not canonical integer index");
    require(n < bound, "address outside complete table");
    return std::size_t(n);
}
#endif

static Fr publicAddress(const std::vector<Fr> &r) {
    Fr value = 0, scale = 1;
    for (auto &bit : r) {
        value += scale * bit;
        scale *= Fr(2);
    }
    return value;
}
static Fr initial(const MemorySpec &s, const Fr &g, const Fr &tau) {
    Fr p = 1;
    for (std::size_t i = 0; i < s.table.size(); ++i)
        p *= fp(Fr(i), s.table[i], Fr(0), g, tau);
    return p;
}
#ifndef R1_VERIFY_ONLY
MemoryProof proveMemory(const MemorySpec &s, const Commitment &ca, const Commitment &cv,
                        const std::vector<Fr> &a, const std::vector<Fr> &v, Fs &t) {
    s.validate();
    require(a.size() == s.address.padded && v.size() == s.value.padded, "memory witness length");
    std::vector<Fr> read(a.size()), final(s.table.size(), Fr(0));
    std::vector<std::size_t> count(s.table.size(), 0);
    for (std::size_t i = 0; i < a.size(); ++i) {
        auto j = asIndex(a[i], s.table.size());
        require(v[i] == s.table[j], "memory witness value");
        read[i] = Fr(count[j]++);
    }
    for (std::size_t j = 0; j < count.size(); ++j)
        final[j] = Fr(count[j]);
    MemoryProof p;
    p.read = commit(readSpec(s), read);
    p.final = commit(finalSpec(s), final);
    identity(s, ca, cv, p, t);
    auto g = t.challenge("memory.gamma"), tau = t.challenge("memory.tau");
    std::array<std::vector<Fr>, 3> leaves;
    for (unsigned kind = 0; kind < 3; ++kind) {
        auto n = kind == 2 ? s.table.size() : a.size();
        leaves[kind].resize(n);
        for (std::size_t i = 0; i < n; ++i)
            leaves[kind][i] = kind == 2 ? fp(Fr(i), s.table[i], final[i], g, tau)
                                        : fp(a[i], v[i], read[i] + Fr(kind == 0 ? 1 : 0), g, tau);
        p.roots[kind] = product(leaves[kind]);
        t.scalar("memory.root", p.roots[kind]);
    }
    for (unsigned kind = 0; kind < 3; ++kind) {
        auto l = s.id + "/product/" + std::to_string(kind);
        auto end = proveProduct(leaves[kind], p.products[kind].tree, t, l);
        auto &ops = p.products[kind].leaf;
        if (kind == 2)
            ops.push_back(open(finalSpec(s), p.final, final, end.point, t, l + "/final"));
        else {
            ops.push_back(open(s.address, ca, a, end.point, t, l + "/address"));
            ops.push_back(open(s.value, cv, v, end.point, t, l + "/value"));
            ops.push_back(open(readSpec(s), p.read, read, end.point, t, l + "/read"));
        }
    }
    return p;
}
#endif

void verifyMemory(const MemorySpec &s, const Commitment &ca, const Commitment &cv,
                  const MemoryProof &p, Fs &t) {
    identity(s, ca, cv, p, t);
    auto g = t.challenge("memory.gamma"), tau = t.challenge("memory.tau");
    for (auto &root : p.roots)
        t.scalar("memory.root", root);
    require(initial(s, g, tau) * p.roots[0] == p.roots[1] * p.roots[2], "memory multiset equation");
    for (unsigned kind = 0; kind < 3; ++kind) {
        auto n = kind == 2 ? s.table.size() : s.address.padded;
        auto l = s.id + "/product/" + std::to_string(kind);
        const auto &proof = p.products[kind];
        auto end = verifyProduct(n, p.roots[kind], proof.tree, t, l);
        const auto &ops = proof.leaf;
        require(ops.size() == (kind == 2 ? 1 : 3), "memory leaf count");
        Fr expected;
        if (kind == 2) {
            verify(finalSpec(s), p.final, end.point, ops[0], t, l + "/final");
            expected = fp(publicAddress(end.point), mle(s.table, end.point), ops[0].value, g, tau);
        } else {
            verify(s.address, ca, end.point, ops[0], t, l + "/address");
            verify(s.value, cv, end.point, ops[1], t, l + "/value");
            verify(readSpec(s), p.read, end.point, ops[2], t, l + "/read");
            expected = fp(ops[0].value, ops[1].value, ops[2].value + Fr(kind == 0 ? 1 : 0), g, tau);
        }
        require(end.value == expected, "memory authenticated leaf");
    }
}
} // namespace zkgpt::r1
