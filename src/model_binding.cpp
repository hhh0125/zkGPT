#include "model_binding.hpp"
#include "hyrax_opening.hpp"
#include "range_serialization.hpp"
#include <cybozu/sha2.hpp>
#include <fstream>
#include <limits>
#include <map>
#include <set>
namespace {
struct Codec {
  std::vector<uint8_t> b;
  size_t pos = 0;
  void put(const void *p, size_t n) {
    const auto *q = (const uint8_t *)p;
    b.insert(b.end(), q, q + n);
  }
  const uint8_t *take(size_t n) {
    if (n > b.size() - pos)
      throw std::runtime_error("truncated binding encoding");
    auto *p = b.data() + pos;
    pos += n;
    return p;
  }
  void u(size_t x) {
    for (int i = 0; i < 8; i++) {
      uint8_t c = x >> (8 * i);
      put(&c, 1);
    }
  }
  size_t u() {
    uint64_t x = 0;
    for (int i = 0; i < 8; i++)
      x |= uint64_t(*take(1)) << (8 * i);
    if (x > (1ULL << 30))
      throw std::runtime_error("binding count exceeds limit");
    return x;
  }
  void blob(const std::vector<uint8_t> &v) {
    u(v.size());
    put(v.data(), v.size());
  }
  std::vector<uint8_t> blob() {
    auto n = u();
    auto *p = take(n);
    return {p, p + n};
  }
  void str(const std::string &s) { blob({s.begin(), s.end()}); }
  std::string str() {
    auto v = blob();
    return {v.begin(), v.end()};
  }
  void dig(const BindingDigest &d) { put(d.data(), 32); }
  BindingDigest dig() {
    BindingDigest d;
    auto *p = take(32);
    std::copy(p, p + 32, d.begin());
    return d;
  }
  template <class T> void field(const T &v) {
    uint8_t d[128];
    auto n = v.serialize(d, sizeof d);
    if (!n)
      throw std::runtime_error("serialize group");
    u(n);
    put(d, n);
  }
  template <class T> T field() {
    auto n = u();
    if (n > 128)
      throw std::runtime_error("field size");
    auto *p = take(n);
    T v;
    if (v.deserialize(p, n) != n)
      throw std::runtime_error("field decode");
    if constexpr (std::is_same<T, G1>::value) {
      if (!v.isValid())
        throw std::runtime_error("invalid binding group point");
    }
    uint8_t d[128];
    if (v.serialize(d, 128) != n || !std::equal(p, p + n, d))
      throw std::runtime_error("noncanonical field");
    return v;
  }
  void end() {
    if (pos != b.size())
      throw std::runtime_error("trailing binding bytes");
  }
};
using Masks = std::map<size_t, std::vector<bool>>;
Masks masks(const ModelAnchor &a, bool input) {
  if (a.layout.val0_log_size < 10 || a.layout.val0_log_size > 30)
    throw std::runtime_error("binding log size");
  size_t cols =
             size_t{1} << (a.layout.val0_log_size - a.layout.val0_log_size / 2),
         cap = size_t{1} << a.layout.val0_log_size;
  Masks m;
  auto add = [&](size_t start, size_t count) {
    if (start > cap || count > cap - start)
      throw std::runtime_error("binding region bounds");
    for (size_t i = start; i < start + count; i++) {
      auto &r = m[i / cols];
      if (r.empty())
        r.resize(cols);
      if (r[i % cols])
        throw std::runtime_error("overlapping binding region");
      r[i % cols] = true;
    }
  };
  for (const auto &r : a.parameters)
    add(r.offset, r.count);
  if (input)
    add(0, 32 * 1024);
  return m;
}
void regions(Codec &c, const ModelAnchor &a) {
  c.u(a.parameters.size());
  for (auto &r : a.parameters) {
    c.str(r.name);
    c.u(r.offset);
    c.u(r.count);
    c.u(r.rows);
    c.u(r.cols);
  }
}
BindingDigest modelId(const ModelAnchor &a) {
  Codec c;
  c.str(a.checkpoint_identifier);
  c.dig(a.profile_id);
  c.dig(a.parameter_digest);
  c.dig(a.circuit_fingerprint);
  regions(c, a);
  c.blob(serializeRangePublicStatement(a.layout));
  return bindingHash("zkGPT.model.manifest.v3", c.b);
}
BindingDigest inputId(const std::vector<Fr> &x) {
  if (x.size() != 32 * 768)
    throw std::runtime_error("embedding shape");
  Codec c;
  c.u(32);
  c.u(768);
  for (auto &v : x)
    c.field(v);
  return bindingHash("zkGPT.public.quantized.embedding.v3", c.b);
}
std::vector<G1> targets(const ModelAnchor &a, const std::vector<Fr> &x) {
  const auto &g = getGeneratorSet(a.layout.val0_generator_domain).generators;
  auto out = a.layout.val0_commitment;
  std::map<size_t, std::vector<Fr>> rows;
  for (size_t i = 0; i < x.size(); i++) {
    size_t idx = (i / 768) * 1024 + i % 768;
    auto &r = rows[idx / g.size()];
    if (r.empty())
      r.resize(g.size(), Fr(0));
    r[idx % g.size()] = x[i];
  }
  for (auto &entry : rows) {
    G1 c;
    G1::mulVec(c, const_cast<G1 *>(g.data()), entry.second.data(), g.size());
    out.at(entry.first) += c;
  }
  return out;
}
Transcript transcript(const ModelAnchor &a, const BoundArtifact &b) {
  Transcript t("zkGPT.model-input-binding.v3");
  t.appendString("anchor", circuitFingerprintHex(a.model_id));
  t.appendString("input", circuitFingerprintHex(b.input_id));
  auto s = serializeZkGPTPublicStatement(b.statement);
  t.appendString("statement", std::string((const char *)s.data(), s.size()));
  auto p = serializeZkGPTProof(b.proof);
  auto h = bindingHash("zkGPT.legacy-proof.v3", p);
  t.appendString("proof", circuitFingerprintHex(h));
  return t;
}
void require(bool ok, const char *m) {
  if (!ok)
    throw std::runtime_error(m);
}
} // namespace
BindingDigest bindingHash(const std::string &domain,
                          const std::vector<uint8_t> &v) {
  Codec c;
  c.str(domain);
  c.blob(v);
  cybozu::Sha256 h;
  BindingDigest d;
  if (h.digest(d.data(), 32, c.b.data(), c.b.size()) != 32)
    throw std::runtime_error("hash");
  return d;
}
BindingDigest quantizationImplementationProfile() {
  Codec c;
  c.str("fixed32x768;int32-little-endian;Fr-BN254;input-search(.01);weight-2^-"
        "10;LN-default-1,-10,1,-10;source-semantics-v1");
  for (auto path : {"src/neuralNetwork.cpp", "src/models.cpp"}) {
    std::ifstream f(path, std::ios::binary);
    if (!f)
      throw std::runtime_error("profile source missing");
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), {});
    c.str(path);
    // Normalize CRLF so the profile is independent of checkout line endings.
    std::vector<uint8_t> canonical;
    for (size_t i = 0; i < b.size(); ++i) {
      if (b[i] == '\r' && i + 1 < b.size() && b[i + 1] == '\n')
        continue;
      canonical.push_back(b[i]);
    }
    c.dig(bindingHash("zkGPT.quantization.source.v3",
                      std::string(path) == "src/neuralNetwork.cpp" ? canonical
                                                                   : b));
  }
  c.dig(canonicalExpTableId());
  c.u(kExpTableSize);
  c.u(kExpTailStart);
  return bindingHash("zkGPT.quantization.profile.v4", c.b);
}
ModelAnchor prepareModelAnchor(const neuralNetwork &n, const prover &p,
                               const BindingDigest &profile) {
  ModelAnchor a;
  a.profile_id = profile;
  a.parameters = n.parameterRegions();
  a.circuit_fingerprint = fingerprintCircuit(p.C);
  a.layout.shape = n.getWitnessRegistry().shape();
  a.layout.val0_log_size = p.cc.l;
  size_t cols = size_t{1} << (p.cc.l - p.cc.l / 2),
         rows = size_t{1} << (p.cc.l / 2);
  a.layout.val0_generator_domain = {"zkGPT/main", uint32_t(cols)};
  a.layout.val0_commitment.resize(rows);
  for (auto &c : a.layout.val0_commitment)
    c.clear();
  cybozu::Sha256 h;
  const std::string domain = "zkGPT.loaded.padded.parameters.v3";
  h.update(domain.data(), domain.size());
  for (const auto &r : a.parameters) {
    Codec metadata;
    metadata.str(r.name);
    metadata.u(r.rows);
    metadata.u(r.cols);
    metadata.u(r.count);
    h.update(metadata.b.data(), metadata.b.size());
    Codec chunk;
    for (size_t i = 0; i < r.count; i++) {
      chunk.field(p.val[0].at(r.offset + i));
      if (chunk.b.size() >= 65536) {
        h.update(chunk.b.data(), chunk.b.size());
        chunk.b.clear();
      }
    }
    if (!chunk.b.empty())
      h.update(chunk.b.data(), chunk.b.size());
  }
  if (h.digest(a.parameter_digest.data(), 32, nullptr, 0) != 32)
    throw std::runtime_error("parameter hash");
  auto m = masks(a, false);
  std::vector<Fr> v(cols);
  for (auto &entry : m) {
    // A complete parameter row already has exactly the trusted publisher's
    // commitment. Reuse it; only mixed rows need a masked MSM.
    if (std::all_of(entry.second.begin(), entry.second.end(),
                    [](bool z) { return z; })) {
      a.layout.val0_commitment[entry.first] = p.cc.comm[entry.first];
      continue;
    }
    for (size_t c = 0; c < cols; c++)
      v[c] = entry.second[c] ? p.val[0].at(entry.first * cols + c) : Fr(0);
    G1::mulVec(a.layout.val0_commitment[entry.first], p.cc.g, v.data(), cols);
  }
  a.model_id = modelId(a);
  return a;
}
BoundArtifact bindModelArtifact(const ModelAnchor &a,
                                const ZkGPTPublicStatement &s,
                                const ZkGPTProof &p, const std::vector<Fr> &w,
                                const std::vector<Fr> &x) {
  BoundArtifact b;
  b.model_id = a.model_id;
  b.profile_id = a.profile_id;
  b.input_id = inputId(x);
  b.statement = s;
  b.proof = p;
  auto t = transcript(a, b);
  auto target = targets(a, x);
  auto m = masks(a, true);
  const auto &g = getGeneratorSet(a.layout.val0_generator_domain);
  size_t cols = g.generators.size();
  for (auto &entry : m) {
    auto row = entry.first;
    G1 diff = s.range_statement.val0_commitment.at(row) - target.at(row);
    if (std::all_of(entry.second.begin(), entry.second.end(),
                    [](bool z) { return z; })) {
      require(diff.isZero(),
              "full parameter/input row differs from trusted anchor");
      continue;
    }
    std::vector<Fr> v(cols), coef(cols, Fr(0));
    for (size_t c = 0; c < cols; c++) {
      v[c] = entry.second[c] ? Fr(0) : w.at(row * cols + c);
      if (entry.second[c])
        coef[c] = t.challenge("selected-coordinate");
    }
    b.equality_openings.push_back(
        hyraxInnerProductProve(v, coef, g.generators, g.u, diff, Fr(0), t,
                               "mixed-row/" + std::to_string(row)));
  }
  return b;
}
bool verifyModelInputBinding(const ModelAnchor &a, const BoundArtifact &b,
                             const std::vector<Fr> &x, std::string *error) {
  try {
    require(a.model_id == modelId(a), "anchor identity invalid");
    require(b.model_id == a.model_id, "unexpected model identity");
    require(b.profile_id == a.profile_id, "unexpected quantization profile");
    require(a.profile_id == quantizationImplementationProfile(),
            "unsupported implementation profile");
    require(b.input_id == inputId(x), "unexpected public input");
    require(b.statement.circuit_fingerprint == a.circuit_fingerprint,
            "unexpected circuit semantics");
    require(b.statement.range_statement.val0_log_size == a.layout.val0_log_size,
            "witness dimensions differ");
    require(serializeRangePublicStatement(b.statement.range_statement).size() >
                0,
            "statement invalid");
    require(b.statement.range_statement.val0_generator_domain.domain ==
                    a.layout.val0_generator_domain.domain &&
                b.statement.range_statement.val0_generator_domain.count ==
                    a.layout.val0_generator_domain.count,
            "generator mismatch");
    auto t = transcript(a, b);
    auto target = targets(a, x);
    auto m = masks(a, true);
    const auto &g = getGeneratorSet(a.layout.val0_generator_domain);
    size_t k = 0;
    for (auto &entry : m) {
      size_t row = entry.first;
      G1 diff =
          b.statement.range_statement.val0_commitment.at(row) - target.at(row);
      if (std::all_of(entry.second.begin(), entry.second.end(),
                      [](bool z) { return z; })) {
        require(diff.isZero(),
                "trusted parameter/input row commitment mismatch");
        continue;
      }
      std::vector<Fr> coef(g.generators.size(), Fr(0));
      for (size_t c = 0; c < coef.size(); c++)
        if (entry.second[c])
          coef[c] = t.challenge("selected-coordinate");
      std::string e;
      require(k < b.equality_openings.size(), "missing equality proof");
      if (!hyraxInnerProductVerify(coef, g.generators, g.u, diff, Fr(0),
                                   b.equality_openings[k++], t,
                                   "mixed-row/" + std::to_string(row), &e))
        throw std::runtime_error("parameter/input equality failed: " + e);
    }
    require(k == b.equality_openings.size(), "extra equality proof");

    return true;
  } catch (const std::exception &e) {
    if (error)
      *error = e.what();
    return false;
  }
}
namespace {
Fr expContext(const ModelAnchor &a,const BoundArtifact &b) {
  auto t=transcript(a,b); t.appendU64("strict-protocol-version",4);
  const auto id=canonicalExpTableId();t.appendBytes("trusted-exp-table",id.data(),id.size());
  return t.challenge("exp-lookup-context.v4");
}
}
BoundArtifact bindStrictArtifactV4(const ModelAnchor &a,const ZkGPTPublicStatement &s,
    const ZkGPTProof &p,const std::vector<Fr> &w,const std::vector<Fr> &x,int threads) {
  auto b=bindModelArtifact(a,s,p,w,x);b.strict_protocol_version=4;
  b.exp_lookup=proveExpLookup(s.range_statement,w,expContext(a,b),threads);
  return b;
}
bool verifyLegacyBoundArtifactV3(const ModelAnchor &a,const BoundArtifact &b,
    const std::vector<Fr> &x,std::string *error) {
  if(b.strict_protocol_version!=3){if(error)*error="legacy v3 version required";return false;}
  return verifyModelInputBinding(a,b,x,error) && verifyZkGPT(b.statement,b.proof,error);
}
bool verifyBoundArtifact(const ModelAnchor &a,const BoundArtifact &b,
    const std::vector<Fr> &x,std::string *error) {
  if(b.strict_protocol_version!=4){if(error)*error="strict v4 required: legacy proof has no exp lookup";return false;}
  // Identity and table are checked before expensive original verification.
  if(!verifyModelInputBinding(a,b,x,error))return false;
  if(!verifyExpLookup(b.statement.range_statement,b.exp_lookup,expContext(a,b),error))return false;
  return verifyZkGPT(b.statement,b.proof,error);
}

