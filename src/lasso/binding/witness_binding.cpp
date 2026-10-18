#include "witness_binding.hpp"
#include "lasso_commitment.hpp"
#include "lasso_prover.hpp"
#include "lasso_serialization.hpp"
#include "lasso_table.hpp"
#include "lasso_verifier.hpp"
#include <algorithm>
#include <cybozu/sha2.hpp>
#include <set>
namespace route_b::binding {
namespace {
constexpr const char *domain = "zkGPT.route-b.witness-binding.v1";
Digest hash(const Bytes &b) {
  Digest d{};
  cybozu::Sha256 h;
  require(h.digest(d.data(), 32, b.data(), b.size()) == 32, "hash");
  return d;
}
std::size_t capacity(const WitnessLayoutDescriptor &d) {
  return paddedSize(d.witness_length);
}
std::size_t columns(const WitnessLayoutDescriptor &d) {
  auto l = log2Exact(capacity(d));
  return std::size_t{1} << (l - l / 2);
}
void writeLayout(Writer &w, const WitnessLayoutDescriptor &d) {
  w.u32(d.version);
  w.u32(d.field);
  w.u32(d.curve);
  w.u32(d.backend);
  w.u32(d.padding);
  w.u32(d.witness_length);
  w.string(d.model_profile);
  w.string(d.circuit_id);
  w.u32(d.regions.size());
  for (auto &r : d.regions) {
    w.string(r.name);
    w.u32(r.offset);
    w.u32(r.count);
  }
  w.u32(d.queries.size());
  for (auto &q : d.queries) {
    w.string(q.id);
    w.string(q.kind);
    w.u32(q.allow_alias);
    w.u32(q.positions.size());
    for (auto p : q.positions) {
      w.u32(p.region);
      w.u32(p.index);
    }
  }
}
void validateInstance(const WitnessBindingInstance &x) {
  validateLayout(x.layout);
  require(x.selected_query < x.layout.queries.size(), "selected query");
  route_b::lasso::validateInstance(x.lookup);
  require(x.lookup.query_length ==
              x.layout.queries[x.selected_query].positions.size(),
          "query length");
  require(x.c0.size() == capacity(x.layout) / columns(x.layout),
          "C0 row count");
  for (auto &c : x.c0)
    require(c.isValid() && c.isValidOrder(), "C0 point");
}
Transcript start(const WitnessBindingInstance &x, const LassoProof &p) {
  Transcript t(domain);
  auto d = bindingInstanceDigest(x);
  t.appendBytes("statement", d.data(), 32);
  t.appendG1("query.C", p.query_commitment);
  return t;
}
std::vector<std::vector<Fr>> coefficients(const WitnessBindingInstance &x,
                                          const std::vector<Fr> &r) {
  auto b = eqWeights(r);
  auto k = columns(x.layout);
  std::vector<std::vector<Fr>> a(x.c0.size(), std::vector<Fr>(k, Fr(0)));
  auto pi = mapping(x);
  for (std::size_t i = 0; i < pi.size(); ++i)
    a[pi[i] / k][pi[i] % k] += b[i];
  return a;
}
Fr paddingValue(const WitnessBindingInstance &x, const std::vector<Fr> &r) {
  auto b = eqWeights(r);
  Fr v = 0;
  for (std::size_t i = x.lookup.query_length; i < b.size(); ++i)
    v += b[i] * x.lookup.table[0];
  return v;
}
// Bounded canonical reader, independent from the private B1 reader.
class Reader {
  const Bytes &b;
  std::size_t pos = 0;

public:
  explicit Reader(const Bytes &v) : b(v) {
    require(b.size() <= kMaxArtifact, "artifact size cap");
  }
  Bytes raw(std::size_t n) {
    require(n <= b.size() - pos, "truncated B2");
    Bytes v(b.begin() + pos, b.begin() + pos + n);
    pos += n;
    return v;
  }
  std::uint32_t u32() {
    auto v = raw(4);
    std::uint32_t x = 0;
    for (unsigned i = 0; i < 4; ++i)
      x |= std::uint32_t(v[i]) << (8 * i);
    return x;
  }
  std::size_t count(std::size_t n) {
    auto x = u32();
    require(x <= n, "B2 length cap");
    return x;
  }
  std::string str() {
    auto v = raw(count(64));
    return {v.begin(), v.end()};
  }
  void tag(std::uint32_t n) { require(u32() == n, "B2 field order"); }
  Digest dig() {
    auto v = raw(32);
    Digest d;
    std::copy(v.begin(), v.end(), d.begin());
    return d;
  }
  template <class T> T scalar() {
    auto v = raw(32);
    T x;
    require(x.deserialize(v.data(), 32) == 32, "B2 invalid scalar");
    std::uint8_t c[32];
    require(x.serialize(c, 32) == 32 && std::equal(v.begin(), v.end(), c),
            "B2 noncanonical scalar");
    return x;
  }
  G1 point() {
    auto x = scalar<G1>();
    require(x.isValid() && x.isValidOrder(), "B2 subgroup");
    return x;
  }
  void end() { require(pos == b.size(), "B2 trailing bytes"); }
};
WitnessLayoutDescriptor readLayout(Reader &r) {
  WitnessLayoutDescriptor d;
  d.version = r.u32();
  d.field = r.u32();
  d.curve = r.u32();
  d.backend = r.u32();
  d.padding = r.u32();
  d.witness_length = r.u32();
  d.model_profile = r.str();
  d.circuit_id = r.str();
  auto n = r.count(kMaxSize);
  while (n--) {
    Region v;
    v.name = r.str();
    v.offset = r.u32();
    v.count = r.u32();
    d.regions.push_back(v);
  }
  n = r.count(16);
  while (n--) {
    QueryLayout q;
    q.id = r.str();
    q.kind = r.str();
    auto alias = r.u32();
    require(alias <= 1, "B2 alias encoding");
    q.allow_alias = alias;
    auto m = r.count(kMaxSize);
    while (m--) {
      Position p;
      p.region = r.u32();
      p.index = r.u32();
      q.positions.push_back(p);
    }
    d.queries.push_back(q);
  }
  return d;
}
void writeOpening(Writer &w, const Opening &o) {
  w.fr(o.value);
  require(o.ipa.rounds.size() <= kMaxBits, "B2 IPA cap");
  w.u32(o.ipa.rounds.size());
  for (auto &r : o.ipa.rounds) {
    w.g1(r.left);
    w.g1(r.right);
  }
  w.fr(o.ipa.final_witness);
}
Opening readOpening(Reader &r) {
  Opening o;
  o.value = r.scalar<Fr>();
  auto n = r.count(kMaxBits);
  while (n--) {
    HyraxIpaRound p;
    p.left = r.point();
    p.right = r.point();
    o.ipa.rounds.push_back(p);
  }
  o.ipa.final_witness = r.scalar<Fr>();
  return o;
}
} // namespace
void validateLayout(const WitnessLayoutDescriptor &d) {
  require(d.version == 1 && d.field == 1 && d.curve == 1 && d.backend == 1 &&
              d.padding == 1,
          "layout version/field/curve/backend/padding");
  capacity(d);
  require(!d.model_profile.empty() && d.model_profile.size() <= 64 &&
              !d.circuit_id.empty() && d.circuit_id.size() <= 64,
          "layout identities");
  require(!d.regions.empty() && d.regions.size() <= kMaxSize, "region count");
  std::set<std::string> names;
  for (std::size_t i = 0; i < d.regions.size(); ++i) {
    auto &a = d.regions[i];
    require(!a.name.empty() && a.name.size() <= 64 &&
                names.insert(a.name).second,
            "region identity");
    require(a.count && a.offset <= d.witness_length &&
                a.count <= d.witness_length - a.offset,
            "region bounds");
    for (std::size_t j = 0; j < i; ++j) {
      auto &b = d.regions[j];
      require(a.offset >= b.offset + b.count || b.offset >= a.offset + a.count,
              "region overlap");
    }
  }
  require(!d.queries.empty() && d.queries.size() <= 16, "lookup count");
  names.clear();
  for (auto &q : d.queries) {
    require(!q.id.empty() && q.id.size() <= 64 && names.insert(q.id).second,
            "lookup identity");
    require(q.kind == "raw-range" || q.kind == "exp-t" || q.kind == "exp-E" ||
                q.kind == "raw-test",
            "lookup kind");
    paddedSize(q.positions.size());
    std::set<std::size_t> seen;
    for (auto p : q.positions) {
      require(p.region < d.regions.size() &&
                  p.index < d.regions[p.region].count,
              "mapping bounds");
      auto idx = d.regions[p.region].offset + p.index;
      require(seen.insert(idx).second || q.allow_alias, "illegal alias");
    }
  }
}
WitnessLayoutDescriptor
layoutFromTrustedRegistry(const WitnessRegistry &registry, std::uint32_t length,
                          const std::string &profile,
                          const std::string &circuit) {
  paddedSize(length);
  require(registry.constraints().size() <= kMaxSize, "registry region cap");
  require(registry.expQueries().size() <= kMaxSize, "registry Exp cap");
  registry.validateLayout(length);
  WitnessLayoutDescriptor d;
  d.witness_length = length;
  d.model_profile = profile;
  d.circuit_id = circuit;
  for (const auto &c : registry.constraints()) {
    // Signed values remain outside the direct raw-query adapter. Their
    // presence must not prevent binding unsigned t/E in the same registry.
    if (c.is_signed)
      continue;
    require(c.val0_offset <= length && c.count <= length - c.val0_offset,
            "registry region bounds");
    paddedSize(c.count);
    require(d.queries.size() < 16, "registry lookup cap");
    auto region = std::uint32_t(d.regions.size());
    d.regions.push_back(
        {c.name, std::uint32_t(c.val0_offset), std::uint32_t(c.count)});
    QueryLayout q;
    q.id = c.name;
    q.kind = "raw-range";
    for (std::size_t i = 0; i < c.count; ++i)
      q.positions.push_back({region, std::uint32_t(i)});
    d.queries.push_back(q);
  }
  auto locate = [&](std::size_t idx) {
    for (std::size_t j = 0; j < d.regions.size(); ++j) {
      const auto &r = d.regions[j];
      if (idx >= r.offset && idx - r.offset < r.count)
        return Position{std::uint32_t(j), std::uint32_t(idx - r.offset)};
    }
    throw std::runtime_error(
        "registered Exp position outside approved regions");
  };
  if (!registry.expQueries().empty()) {
    QueryLayout t, e;
    t.id = "registered-exp-t";
    t.kind = "exp-t";
    e.id = "registered-exp-E";
    e.kind = "exp-E";
    std::size_t previous_op = 0, expected_query = 0;
    bool first = true;
    std::set<std::size_t> slots;
    for (auto p : registry.expQueries()) {
      if (first || p.operator_id != previous_op) {
        require(first || p.operator_id > previous_op, "Exp operator order");
        expected_query = 0;
        previous_op = p.operator_id;
        first = false;
      }
      require(p.query_id == expected_query++, "Exp query order");
      require(p.t_witness_index < length && p.E_witness_index < length,
              "Exp bounds");
      require(slots.insert(p.t_witness_index).second &&
                  slots.insert(p.E_witness_index).second,
              "Exp alias");
      t.positions.push_back(locate(p.t_witness_index));
      e.positions.push_back(locate(p.E_witness_index));
    }
    d.queries.push_back(t);
    d.queries.push_back(e);
  }
  validateLayout(d);
  return d;
}
std::vector<std::size_t> mapping(const WitnessBindingInstance &x) {
  validateInstance(x);
  std::vector<std::size_t> p;
  for (auto i : x.layout.queries[x.selected_query].positions)
    p.push_back(x.layout.regions[i.region].offset + i.index);
  return p;
}
Digest layoutDigest(const WitnessLayoutDescriptor &d) {
  validateLayout(d);
  Writer w;
  w.string("zkGPT.B2.layout.v1");
  writeLayout(w, d);
  return hash(w.bytes);
}
Digest c0Digest(const WitnessBindingInstance &x) {
  validateInstance(x);
  Writer w;
  w.string("zkGPT.B2.C0.main.row-major.v1");
  w.u32(x.layout.witness_length);
  w.u32(columns(x.layout));
  w.u32(x.c0.size());
  for (auto &c : x.c0)
    w.g1(c);
  return hash(w.bytes);
}
Bytes encodeBindingInstance(const WitnessBindingInstance &x) {
  validateInstance(x);
  Writer w;
  w.u32(0x32424931);
  writeLayout(w, x.layout);
  w.u32(x.selected_query);
  auto b = encodeInstance(x.lookup);
  w.u32(b.size());
  w.raw(b.data(), b.size());
  w.u32(x.c0.size());
  for (auto &c : x.c0)
    w.g1(c);
  return w.bytes;
}
WitnessBindingInstance decodeBindingInstance(const Bytes &b) {
  Reader r(b);
  r.tag(0x32424931);
  WitnessBindingInstance x;
  x.layout = readLayout(r);
  x.selected_query = r.u32();
  x.lookup = decodeInstance(r.raw(r.count(kMaxArtifact)));
  auto n = r.count(16);
  while (n--)
    x.c0.push_back(r.point());
  r.end();
  validateInstance(x);
  return x;
}
Digest bindingInstanceDigest(const WitnessBindingInstance &x) {
  return hash(encodeBindingInstance(x));
}
Bytes encodeBindingProof(const WitnessBindingProof &p) {
  Writer w;
  w.u32(0x32425031);
  w.tag(1);
  w.raw(p.statement_digest.data(), 32);
  auto b = encodeProof(p.lookup);
  w.u32(b.size());
  w.raw(b.data(), b.size());
  w.tag(2);
  writeOpening(w, p.query);
  w.tag(3);
  require(p.rows.size() <= 16, "B2 row cap");
  w.u32(p.rows.size());
  for (auto &o : p.rows)
    writeOpening(w, o);
  w.tag(4);
  return w.bytes;
}
WitnessBindingProof decodeBindingProof(const Bytes &b) {
  Reader r(b);
  r.tag(0x32425031);
  r.tag(1);
  WitnessBindingProof p;
  p.statement_digest = r.dig();
  p.lookup = decodeProof(r.raw(r.count(kMaxArtifact)));
  r.tag(2);
  p.query = readOpening(r);
  r.tag(3);
  auto n = r.count(16);
  while (n--)
    p.rows.push_back(readOpening(r));
  r.tag(4);
  r.end();
  return p;
}
std::vector<G1> commitWitness(const std::vector<Fr> &w) {
  auto n = paddedSize(w.size());
  auto l = log2Exact(n);
  auto k = std::size_t{1} << (l - l / 2);
  auto &g = getGeneratorSet({"zkGPT/main", std::uint32_t(k)});
  auto a = w;
  a.resize(n, Fr(0));
  std::vector<G1> rows(n / k);
  for (std::size_t i = 0; i < rows.size(); ++i)
    G1::mulVec(rows[i], const_cast<G1 *>(g.generators.data()), a.data() + i * k,
               k);
  return rows;
}
WitnessBindingProof proveBindingUnchecked(const WitnessBindingInstance &x,
                                          const std::vector<Fr> &w,
                                          const std::vector<Fr> &q) {
  validateInstance(x);
  require(w.size() == x.layout.witness_length &&
              q.size() == x.lookup.query_length,
          "prover shape");
  WitnessBindingProof p;
  p.statement_digest = bindingInstanceDigest(x);
  p.lookup = route_b::lasso::prove(x.lookup, q);
  auto t = start(x, p.lookup);
  auto r = challengePoint(t, log2Exact(paddedSize(q.size())), "binding.row");
  auto qp = q;
  qp.resize(paddedSize(q.size()), x.lookup.table[0]);
  p.query = openMle(qp, p.lookup.query_commitment, r, t, "binding.query");
  auto a = coefficients(x, r);
  auto k = columns(x.layout);
  auto &g = getGeneratorSet({"zkGPT/main", std::uint32_t(k)});
  auto wp = w;
  wp.resize(capacity(x.layout), Fr(0));
  for (std::size_t i = 0; i < x.c0.size(); ++i) {
    std::vector<Fr> row(wp.begin() + i * k, wp.begin() + (i + 1) * k);
    Opening o;
    for (std::size_t j = 0; j < k; ++j)
      o.value += row[j] * a[i][j];
    o.ipa =
        hyraxInnerProductProve(row, a[i], g.generators, g.u, x.c0[i], o.value,
                               t, "binding.C0.row." + std::to_string(i));
    p.rows.push_back(o);
  }
  return p;
}
WitnessBindingProof proveBinding(const WitnessBindingInstance &x,
                                 const std::vector<Fr> &w,
                                 const std::vector<Fr> &q) {
  require(w.size() == x.layout.witness_length &&
              q.size() == x.lookup.query_length,
          "prover shape");
  auto pi = mapping(x);
  for (std::size_t i = 0; i < pi.size(); ++i)
    require(q[i] == w[pi[i]], "honest prover query mismatch");
  return proveBindingUnchecked(x, w, q);
}
std::unique_ptr<const VerifiedWitnessBindingClaim>
verifyBinding(const WitnessBindingInstance &x, const WitnessBindingProof &p,
              std::string *error) {
  try {
    validateInstance(x);
    require(p.statement_digest == bindingInstanceDigest(x),
            "unapproved binding statement");
    VerifiedLookupClaim lookup;
    std::string e;
    if (!route_b::lasso::verify(x.lookup, p.lookup, &lookup, &e))
      throw std::runtime_error("B1 lookup: " + e);
    require(p.rows.size() == x.c0.size(), "B2 row count");
    auto t = start(x, p.lookup);
    auto r = challengePoint(t, log2Exact(paddedSize(x.lookup.query_length)),
                            "binding.row");
    require(
        verifyMle(p.lookup.query_commitment, r, p.query, t, "binding.query"),
        "B2 query opening");
    auto a = coefficients(x, r);
    auto k = columns(x.layout);
    auto &g = getGeneratorSet({"zkGPT/main", std::uint32_t(k)});
    Fr rhs = paddingValue(x, r);
    for (std::size_t i = 0; i < x.c0.size(); ++i) {
      if (!hyraxInnerProductVerify(a[i], g.generators, g.u, x.c0[i],
                                   p.rows[i].value, p.rows[i].ipa, t,
                                   "binding.C0.row." + std::to_string(i), &e))
        throw std::runtime_error("B2 C0 opening: " + e);
      rhs += p.rows[i].value;
    }
    require(rhs == p.query.value, "B2 mapped equality");
    auto c = std::unique_ptr<VerifiedWitnessBindingClaim>(
        new VerifiedWitnessBindingClaim);
    c->c0_digest = c0Digest(x);
    Writer w;
    w.g1(p.lookup.query_commitment);
    c->query_commitment_digest = hash(w.bytes);
    c->layout_identifier = layoutDigest(x.layout);
    c->instance_digest = p.statement_digest;
    c->query_length = x.lookup.query_length;
    c->row_point = r;
    c->evaluation = p.query.value;
    c->transcript_challenges = t.challengeCounter();
    if (error)
      error->clear();
    return c;
  } catch (const std::exception &e) {
    if (error)
      *error = e.what();
    return nullptr;
  }
}
} // namespace route_b::binding
