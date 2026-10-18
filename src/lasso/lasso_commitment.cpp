#include "lasso_commitment.hpp"
#include "lasso_table.hpp"
#include <map>
#include <mutex>
namespace route_b::lasso {
const GeneratorSet &lassoGenerators(std::size_t n) {
    require(Fr::getModulo() == kFieldModulus, "runtime field is not project MCL BN254");
    log2Exact(n);
    require(n <= kMaxSize, "generator bound");
    static std::mutex mutex;
    static std::map<std::size_t, GeneratorSet> cache;
    std::lock_guard<std::mutex> lock(mutex);
    auto found = cache.find(n);
    if (found != cache.end())
        return found->second;
    GeneratorSet g;
    g.generators.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        hashAndMapToG1(g.generators[i], std::string(kGenerators) + "/g/" + std::to_string(i));
        require(g.generators[i].isValid() && g.generators[i].isValidOrder() &&
                    !g.generators[i].isZero(),
                "Lasso generator derivation");
    }
    hashAndMapToG1(g.u, std::string(kGenerators) + "/u");
    require(g.u.isValid() && g.u.isValidOrder() && !g.u.isZero(), "Lasso U derivation");
    return cache.emplace(n, std::move(g)).first->second;
}
static const GeneratorSet &generators(std::size_t n) { return lassoGenerators(n); }
G1 commit(const std::vector<Fr> &v) {
    auto &g = generators(v.size());
    G1 c;
    G1::mulVec(c, const_cast<G1 *>(g.generators.data()), v.data(), v.size());
    return c;
}
Opening openLinear(const std::vector<Fr> &v, const G1 &c, const std::vector<Fr> &b, Transcript &t,
                   const std::string &l) {
    require(v.size() == b.size(), "opening shape");
    auto &g = generators(v.size());
    Opening o;
    for (std::size_t i = 0; i < v.size(); ++i)
        o.value += v[i] * b[i];
    o.ipa = hyraxInnerProductProve(v, b, g.generators, g.u, c, o.value, t, l);
    return o;
}
bool verifyLinear(const G1 &c, const std::vector<Fr> &b, const Opening &o, Transcript &t,
                  const std::string &l) {
    auto &g = generators(b.size());
    require(o.ipa.rounds.size() == log2Exact(b.size()), "IPA round count");
    return hyraxInnerProductVerify(b, g.generators, g.u, c, o.value, o.ipa, t, l);
}
Opening openMle(const std::vector<Fr> &v, const G1 &c, const std::vector<Fr> &p, Transcript &t,
                const std::string &l) {
    return openLinear(v, c, eqWeights(p), t, l);
}
bool verifyMle(const G1 &c, const std::vector<Fr> &p, const Opening &o, Transcript &t,
               const std::string &l) {
    return verifyLinear(c, eqWeights(p), o, t, l);
}
void appendCommitments(Transcript &t, const LassoLookupInstance &x, const LassoProof &p) {
    auto d = instanceDigest(x);
    t.appendBytes("instance", d.data(), d.size());
    t.appendG1("query.C", p.query_commitment);
    t.appendU64("dim.count", p.dim_commitments.size());
    for (auto &c : p.dim_commitments)
        t.appendG1("dim.C", c);
}
std::vector<Fr> challengePoint(Transcript &t, unsigned b, const std::string &l) {
    std::vector<Fr> r;
    for (unsigned i = 0; i < b; ++i)
        r.push_back(t.challenge(l + "." + std::to_string(i)));
    return r;
}
} // namespace route_b::lasso
