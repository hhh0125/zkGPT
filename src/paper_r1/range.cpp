#include "range.hpp"
#include <algorithm>
namespace zkgpt::r1 {
static void linearStatement(const PCSpec &s, const Commitment &c, const std::vector<Fr> &a,
                            const Fr &claim, Fs &t, const std::string &l) {
    require(c.spec == s && a.size() == s.padded, "linear statement shape");
    t.text("linear.label", l);
    append(t, c);
    t.integer("linear.coefficient.count", a.size());
    for (auto &f : a)
        t.scalar("linear.coefficient", f);
    t.scalar("linear.claim", claim);
}
#ifndef R1_VERIFY_ONLY
LinearProof proveLinear(const PCSpec &s, const Commitment &c, const std::vector<Fr> &w,
                        const std::vector<Fr> &a, const Fr &claim, Fs &t, const std::string &l) {
    require(w.size() == s.padded && a.size() == w.size() && dot(a, w) == claim,
            "linear witness claim");
    linearStatement(s, c, a, claim, t, l);
    LinearProof p;
    auto end = proveSumcheck(
        {a, w}, 2, [](const auto &v) { return v[0] * v[1]; }, p.sumcheck, t, l);
    p.endpoint = open(s, c, w, end.point, t, l + "/endpoint");
    return p;
}
#endif

void verifyLinear(const PCSpec &s, const Commitment &c, const std::vector<Fr> &a, const Fr &claim,
                  const LinearProof &p, Fs &t, const std::string &l) {
    linearStatement(s, c, a, claim, t, l);
    auto end = verifySumcheck(log2exact(s.padded), 2, claim, p.sumcheck, t, l);
    verify(s, c, end.point, p.endpoint, t, l + "/endpoint");
    require(end.value == mle(a, end.point) * p.endpoint.value, "linear endpoint");
}
static std::vector<Fr> tailWeights(const PCSpec &s, const Commitment &c, Fs &t) {
    require(s == c.spec, "tail PC identity");
    t.text("tail.check", "random-MLE-zero-tail-v1");
    append(t, c);
    auto a = weights(point(t, log2exact(s.padded), "tail.random"));
    std::fill(a.begin(), a.begin() + s.actual, Fr(0));
    return a;
}
#ifndef R1_VERIFY_ONLY
LinearProof proveZeroTail(const PCSpec &s, const Commitment &c, const std::vector<Fr> &w, Fs &t) {
    auto a = tailWeights(s, c, t);
    return proveLinear(s, c, w, a, Fr(0), t, "zero-tail");
}
#endif

void verifyZeroTail(const PCSpec &s, const Commitment &c, const LinearProof &p, Fs &t) {
    auto a = tailWeights(s, c, t);
    verifyLinear(s, c, a, Fr(0), p, t, "zero-tail");
}
void RangeSpec::validate() const {
    query.validate();
    require(bits >= 1 && bits <= 126, "Range bitwidth");
    require(!id.empty() && id.size() <= 55, "Range ID");
    PCSpec::make(id + "/digit/0", query.actual, query.padded, query.layout);
}
PCSpec digitSpec(const RangeSpec &s, unsigned h) {
    s.validate();
    require(h < (s.bits + 8) / 9, "Range digit index");
    return PCSpec::make(s.id + "/digit/" + std::to_string(h), s.query.actual, s.query.padded,
                        s.query.layout);
}
MemorySpec digitMemory(const RangeSpec &s, unsigned h) {
    auto ds = digitSpec(s, h);
    unsigned bits = std::min(9U, s.bits - 9 * h);
    std::vector<Fr> table(std::size_t(1) << bits);
    for (std::size_t i = 0; i < table.size(); ++i)
        table[i] = Fr(i);
    return {s.id + "/memory/" + std::to_string(h), ds, ds, std::move(table)};
}
std::vector<Fr> rangeRowPoint(const RangeSpec &s, const Commitment &c, const RangeProof &p, Fs &t) {
    s.validate();
    require(c.spec == s.query && p.digits.size() == (s.bits + 8) / 9, "Range identity/count");
    t.text("range.kind", "structured-additive-identity-Lasso-v1");
    t.text("range.id", s.id);
    t.integer("range.bits", s.bits);
    append(t, c);
    for (unsigned h = 0; h < p.digits.size(); ++h) {
        require(p.digits[h].spec == digitSpec(s, h), "Range expected digit PC");
        append(t, p.digits[h]);
    }
    return point(t, log2exact(s.query.padded), "range.row");
}
#ifndef R1_VERIFY_ONLY
RangeProof proveRange(const RangeSpec &s, const Commitment &c, const std::vector<Fr> &w, Fs &t) {
    s.validate();
    require(w.size() == s.query.padded, "Range witness shape");
    auto count = (s.bits + 8) / 9;
    std::vector<std::vector<Fr>> digits(count, std::vector<Fr>(w.size()));
    for (std::size_t i = 0; i < w.size(); ++i) {
        auto b = encode(w[i]);
        for (unsigned bit = s.bits; bit < 256; ++bit)
            require(!(b[bit / 8] & (1U << (bit % 8))), "Range witness outside bitwidth");
        require(i < s.query.actual || w[i].isZero(), "Range witness nonzero padding");
        for (unsigned h = 0; h < count; ++h) {
            unsigned d = 0;
            for (unsigned j = 0; j < std::min(9U, s.bits - h * 9); ++j) {
                unsigned k = h * 9 + j;
                d |= unsigned((b[k / 8] >> (k % 8)) & 1) << j;
            }
            digits[h][i] = Fr(d);
        }
    }
    RangeProof p;
    for (unsigned h = 0; h < count; ++h)
        p.digits.push_back(commit(digitSpec(s, h), digits[h]));
    auto r = rangeRowPoint(s, c, p, t);
    p.query = open(s.query, c, w, r, t, "range.query");
    for (unsigned h = 0; h < count; ++h)
        p.digit_values.push_back(open(digitSpec(s, h), p.digits[h], digits[h], r, t,
                                      "range.digit/" + std::to_string(h)));
    for (unsigned h = 0; h < count; ++h)
        p.memories.push_back(
            proveMemory(digitMemory(s, h), p.digits[h], p.digits[h], digits[h], digits[h], t));
    p.tail = proveZeroTail(s.query, c, w, t);
    return p;
}
#endif

void verifyRange(const RangeSpec &s, const Commitment &c, const RangeProof &p, Fs &t) {
    auto r = rangeRowPoint(s, c, p, t);
    auto count = p.digits.size();
    require(p.digit_values.size() == count && p.memories.size() == count, "Range proof shape");
    verify(s.query, c, r, p.query, t, "range.query");
    Fr reconstructed = 0, scale = 1;
    for (unsigned h = 0; h < count; ++h) {
        verify(digitSpec(s, h), p.digits[h], r, p.digit_values[h], t,
               "range.digit/" + std::to_string(h));
        reconstructed += scale * p.digit_values[h].value;
        scale *= Fr(512);
    }
    require(reconstructed == p.query.value, "Range MLE reconstruction");
    for (unsigned h = 0; h < count; ++h)
        verifyMemory(digitMemory(s, h), p.digits[h], p.digits[h], p.memories[h], t);
    verifyZeroTail(s.query, c, p.tail, t);
}
static std::vector<Fr> bindingPoint(const RangeSpec &s, const PCSpec &source, const Commitment &cs,
                                    const Commitment &cq, bool sign, Fs &t) {
    s.validate();
    require(source.actual == s.query.actual && source.padded == s.query.padded &&
                cs.spec == source && cq.spec == s.query,
            "Range binding shape");
    t.text("binding.kind", "signed-active-mask-v1");
    t.integer("binding.bits", s.bits);
    t.integer("binding.signed", sign);
    append(t, cs);
    append(t, cq);
    return point(t, log2exact(source.padded), "binding.row");
}
static Fr biasValue(unsigned bits) {
    Fr b = 1;
    for (unsigned i = 1; i < bits; ++i)
        b *= Fr(2);
    return b;
}
static Fr activeValue(std::size_t actual, const std::vector<Fr> &r) {
    auto a = weights(r);
    Fr v = 0;
    for (std::size_t i = 0; i < actual; ++i)
        v += a[i];
    return v;
}
#ifndef R1_VERIFY_ONLY
RangeBindingProof proveRangeBinding(const RangeSpec &s, const PCSpec &source, const Commitment &cs,
                                    const Commitment &cq, const std::vector<Fr> &raw,
                                    const std::vector<Fr> &q, bool sign, Fs &t) {
    auto r = bindingPoint(s, source, cs, cq, sign, t);
    RangeBindingProof p;
    p.source = open(source, cs, raw, r, t, "binding.source");
    p.encoded = open(s.query, cq, q, r, t, "binding.encoded");
    require(p.encoded.value ==
                p.source.value + (sign ? biasValue(s.bits) * activeValue(source.actual, r) : Fr(0)),
            "Range affine source binding");
    return p;
}
#endif

void verifyRangeBinding(const RangeSpec &s, const PCSpec &source, const Commitment &cs,
                        const Commitment &cq, bool sign, const RangeBindingProof &p, Fs &t) {
    auto r = bindingPoint(s, source, cs, cq, sign, t);
    verify(source, cs, r, p.source, t, "binding.source");
    verify(s.query, cq, r, p.encoded, t, "binding.encoded");
    require(p.encoded.value ==
                p.source.value + (sign ? biasValue(s.bits) * activeValue(source.actual, r) : Fr(0)),
            "Range affine source binding");
}
} // namespace zkgpt::r1
