#include "hyrax.hpp"
#include "hyrax_opening.hpp"
#include "model_binding.hpp"
#include "models.hpp"
#include "range_serialization.hpp"
#include <iostream>
#include <memory>
void check(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
}
int main() {
  try {
    initPairing(mcl::BN254);
    LLM n(1, 12, 64, 768, 3072);
    prover p;
    n.create(p, false);
    auto a = prepareModelAnchor(n, p, quantizationImplementationProfile());
    auto a2 = prepareModelAnchor(n, p, quantizationImplementationProfile());
    check(a.model_id == a2.model_id, "nondeterministic model id");
    a = decodeModelAnchor(encodeModelAnchor(a));
    ZkGPTPublicStatement s;
    s.model_shape = a.layout.shape;
    s.circuit_fingerprint = a.circuit_fingerprint;
    s.output_claim.mle_evaluation = Fr(0);
    s.range_statement = a.layout;
    s.range_statement.val0_commitment.assign(
        p.cc.comm, p.cc.comm + (size_t{1} << (p.cc.l / 2)));
    ZkGPTProof proof{};
    proof.transcript_binding = Fr(0);
    proof.gkr_proof.output_evaluation = Fr(0);
    proof.gkr_proof.lasso.input_evaluation = Fr(0);
    proof.gkr_proof.lasso.mapping_evaluation = Fr(0);
    proof.gkr_proof.lasso.input_opening.ipa.final_witness = Fr(0);
    proof.gkr_proof.transcript_binding = Fr(0);
    proof.range_proof.transcript_binding = Fr(0);
    std::vector<Fr> x(32 * 768);
    for (size_t i = 0; i < x.size(); i++)
      x[i] = p.val[0][(i / 768) * 1024 + i % 768];
    auto b = bindModelArtifact(a, s, proof, p.val[0], x);
    auto same = bindModelArtifact(a, s, proof, p.val[0], x);
    check(b.input_id == same.input_id, "nondeterministic input id");
    b = decodeBoundArtifact(encodeBoundArtifact(b));
    std::string e;
    check(verifyModelInputBinding(a, b, x, &e), e.c_str());
    std::cout << "PASS deterministic identities, anchor/artifact canonical "
                 "roundtrip, honest full/mixed row binding\n";
    auto reject = [&](const BoundArtifact &v, const std::vector<Fr> &input,
                      const char *label) {
      check(!verifyModelInputBinding(a, v, input, &e), label);
      std::cout << "REJECT " << label << ": " << e << "\n";
    };
    auto v = b;
    v.model_id[0] ^= 1;
    reject(v, x, "model_id");
    v = b;
    v.profile_id[0] ^= 1;
    reject(v, x, "profile_id");
    auto xx = x;
    xx[0] += Fr(1);
    reject(b, xx, "public embedding");
    v = b;
    v.statement.range_statement.val0_commitment[0] += p.gens[0];
    reject(v, x, "commitment");
    v = b;
    v.equality_openings[0].final_witness += Fr(1);
    reject(v, x, "mixed row IPA");
    // Actual witness attacks: recompute commitments after changing each model
    // class, keep official anchor/model_id, and ask verifier to reject changed
    // witness.
    for (const auto &name :
         {"fc_0.weight", "fc_0.bias", "ln_0.weight", "ln_0.bias"}) {
      const neuralNetwork::ParameterRegion *r = nullptr;
      for (auto &z : a.parameters)
        if (z.name == name)
          r = &z;
      check(r, "missing parameter region");
      auto idx = r->offset;
      p.val[0][idx] += Fr(1);
      p.commitInput(p.gens, 1);
      v = b;
      v.statement.range_statement.val0_commitment.assign(
          p.cc.comm, p.cc.comm + (size_t{1} << (p.cc.l / 2)));
      reject(v, x, name);
      bool threw = false;
      try {
        bindModelArtifact(a, v.statement, proof, p.val[0], x);
      } catch (const std::exception &) {
        threw = true;
      }
      check(threw,
            "alternate witness proof construction unexpectedly succeeds");
      p.val[0][idx] -= Fr(1);
    }
    // Adversarial mixed-row proof: bypass the prover commitment self-check.
    // The actual committed witness uses a wrong LN parameter; the prover
    // pretends its difference vector is zero on every selected coordinate.
    const neuralNetwork::ParameterRegion *mixed = nullptr;
    for (auto &r : a.parameters)
      if (r.name == "ln_1.bias")
        mixed = &r;
    check(mixed, "missing mixed LN parameter");
    p.val[0][mixed->offset] += Fr(1);
    p.commitInput(p.gens, 1);
    v = b;
    v.statement.range_statement.val0_commitment.assign(
        p.cc.comm, p.cc.comm + (size_t{1} << (p.cc.l / 2)));
    v.equality_openings.clear();
    Transcript malicious("zkGPT.model-input-binding.v3");
    malicious.appendString("anchor", circuitFingerprintHex(a.model_id));
    malicious.appendString("input", circuitFingerprintHex(v.input_id));
    auto encoded = serializeZkGPTPublicStatement(v.statement);
    malicious.appendString(
        "statement", std::string((const char *)encoded.data(), encoded.size()));
    malicious.appendString(
        "proof", circuitFingerprintHex(bindingHash(
                     "zkGPT.legacy-proof.v3", serializeZkGPTProof(v.proof))));
    const auto &gens = getGeneratorSet(a.layout.val0_generator_domain);
    size_t cols = gens.generators.size();
    std::map<size_t, std::vector<bool>> selected;
    auto select = [&](size_t off, size_t count) {
      for (size_t i = off; i < off + count; i++) {
        auto &m = selected[i / cols];
        if (m.empty())
          m.resize(cols);
        m[i % cols] = true;
      }
    };
    for (auto &r : a.parameters)
      select(r.offset, r.count);
    select(0, 32 * 1024);
    for (auto &row : selected) {
      if (std::all_of(row.second.begin(), row.second.end(),
                      [](bool x) { return x; }))
        continue;
      std::vector<Fr> fake(cols, Fr(0)), coeff(cols, Fr(0));
      for (size_t c = 0; c < cols; c++) {
        if (row.second[c])
          coeff[c] = malicious.challenge("selected-coordinate");
        else
          fake[c] = p.val[0].at(row.first * cols + c);
      }
      G1 target =
          a.layout.val0_commitment.at(row.first); // test embedding is all zero
      G1 diff =
          v.statement.range_statement.val0_commitment.at(row.first) - target;
      v.equality_openings.push_back(hyraxInnerProductProve(
          fake, coeff, gens.generators, gens.u, diff, Fr(0), malicious,
          "mixed-row/" + std::to_string(row.first), false));
    }
    reject(
        v, x,
        "malicious LN witness with forged IPA and prover self-check bypassed");
    p.val[0][mixed->offset] -= Fr(1);
    auto bad = a;
    std::swap(bad.parameters[0], bad.parameters[1]);
    check(!verifyModelInputBinding(bad, b, x, &e), "tensor order");
    bad = a;
    bad.parameters[0].rows++;
    check(!verifyModelInputBinding(bad, b, x, &e), "tensor shape");
    bad = a;
    bad.profile_id[0] ^= 1;
    check(!verifyModelInputBinding(bad, b, x, &e),
          "quantization constant profile");
    std::cout << "PASS actual alternate-weight/bias/LN witness attacks and "
                 "order/shape/profile negatives\n";
    auto bytes = encodeBoundArtifact(b);
    bytes.push_back(0);
    bool threw = false;
    try {
      decodeBoundArtifact(bytes);
    } catch (...) {
      threw = true;
    }
    check(threw, "trailing decoder");
    bytes.resize(16);
    threw = false;
    try {
      decodeBoundArtifact(bytes);
    } catch (...) {
      threw = true;
    }
    check(threw, "truncated decoder");
    std::cout << "PASS decoder bounds; identity binding tests complete "
                 "(Range/GKR relation tested separately)\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}
