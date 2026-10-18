#include "hyrax.hpp"
#include "lasso_serialization.hpp"
#include "lasso_table.hpp"
#include "lasso_verifier.hpp"
#include "prover.hpp"
#include "utils.hpp"
#include "witness_binding.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <type_traits>
using namespace route_b::binding;
static unsigned checks = 0;
static void check(bool ok, const std::string &s) {
  if (!ok)
    throw std::runtime_error("FAIL " + s);
  ++checks;
  std::cout << "PASS " << s << '\n';
}
static void rejects(const std::function<void()> &f, const std::string &s) {
  bool failed = false;
  try {
    f();
  } catch (const std::exception &) {
    failed = true;
  }
  check(failed, s);
}
// Malformed trust-input fixtures deliberately bypass the production encoder's
// validation.
static Bytes unsafeInstance(const WitnessBindingInstance &x) {
  Writer w;
  w.u32(0x32424931);
  auto &d = x.layout;
  for (auto v :
       {d.version, d.field, d.curve, d.backend, d.padding, d.witness_length})
    w.u32(v);
  w.string(d.model_profile);
  w.string(d.circuit_id);
  w.u32(d.regions.size());
  for (auto r : d.regions) {
    w.string(r.name);
    w.u32(r.offset);
    w.u32(r.count);
  }
  w.u32(d.queries.size());
  for (auto q : d.queries) {
    w.string(q.id);
    w.string(q.kind);
    w.u32(q.allow_alias);
    w.u32(q.positions.size());
    for (auto p : q.positions) {
      w.u32(p.region);
      w.u32(p.index);
    }
  }
  w.u32(x.selected_query);
  auto l = encodeInstance(x.lookup);
  w.u32(l.size());
  w.raw(l.data(), l.size());
  w.u32(x.c0.size());
  for (auto c : x.c0)
    w.g1(c);
  return w.bytes;
}
static std::vector<Fr> witness(std::size_t n) {
  std::vector<Fr> w;
  for (std::size_t i = 0; i < n; ++i)
    w.push_back(Fr(1 + i % 3));
  return w;
}
static WitnessBindingInstance fixture(std::size_t n, std::size_t m,
                                      bool alias = false) {
  WitnessBindingInstance x;
  auto &d = x.layout;
  d.witness_length = n;
  d.model_profile = "approved-small-profile-v1";
  d.circuit_id = "fixed-small-circuit-v1";
  std::size_t half = (n + 1) / 2;
  d.regions.push_back({"first", 0, std::uint32_t(half)});
  if (n > 1)
    d.regions.push_back(
        {"second", std::uint32_t(half), std::uint32_t(n - half)});
  QueryLayout q;
  q.id = "range-raw";
  q.kind = "raw-range";
  q.allow_alias = alias;
  for (std::size_t i = 0; i < m; ++i) {
    std::size_t idx = i % n;
    std::uint32_t r = idx < half ? 0 : 1;
    q.positions.push_back({r, std::uint32_t(r ? idx - half : idx)});
  }
  d.queries.push_back(q);
  auto t = q;
  t.id = "exp-input-raw";
  t.kind = "exp-t";
  std::reverse(t.positions.begin(), t.positions.end());
  d.queries.push_back(t);
  auto e = q;
  e.id = "exp-output-raw";
  e.kind = "exp-E";
  d.queries.push_back(e);
  x.lookup =
      makeInstance({Fr(1), Fr(2), Fr(3)}, m, {2}, false, "small-raw-table");
  x.c0 = commitWitness(witness(n));
  return x;
}
static std::vector<Fr> queryFor(const WitnessBindingInstance &x,
                                const std::vector<Fr> &w) {
  std::vector<Fr> q;
  for (auto i : mapping(x))
    q.push_back(w.at(i));
  return q;
}
static void productionC0(std::size_t n) {
  auto w = witness(n);
  prover p;
  p.C.init(0, 1);
  p.val.resize(1);
  p.val[0] = w;
  initLayer(p.C.circuit[0], n, layerType::INPUT);
  auto l = p.C.circuit[0].bit_length;
  auto k = std::size_t{1} << (l - l / 2);
  p.gens.resize(k);
  auto u = gen_gi(p.gens.data(), k);
  p.gens.push_back(u);
  p.commitInput(p.gens, 1);
  auto expected = commitWitness(w);
  check(p.cc.l == int(log2Exact(paddedSize(n))),
        "production C0 log-size " + std::to_string(n));
  for (std::size_t i = 0; i < expected.size(); ++i)
    check(expected[i] == p.cc.comm[i], "production C0 row match " +
                                           std::to_string(n) + "/" +
                                           std::to_string(i));
  for (std::size_t i = n; i < paddedSize(n); ++i)
    check(p.cc.w[i] == Fr(0), "production padding " + std::to_string(i));
}
static std::size_t b1Openings(const LassoProof &p) {
  std::size_t n = 1 + p.padding_openings.size() + p.read_evaluations.size();
  for (auto &m : p.memories)
    for (auto &t : m.products)
      n += t.leaf_openings.size();
  return n;
}
static std::size_t b1IpaRounds(const LassoProof &p) {
  std::size_t n = p.query_evaluation.ipa.rounds.size();
  for (auto &o : p.padding_openings)
    n += o.ipa.rounds.size();
  for (auto &o : p.read_evaluations)
    n += o.ipa.rounds.size();
  for (auto &m : p.memories)
    for (auto &t : m.products)
      for (auto &o : t.leaf_openings)
        n += o.ipa.rounds.size();
  return n;
}
static std::size_t b1ScRounds(const LassoProof &p) {
  std::size_t n = p.lookup_sumcheck.rounds.size();
  for (auto &m : p.memories)
    for (auto &t : m.products)
      for (auto &l : t.layers)
        n += l.sumcheck.rounds.size();
  return n;
}
int main(int argc, char **argv) {
  try {
    mcl::bn::initPairing(mcl::BN254);
    check(argc == 2, "fixture argument");
    static_assert(
        !std::is_default_constructible<VerifiedWitnessBindingClaim>::value,
        "claim constructor private");
    static_assert(
        !std::is_copy_constructible<VerifiedWitnessBindingClaim>::value,
        "claim cannot be forged by copy");
    std::filesystem::path dir(argv[1]);
    std::filesystem::create_directories(dir);
    std::ofstream manifest(dir / "manifest.tsv"), bench(dir / "metrics.csv");
    bench
        << "case,witness_length,query_length,regions,mapping_type,c0_points,"
           "all_commitments,b2_openings,total_openings,proof_bytes,b2_overhead_"
           "bytes,prover_ms,verifier_ms,b2_sumcheck_rounds,total_sumcheck_"
           "rounds,b2_ipa_rounds,total_ipa_rounds,b2_transcript_challenges\n";
    auto emit = [&](const std::string &name, const Bytes &inst,
                    const Bytes &proof, bool accept,
                    const std::string &category) {
      writeFile((dir / (name + ".instance")).string(), inst);
      writeFile((dir / (name + ".b2")).string(), proof);
      manifest << name << '\t' << (accept ? 0 : 1) << '\t' << category << '\n';
    };
    auto good = [&](const std::string &name, WitnessBindingInstance x) {
      auto w = witness(x.layout.witness_length), q = queryFor(x, w);
      auto begin = std::chrono::steady_clock::now();
      auto p = proveBinding(x, w, q);
      auto end = std::chrono::steady_clock::now();
      auto wire = encodeBindingProof(p);
      auto ib = encodeBindingInstance(x);
      auto vb = std::chrono::steady_clock::now();
      std::string error;
      auto parsed = decodeBindingProof(wire);
      auto c = verifyBinding(decodeBindingInstance(ib), parsed, &error);
      auto ve = std::chrono::steady_clock::now();
      check(bool(c), name + ": " + error);
      check(encodeBindingProof(parsed) == wire, name + " proof roundtrip");
      check(encodeBindingInstance(decodeBindingInstance(ib)) == ib,
            name + " C0/instance roundtrip");
      check(c->c0_digest == c0Digest(x) &&
                c->layout_identifier == layoutDigest(x.layout),
            name + " claim identities");
      auto cq = q;
      cq.resize(paddedSize(q.size()), x.lookup.table[0]);
      check(c->evaluation == mle(cq, c->row_point), name + " claim MLE");
      std::size_t ipa = p.query.ipa.rounds.size();
      for (auto &o : p.rows)
        ipa += o.ipa.rounds.size();
      check(c->transcript_challenges == ipa + c->row_point.size(),
            name + " challenge count");
      auto b1 = encodeProof(p.lookup).size();
      auto commitments = x.c0.size() + 4 * x.lookup.segment_bits.size() + 3;
      bench << name << ',' << w.size() << ',' << q.size() << ','
            << x.layout.regions.size() << ','
            << (x.layout.queries[x.selected_query].allow_alias
                    ? "explicit-alias"
                    : "fixed-selection")
            << ',' << x.c0.size() << ',' << commitments << ','
            << 1 + p.rows.size() << ','
            << 1 + p.rows.size() + b1Openings(p.lookup) << ',' << wire.size()
            << ',' << wire.size() - b1 << ','
            << std::chrono::duration<double, std::milli>(end - begin).count()
            << ',' << std::chrono::duration<double, std::milli>(ve - vb).count()
            << ",0," << b1ScRounds(p.lookup) << ',' << ipa << ','
            << ipa + b1IpaRounds(p.lookup) << ',' << c->transcript_challenges
            << '\n';
      emit(name, ib, wire, true, "positive");
      return p;
    };
    for (auto n : {1u, 5u, 8u, 31u, 256u})
      productionC0(n);
    good("query_one", fixture(5, 1));
    good("both_one", fixture(1, 1));
    good("power_two", fixture(8, 4));
    auto x = fixture(13, 5);
    auto p = good("nonpower_query_and_witness", x);
    good("repeated_values_distinct_positions", fixture(16, 8));
    good("approved_alias", fixture(3, 8, true));
    good("one_witness_repeated", fixture(1, 5, true));
    good("multiple_regions", fixture(17, 17));
    good("maximum_small", fixture(256, 256));
    auto xt = x;
    xt.selected_query = 1;
    auto p2 = good("same_C0_exp_t", xt);
    xt.selected_query = 2;
    good("same_C0_exp_E", xt);
    check(p.lookup.query_commitment != p2.lookup.query_commitment,
          "same C0 distinct CQ");
    auto w = witness(13);
    auto q = queryFor(x, w);
    std::string err;
    auto original = encodeBindingProof(p);
    auto trusted = encodeBindingInstance(x);
    auto bad = [&](const std::string &name,
                   const std::function<void(WitnessBindingProof &)> &change) {
      auto b = p;
      change(b);
      check(!verifyBinding(x, b, &err), name);
      emit(name, trusted, encodeBindingProof(b), false, "negative");
    };
    bad("query_value",
        [](auto &b) { b.lookup.query_evaluation.value += Fr(1); });
    bad("binding_evaluation", [](auto &b) { b.query.value += Fr(1); });
    bad("statement_C0_digest", [](auto &b) { b.statement_digest[0] ^= 1; });
    bad("linear_combination", [](auto &b) { b.rows[0].value += Fr(1); });
    bad("row_IPA", [](auto &b) { b.rows[0].ipa.final_witness += Fr(1); });
    bad("query_IPA", [](auto &b) { b.query.ipa.final_witness += Fr(1); });
    bad("lookup_sumcheck",
        [](auto &b) { b.lookup.lookup_sumcheck.rounds[0][0] += Fr(1); });
    bad("transcript_IPA_order", [](auto &b) {
      std::swap(b.query.ipa.rounds[0], b.query.ipa.rounds[1]);
    });
    bad("missing_row", [](auto &b) { b.rows.pop_back(); });
    auto different =
        [&](const std::string &name,
            const std::function<void(WitnessBindingInstance &)> &change) {
          auto a = x;
          change(a);
          emit(name, unsafeInstance(a), original, false,
               "negative-trusted-input");
          rejects(
              [&] {
                auto inst = decodeBindingInstance(unsafeInstance(a));
                require(bool(verifyBinding(inst, p, &err)), "rejected");
              },
              name);
        };
    different("wrong_C0", [&](auto &a) {
      a.c0[0] += getGeneratorSet({"zkGPT/main", 4}).u;
    });
    different("other_model_layout",
              [](auto &a) { a.layout.model_profile = "other-model"; });
    different("changed_mapping", [](auto &a) {
      a.layout.queries[0].positions[0].index = 1;
      a.layout.queries[0].allow_alias = true;
    });
    different("mapping_out_of_bounds",
              [](auto &a) { a.layout.queries[0].positions[0].index = 1000; });
    different("mapping_bad_region",
              [](auto &a) { a.layout.queries[0].positions[0].region = 1000; });
    different("region_overlap",
              [](auto &a) { a.layout.regions[1].offset = 1; });
    different("region_overflow", [](auto &a) {
      a.layout.regions[1].offset = 0xffffffffu;
      a.layout.regions[1].count = 0xffffffffu;
    });
    different("illegal_alias", [](auto &a) {
      a.layout.queries[0].positions[1] = a.layout.queries[0].positions[0];
    });
    different("query_length",
              [](auto &a) { a.layout.queries[0].positions.pop_back(); });
    different("witness_length", [](auto &a) { a.layout.witness_length = 12; });
    different("padding_rule", [](auto &a) { a.layout.padding = 2; });
    different("field", [](auto &a) { a.layout.field = 2; });
    different("curve", [](auto &a) { a.layout.curve = 2; });
    different("backend", [](auto &a) { a.layout.backend = 2; });
    different("duplicate_region_name", [](auto &a) {
      a.layout.regions[1].name = a.layout.regions[0].name;
    });
    different("unknown_version", [](auto &a) { a.layout.version = 2; });
    different("selected_query", [](auto &a) { a.selected_query = 999; });
    auto wirebad = [&](const std::string &name, Bytes wire) {
      rejects(
          [&] {
            auto b = decodeBindingProof(wire);
            require(bool(verifyBinding(x, b, &err)), "rejected");
          },
          name);
      emit(name, trusted, wire, false, "negative-wire");
    };
    auto b = original;
    b.push_back(0);
    wirebad("trailing_bytes", b);
    b = original;
    b.pop_back();
    wirebad("truncated_artifact", b);
    b = original;
    b.erase(b.begin() + 4, b.begin() + 8);
    wirebad("missing_required_field", b);
    for (auto n : {0u, 1u, 8u, 40u, 80u}) {
      b = original;
      b.resize(n);
      wirebad("truncate_" + std::to_string(n), b);
    }
    // Nested B1 length starts at offset 40; B2 query field follows tag 2.
    auto nested = encodeProof(p.lookup).size();
    auto scalar_start = 44 + nested + 4;
    b = original;
    std::fill(b.begin() + scalar_start, b.begin() + scalar_start + 32, 0xff);
    wirebad("noncanonical_field", b);
    b = original;
    b[44 + nested] = 99;
    wirebad("transcript_field_order", b);
    b = original;
    std::fill(b.begin() + scalar_start + 32, b.begin() + scalar_start + 36,
              0xff);
    wirebad("oversized_IPA_count", b);
    b = original;
    std::fill(b.begin() + scalar_start + 36, b.begin() + scalar_start + 68,
              0xff);
    wirebad("invalid_IPA_point", b);
    b = original;
    std::fill(b.begin() + 40, b.begin() + 44, 0xff);
    wirebad("oversized_nested_length", b);
    b = original;
    b.resize(kMaxArtifact + 1, 0);
    rejects([&] { decodeBindingProof(b); }, "oversized artifact memory cap");
    // Deliberately bypass the bounded writer to exercise file-size rejection.
    auto oversized_name = dir / "oversized_file.b2";
    {
      std::ofstream f(oversized_name, std::ios::binary);
      f.write(reinterpret_cast<const char *>(b.data()), b.size());
    }
    writeFile((dir / "oversized_file.instance").string(), trusted);
    manifest << "oversized_file\t1\tnegative-wire\n";
    // A: Query is valid for B1 table but differs from the approved witness
    // slot.
    auto qa = q;
    qa[0] = Fr(2);
    auto a = proveBindingUnchecked(x, w, qa);
    VerifiedLookupClaim lc;
    check(route_b::lasso::verify(x.lookup, a.lookup, &lc, &err),
          "attack A B1 still valid");
    check(!verifyBinding(x, a, &err) && err == "B2 mapped equality",
          "attack A rejected at relation");
    emit("regenerated_A_wrong_query", trusted, encodeBindingProof(a), false,
         "malicious-regenerated");
    // B: adversary chooses another legal-format mapping and regenerates
    // everything.
    auto bx = x;
    std::swap(bx.layout.queries[0].positions[0],
              bx.layout.queries[0].positions[1]);
    auto qb = queryFor(bx, w);
    auto bp = proveBindingUnchecked(bx, w, qb);
    check(bool(verifyBinding(bx, bp, &err)),
          "attack B internally valid alternative statement");
    check(!verifyBinding(x, bp, &err), "attack B unapproved mapping rejected");
    emit("regenerated_B_wrong_mapping", trusted, encodeBindingProof(bp), false,
         "malicious-regenerated");
    bp.statement_digest = bindingInstanceDigest(x);
    emit("regenerated_B_relabelled_hash", trusted, encodeBindingProof(bp),
         false, "malicious-regenerated");
    check(!verifyBinding(x, bp, &err),
          "attack B digest relabel cannot change transcript");
    // C: valid but unapproved model profile; never use its own statement as
    // trust anchor.
    auto cx = x;
    cx.layout.model_profile = "prover-chosen-model";
    auto cp = proveBindingUnchecked(cx, w, q);
    check(bool(verifyBinding(cx, cp, &err)),
          "attack C internally valid alternative model");
    check(!verifyBinding(x, cp, &err), "attack C unapproved model rejected");
    emit("regenerated_C_unapproved_model", trusted, encodeBindingProof(cp),
         false, "malicious-regenerated");
    WitnessRegistry registry;
    registry.addRange(WitnessKind::SOFTMAX, "softmax_0_t", 0, 3, 32, false);
    registry.addRange(WitnessKind::SOFTMAX, "softmax_0_E", 3, 3, 32, false);
    registry.addExpQueries(0, 0, 3, 3);
    auto rd = layoutFromTrustedRegistry(registry, 6, "registry-test-model",
                                        "fixed-registry-test-circuit");
    check(rd.queries.size() == 4, "registry derives range/t/E queries");
    WitnessBindingInstance rx;
    rx.layout = rd;
    rx.selected_query = 2;
    rx.lookup =
        makeInstance({Fr(1), Fr(2), Fr(3)}, 3, {2}, false, "registry-table");
    rx.c0 = commitWitness(witness(6));
    good("trusted_registry_exp_t", rx);
    rx.selected_query = 3;
    good("trusted_registry_exp_E", rx);
    rx.selected_query = 0;
    good("trusted_registry_range", rx);
    registry.addRange(WitnessKind::ROUNDING, "unrelated-signed", 6, 1, 32,
                      true);
    auto mixed = layoutFromTrustedRegistry(registry, 7, "p", "c");
    check(mixed.queries.size() == 4, "mixed registry keeps unsigned t/E only");
    registry.clear();
    registry.addRange(WitnessKind::ROUNDING, "signed", 0, 1, 32, true);
    rejects([&] { layoutFromTrustedRegistry(registry, 1, "p", "c"); },
            "registry signed derived query not silently accepted");
    mcl::bn::initPairing(mcl::BN_SNARK1);
    rejects(
        [&] {
          validateLayout(x.layout);
          decodeBindingInstance(trusted);
        },
        "wrong runtime field");
    std::cout << checks << " checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
