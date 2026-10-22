#include "bundle.hpp"
namespace zkgpt::r1 {
void BundleStatement::validate() const {
    input.validate();
    gkr.validate();
    range.validate();
    indexed.validate();
    require(input.x.size() * 4 == gkr.input_size && input.context == gkr.context,
            "bundle input/circuit identity");
    require(range.query == input.spec[2] && indexed.address == input.spec[1] &&
                indexed.value == input.spec[2],
            "bundle same-witness PC identity");
}
static Fs start(const BundleStatement &s) {
    s.validate();
    Fs t("zkGPT.paper-r1.foundation-bundle.v1");
    appendInputStatement(s.input, t);
    appendGkrStatement(s.gkr, t);
    t.text("bundle.range.id", s.range.id);
    t.integer("bundle.range.bits", s.range.bits);
    t.text("bundle.indexed.id", s.indexed.id);
    t.integer("bundle.table.size", s.indexed.table.size());
    for (auto &f : s.indexed.table)
        t.scalar("bundle.table", f);
    return t;
}
static InputClaim lookupClaim(std::vector<Fr> r, const Fr &value) {
    r.push_back(Fr(1));
    r.push_back(Fr(1));
    return {std::move(r), value};
}
#ifndef R1_VERIFY_ONLY
BundleProof proveBundle(const BundleStatement &s, const std::array<std::vector<Fr>, 3> &w) {
    auto t = start(s);
    BundleProof p;
    Fs range_start = t;
    p.range = proveRange(s.range, s.input.commitment[2], w[2], t);
    auto rr = rangeRowPoint(s.range, s.input.commitment[2], p.range, range_start);
    auto ir = point(t, log2exact(s.input.x.size()), "bundle.indexed.row");
    p.indexed_value =
        open(s.input.spec[2], s.input.commitment[2], w[2], ir, t, "bundle.indexed.value");
    p.memory = proveMemory(s.indexed, s.input.commitment[1], s.input.commitment[2], w[1], w[2], t);
    std::vector<Fr> input = s.input.x;
    for (auto &b : w)
        input.insert(input.end(), b.begin(), b.end());
    Fs gkr_start = t;
    p.gkr = proveGkr(s.gkr, input, t);
    auto claims = verifyGkr(s.gkr, p.gkr, gkr_start);
    require(gkr_start.digest() == t.digest(), "prover GKR replay");
    claims.push_back(lookupClaim(rr, p.range.query.value));
    claims.push_back(lookupClaim(ir, p.indexed_value.value));
    p.input = proveInput(s.input, w, claims, t);
    return p;
}
#endif

void verifyBundle(const BundleStatement &s, const BundleProof &p) {
    auto t = start(s);
    Fs range_start = t;
    verifyRange(s.range, s.input.commitment[2], p.range, t);
    auto rr = rangeRowPoint(s.range, s.input.commitment[2], p.range, range_start);
    auto ir = point(t, log2exact(s.input.x.size()), "bundle.indexed.row");
    verify(s.input.spec[2], s.input.commitment[2], ir, p.indexed_value, t, "bundle.indexed.value");
    verifyMemory(s.indexed, s.input.commitment[1], s.input.commitment[2], p.memory, t);
    auto claims = verifyGkr(s.gkr, p.gkr, t);
    claims.push_back(lookupClaim(rr, p.range.query.value));
    claims.push_back(lookupClaim(ir, p.indexed_value.value));
    verifyInput(s.input, claims, p.input, t);
}
} // namespace zkgpt::r1