std::vector<uint8_t> encodeModelAnchor(const ModelAnchor &a) {
  Codec c;
  c.str("ZKGPT-TRUSTED-MODEL-V3");
  c.str(a.checkpoint_identifier);
  c.dig(a.model_id);
  c.dig(a.profile_id);
  c.dig(a.parameter_digest);
  c.dig(a.circuit_fingerprint);
  c.blob(serializeRangePublicStatement(a.layout));
  regions(c, a);
  return c.b;
}
ModelAnchor decodeModelAnchor(const std::vector<uint8_t> &v) {
  Codec c;
  c.b = v;
  require(c.str() == "ZKGPT-TRUSTED-MODEL-V3", "anchor version");
  ModelAnchor a;
  a.checkpoint_identifier = c.str();
  a.model_id = c.dig();
  a.profile_id = c.dig();
  a.parameter_digest = c.dig();
  a.circuit_fingerprint = c.dig();
  auto r = c.blob();
  std::string e;
  require(deserializeRangePublicStatement(r, a.layout, &e),
          "anchor layout decode");
  auto n = c.u();
  require(n <= 1000, "parameter count bound");
  for (size_t i = 0; i < n; i++) {
    neuralNetwork::ParameterRegion p;
    p.name = c.str();
    p.offset = c.u();
    p.count = c.u();
    p.rows = c.u();
    p.cols = c.u();
    a.parameters.push_back(p);
  }
  c.end();
  masks(a, false);
  require(a.model_id == modelId(a), "anchor digest");
  return a;
}
std::vector<uint8_t> encodeBoundArtifact(const BoundArtifact &a) {
  Codec c;
  require(a.strict_protocol_version==3 || a.strict_protocol_version==4,"strict encode version");
  c.str(a.strict_protocol_version==4?"ZKGPT-STRICT-ARTIFACT-V4":"ZKGPT-STRICT-ARTIFACT-V3");
  c.dig(a.model_id);
  c.dig(a.profile_id);
  c.dig(a.input_id);
  c.blob(serializeZkGPTPublicStatement(a.statement));
  c.blob(serializeZkGPTProof(a.proof));
  c.u(a.equality_openings.size());
  for (auto &p : a.equality_openings) {
    c.u(p.rounds.size());
    for (auto &r : p.rounds) {
      c.field(r.left);
      c.field(r.right);
    }
    c.field(p.final_witness);
  }
  if(a.strict_protocol_version==4) {
    c.dig(a.exp_lookup.exp_table_id);c.u(a.exp_lookup.query_count);
    c.blob(serializeRangeProof(a.exp_lookup.payload));
  }
  return c.b;
}
BoundArtifact decodeBoundArtifact(const std::vector<uint8_t> &v) {
  Codec c;
  c.b = v;
  auto magic=c.str();
  require(magic=="ZKGPT-STRICT-ARTIFACT-V3" || magic=="ZKGPT-STRICT-ARTIFACT-V4","strict artifact version");
  BoundArtifact a;a.strict_protocol_version=magic=="ZKGPT-STRICT-ARTIFACT-V4"?4:3;
  a.model_id = c.dig();
  a.profile_id = c.dig();
  a.input_id = c.dig();
  std::string e;
  require(deserializeZkGPTPublicStatement(c.blob(), a.statement, &e),
          "strict statement decode");
  require(deserializeZkGPTProof(c.blob(), a.proof, &e), "strict proof decode");
  auto n = c.u();
  require(n <= 1000, "opening count bound");
  for (size_t i = 0; i < n; i++) {
    HyraxIpaProof p;
    auto k = c.u();
    require(k <= 20, "IPA round bound");
    for (size_t j = 0; j < k; j++)
      p.rounds.push_back({c.field<G1>(), c.field<G1>()});
    p.final_witness = c.field<Fr>();
    a.equality_openings.push_back(p);
  }
  if(a.strict_protocol_version==4) {
    a.exp_lookup.exp_table_id=c.dig();a.exp_lookup.query_count=c.u();
    require(deserializeRangeProof(c.blob(),a.exp_lookup.payload,&e),"exp lookup decode");
  }
  c.end();
  return a;
}
std::vector<Fr> readPublicEmbedding(const std::string &path) {
  std::ifstream f(path);
  require(bool(f), "embedding file missing");
  std::vector<Fr> x(32 * 768);
  long long v;
  for (auto &a : x) {
    require(bool(f >> v), "embedding integer count");
    a = Fr(v);
  }
  std::string extra;
  require(!(f >> extra), "extra embedding values");
  return x;
}
