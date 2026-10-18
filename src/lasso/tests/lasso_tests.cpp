#include "adversarial_prover.hpp"
#include "lasso_commitment.hpp"
#include "lasso_memory.hpp"
#include "lasso_prover.hpp"
#include "lasso_serialization.hpp"
#include "lasso_sumcheck.hpp"
#include "lasso_verifier.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
using namespace route_b::lasso;
static unsigned passed = 0;
static void check(bool ok, const std::string &name) {
    if (!ok)
        throw std::runtime_error("FAIL " + name);
    ++passed;
    std::cout << "PASS " << name << '\n';
}
static void throws(const std::function<void()> &f, const std::string &name) {
    bool caught = false;
    try {
        f();
    } catch (const std::exception &) {
        caught = true;
    }
    check(caught, name);
}
static void metrics(std::ostream &out, const std::string &name, const LassoLookupInstance &x,
                    const LassoProof &p, std::size_t bytes, double prover, double verifier) {
    std::size_t rounds = p.lookup_sumcheck.rounds.size(),
                openings = 1 + p.padding_openings.size() + p.read_evaluations.size(),
                evaluations = openings;
    for (auto &m : p.memories)
        for (auto &g : m.products) {
            openings += g.leaf_openings.size();
            evaluations += g.leaf_openings.size();
            for (auto &l : g.layers) {
                rounds += l.sumcheck.rounds.size();
                evaluations += 2;
            }
        }
    auto c = x.segment_bits.size();
    auto commitments = 1 + c + 2 * (c + 1) + c; // validity E is a constant, not a commitment
    out << name << ',' << x.query_length << ',' << x.table.size() << ','
        << log2Exact(paddedSize(x.table.size())) << ',' << c << ',' << prover << ',' << verifier
        << ',' << bytes << ',' << commitments << ',' << rounds << ',' << evaluations << ','
        << openings << '\n';
}
int main(int argc, char **argv) {
    try {
        mcl::bn::initPairing(mcl::BN_SNARK1);
        throws([] { makeInstance({Fr(1)}, 1, {0}, false, "wrong-runtime-field"); },
               "wrong runtime field");
        mcl::bn::initPairing(mcl::BN254);
        require(argc == 2, "fixture directory argument");
        std::filesystem::path dir = argv[1];
        std::filesystem::create_directories(dir);
        std::ofstream manifest(dir / "manifest.tsv"), bench(dir / "metrics.csv");
        bench << "case,m,N,address_bits,segments,prover_ms,verifier_ms,proof_bytes,commitments,"
                 "sumcheck_rounds,evaluations,openings\n";
        auto emit = [&](const std::string &name, const LassoLookupInstance &x, const Bytes &b,
                        bool accept, const std::string &category) {
            writeFile((dir / (name + ".instance")).string(), encodeInstance(x));
            writeFile((dir / (name + ".lasso")).string(), b);
            manifest << name << '\t' << (accept ? 0 : 1) << '\t' << category << '\n';
        };
        auto positive = [&](const std::string &name, const std::vector<Fr> &table,
                            const std::vector<Fr> &q, const std::vector<unsigned> &bits, bool add) {
            auto x = makeInstance(table, q.size(), bits, add, name);
            auto start = std::chrono::steady_clock::now();
            auto p = prove(x, q);
            auto middle = std::chrono::steady_clock::now();
            auto b = encodeProof(p);
            auto d = decodeProof(b);
            VerifiedLookupClaim claim;
            std::string error;
            bool accepted = verify(x, d, &claim, &error);
            auto finish = std::chrono::steady_clock::now();
            check(accepted, name + " " + error);
            check(encodeProof(d) == b, name + " canonical roundtrip");
            check(claim.evaluation == p.query_evaluation.value &&
                      claim.query_commitment == p.query_commitment,
                  name + " authenticated claim");
            emit(name, x, b, true, "positive");
            metrics(bench, name, x, p, b.size(),
                    std::chrono::duration<double, std::milli>(middle - start).count(),
                    std::chrono::duration<double, std::milli>(finish - middle).count());
            return std::make_pair(x, p);
        };
        positive("query_one", {Fr(1), Fr(9)}, {Fr(9)}, {1}, false);
        positive("table_one", {Fr(7)}, {Fr(7), Fr(7), Fr(7)}, {0}, false);
        positive("both_one", {Fr(7)}, {Fr(7)}, {0}, false);
        positive("power_two", {Fr(2), Fr(3), Fr(5), Fr(8)}, {Fr(8), Fr(2), Fr(5), Fr(3)}, {2},
                 false);
        auto base = positive("nonpower_repeated", {Fr(3), Fr(5), Fr(8)},
                             {Fr(8), Fr(3), Fr(5), Fr(8), Fr(3)}, {2}, false);
        positive("duplicate_table", {Fr(4), Fr(4), Fr(9)},
                 {Fr(4), Fr(9), Fr(4), Fr(4), Fr(9), Fr(4)}, {2}, false);
        positive("minimum_bit", {Fr(0), Fr(1)}, {Fr(1), Fr(0), Fr(1)}, {1}, false);
        std::vector<Fr> seven;
        for (int i = 0; i < 7; ++i)
            seven.push_back(Fr(10 + 3 * i));
        positive("sos_1_2", seven, {seven[6], seven[2], seven[0], seven[5], seven[1]}, {1, 2},
                 true);
        positive("sos_2_1", seven, {seven[6], seven[3], seven[0]}, {2, 1}, true);
        std::vector<Fr> sixteen;
        for (int i = 0; i < 16; ++i)
            sixteen.push_back(Fr(2 * i));
        positive("sos_1_1_2", sixteen, {Fr(30), Fr(0), Fr(12), Fr(24)}, {1, 1, 2}, true);
        std::vector<Fr> longq;
        for (int i = 0; i < 256; ++i)
            longq.push_back(Fr(i % 2));
        positive("max_small_bound", {Fr(0), Fr(1)}, longq, {1}, false);
        std::vector<Fr> maximum_table;
        for (unsigned i = 0; i < 256; ++i)
            maximum_table.push_back(Fr(i));
        positive("max_table_eight_segments", maximum_table, {Fr(255), Fr(0), Fr(128)},
                 {1, 1, 1, 1, 1, 1, 1, 1}, true);
        // Independent reference checks establish bit ordering and degree limits.
        for (unsigned bits = 0; bits < 6; ++bits) {
            std::vector<Fr> p(bits), v(std::size_t{1} << bits);
            for (unsigned j = 0; j < bits; ++j)
                p[j] = Fr(j + 3);
            for (std::size_t j = 0; j < v.size(); ++j)
                v[j] = Fr(j * j + 2);
            auto w = eqWeights(p);
            Fr dot = 0;
            for (std::size_t j = 0; j < v.size(); ++j)
                dot += w[j] * v[j];
            check(mle(v, p) == dot, "MLE bit order " + std::to_string(bits));
        }
        auto &x = base.first;
        auto &p = base.second;
        auto valid = encodeProof(p);
        auto reject = [&](const std::string &name, const Bytes &b) {
            if (name.rfind("regenerated_", 0) == 0)
                check(encodeProof(decodeProof(b)) == b, name + " canonical regenerated artifact");
            bool ok = false;
            std::string error;
            try {
                ok = verify(x, decodeProof(b), nullptr, &error);
            } catch (const std::exception &) {
            }
            check(!ok, "reject " + name);
            emit(name, x, b, false, "negative");
        };
        auto mutate = [&](const std::string &name, const std::function<void(LassoProof &)> &fn) {
            auto changed = p;
            fn(changed);
            reject(name, encodeProof(changed));
        };
        auto delta = lassoGenerators(8).generators[0];
        mutate("query_value", [](auto &p) { p.query_evaluation.value += Fr(1); });
        mutate("selector_commitment", [&](auto &p) { p.dim_commitments[0] += delta; });
        mutate("dim_evaluation",
               [](auto &p) { p.memories[0].products[0].leaf_openings[0].value += Fr(1); });
        mutate("sumcheck_round", [](auto &p) { p.lookup_sumcheck.rounds[0][0] += Fr(1); });
        mutate("final_evaluation", [](auto &p) { p.read_evaluations[0].value += Fr(1); });
        mutate("ipa_opening", [](auto &p) { p.query_evaluation.ipa.final_witness += Fr(1); });
        mutate("table_digest", [](auto &p) { p.instance_digest[0] ^= 1; });
        mutate("read_counter_commitment", [&](auto &p) { p.memory_commitments[0].read += delta; });
        mutate("final_counter_commitment",
               [&](auto &p) { p.memory_commitments[0].final += delta; });
        mutate("read_value_commitment", [&](auto &p) { p.memory_commitments[0].value += delta; });
        mutate("memory_product_root", [](auto &p) { p.memories[0].roots[0] += Fr(1); });
        mutate("grand_product_round",
               [](auto &p) { p.memories[0].products[0].layers[1].sumcheck.rounds[0][0] += Fr(1); });
        mutate("grand_product_endpoint",
               [](auto &p) { p.memories[0].products[0].layers[0].left += Fr(1); });
        mutate("final_counter_evaluation",
               [](auto &p) { p.memories[0].products[2].leaf_openings[0].value += Fr(1); });
        mutate("padding_evaluation", [](auto &p) { p.padding_openings[0].value += Fr(1); });
        mutate("challenge_message", [&](auto &p) { p.query_commitment += delta; });
        mutate("missing_opening",
               [](auto &p) { p.memories[0].products[0].leaf_openings.pop_back(); });
        mutate("missing_memory", [](auto &p) { p.memories.pop_back(); });
        mutate("extra_dim", [&](auto &p) { p.dim_commitments.push_back(delta); });
        mutate("invalid_constant_E", [&](auto &p) { p.memory_commitments.back().value = delta; });
        auto other = makeInstance({Fr(3), Fr(6), Fr(8)}, x.query_length, {2}, false, x.profile);
        emit("replace_table", other, valid, false, "negative");
        check(!verify(other, p), "replace approved table");
        other = x;
        other.query_length = 6;
        emit("query_length", other, valid, false, "negative");
        check(!verify(other, p), "change query length");
        other = makeInstance({Fr(3), Fr(5), Fr(8), Fr(9)}, x.query_length, {2}, false, x.profile);
        emit("table_length", other, valid, false, "negative");
        check(!verify(other, p), "change table length");
        auto b = valid;
        b.push_back(0);
        reject("trailing_bytes", b);
        b = valid;
        b.pop_back();
        reject("truncated_artifact", b);
        b = valid;
        b.erase(b.end() - 4, b.end());
        reject("missing_required_field", b);
        b = valid;
        b[12] = 2;
        reject("reordered_fields", b);
        b = valid;
        b[8] = 2;
        reject("unknown_version", b);
        b = valid;
        for (unsigned i = 80; i < 84; ++i)
            b[i] = 255;
        reject("oversize_dim_count", b);
        // Query evaluation is the first Fr after tag 3. Fr >= modulus must not reduce.
        const std::size_t fr_offset = 8 + 4 + 4 + 32 + 32 + 4 + 32 * p.dim_commitments.size() + 4 +
                                      4 + 96 * p.memory_commitments.size() + 4;
        b = valid;
        std::fill(b.begin() + fr_offset, b.begin() + fr_offset + 32, 255);
        reject("noncanonical_field", b);
        b = valid;
        std::fill(b.begin() + 48, b.begin() + 80, 255);
        reject("invalid_curve_encoding", b);
        auto badInstance = [&](const std::string &name,
                               const std::function<void(Bytes &)> &change) {
            auto ib = encodeInstance(x);
            change(ib);
            throws([&] { decodeInstance(ib); }, "instance reject " + name);
            writeFile((dir / (name + ".instance")).string(), ib);
            writeFile((dir / (name + ".lasso")).string(), valid);
            manifest << name << "\t1\tnegative\n";
        };
        // Instance fixed header: magic + eight u32 + profile + segment list + digest.
        auto digestOffset = 8 + 8 * 4 + 4 + x.profile.size() + 4 + 4 * x.segment_bits.size();
        badInstance("table_digest_bytes", [&](auto &b) { b[digestOffset] ^= 1; });
        badInstance("table_content_stale_digest", [](auto &b) { b.back() ^= 1; });
        badInstance("noncanonical_instance_integer",
                    [](auto &b) { b[36] = 2; }); // bool must be 0/1
        badInstance("unsupported_field", [](auto &b) { b[12] = 2; });
        badInstance("unsupported_curve", [](auto &b) { b[16] = 2; });
        badInstance("altered_padding_rule", [](auto &b) { b[24] = 2; });
        badInstance("zero_table_length", [](auto &b) {
            for (unsigned i = 32; i < 36; ++i)
                b[i] = 0;
        });
        badInstance("instance_trailing_bytes", [](auto &b) { b.push_back(0); });
        badInstance("instance_truncated", [](auto &b) { b.pop_back(); });
        for (std::size_t cut : std::vector<std::size_t>{0, 1, 8, 12, 47, 83, valid.size() / 2}) {
            b = valid;
            b.resize(cut);
            reject("truncate_" + std::to_string(cut), b);
        }
        // Fresh malicious prover: invalid query AND forged E, with every C/message regenerated.
        auto w = makeWitness(x, {Fr(8), Fr(3), Fr(5), Fr(8), Fr(3)});
        w.query[0] = Fr(99);
        w.memories[0].value[0] = Fr(99);
        auto attack = adversarial::proveWitness(x, w);
        check(attack.query_commitment != p.query_commitment, "malicious commitment regenerated");
        reject("regenerated_invalid_query", encodeProof(attack));
        // Fresh out-of-range address within the padded domain, all counters regenerated.
        w = makeWitness(x, {Fr(3), Fr(3), Fr(5), Fr(8), Fr(3)});
        w.selector.dim[0][0] = Fr(3);
        for (auto &m : w.memories) {
            m.address[0] = Fr(3);
            m.final.assign(m.table.size(), Fr(0));
            for (std::size_t i = 0; i < m.address.size(); ++i) {
                auto a = static_cast<std::size_t>(std::stoul(m.address[i].getStr()));
                m.read[i] = m.final[a];
                m.final[a] += Fr(1);
            }
        }
        // The validity read is fixed to one, even for this dishonest address.
        reject("regenerated_padding_address", encodeProof(adversarial::proveWitness(x, w)));
        auto sx = makeInstance(seven, 3, {1, 2}, true, "segmented-invalid-address");
        auto sw = makeWitness(sx, {Fr(10), Fr(13), Fr(16)});
        sw.query[0] = Fr(31);
        sw.selector.dim[0][0] = Fr(1);
        sw.selector.dim[1][0] = Fr(3);
        for (std::size_t h = 0; h < sw.memories.size(); ++h) {
            auto &m = sw.memories[h];
            std::size_t a = h == 0 ? 1 : (h == 1 ? 3 : 7);
            m.address[0] = Fr(a);
            if (h < 2)
                m.value[0] = m.table[a];
            m.final.assign(m.table.size(), Fr(0));
            for (std::size_t i = 0; i < m.address.size(); ++i) {
                a = std::stoul(m.address[i].getStr());
                m.read[i] = m.final[a];
                m.final[a] += Fr(1);
            }
        }
        auto sp = adversarial::proveWitness(sx, sw);
        check(encodeProof(decodeProof(encodeProof(sp))) == encodeProof(sp),
              "segmented regenerated artifact canonical");
        check(!verify(sx, sp), "segmented invalid address regenerated");
        emit("regenerated_segmented_invalid_address", sx, encodeProof(sp), false, "negative");
        w = makeWitness(x, {Fr(8), Fr(3), Fr(5), Fr(8), Fr(3)});
        w.query.back() = Fr(5);
        reject("regenerated_bad_query_padding", encodeProof(adversarial::proveWitness(x, w)));
        w = makeWitness(x, {Fr(8), Fr(3), Fr(5), Fr(8), Fr(3)});
        w.selector.dim[0].back() = Fr(1);
        reject("regenerated_bad_dim_padding", encodeProof(adversarial::proveWitness(x, w)));
        w = makeWitness(x, {Fr(8), Fr(3), Fr(5), Fr(8), Fr(3)});
        w.memories[0].read[0] = Fr(13);
        reject("regenerated_bad_read_counter", encodeProof(adversarial::proveWitness(x, w)));
        throws(
            [&] {
                prove(x, {Fr(99), Fr(3), Fr(5), Fr(8), Fr(3)});
            },
            "honest prover invalid query");
        throws([&] { makeSelector(x, {3, 0, 0, 0, 0}); }, "honest prover invalid address");
        throws([&] { makeInstance({}, 1, {0}, false, "empty"); }, "empty table");
        throws([&] { makeInstance({Fr(1)}, 0, {0}, false, "empty"); }, "empty query");
        throws([&] { makeInstance({Fr(1)}, 257, {0}, false, "oversize"); }, "query limit");
        throws([&] { makeInstance(std::vector<Fr>(257, Fr(1)), 1, {8}, false, "oversize-table"); },
               "table limit");
        auto malformed = makeWitness(x, {Fr(8), Fr(3), Fr(5), Fr(8), Fr(3)});
        malformed.memories[0].final.pop_back();
        throws([&] { adversarial::proveWitness(x, malformed); },
               "malformed adversarial witness shape");
        Bytes oversized(kMaxArtifact + 1, 0);
        throws([&] { decodeProof(oversized); }, "oversized decoder input");
        std::ofstream oversized_file(dir / "oversized_file.lasso", std::ios::binary);
        oversized_file.write(reinterpret_cast<const char *>(oversized.data()), oversized.size());
        oversized_file.close();
        writeFile((dir / "oversized_file.instance").string(), encodeInstance(x));
        manifest << "oversized_file\t1\tnegative\n";
        throws([&] { readFile((dir / "oversized_file.lasso").string()); }, "oversized file input");
        throws([&] { makeInstance(seven, 2, {2}, false, "wronglayout"); }, "wrong address width");
        throws(
            [&] {
                makeInstance({Fr(1), Fr(2), Fr(4), Fr(8)}, 2, {1, 1}, true, "nonsos");
            },
            "non SOS rejected");
        throws([&] { makeInstance(seven, 2, {0, 3}, true, "badsegment"); }, "zero-width segment");
        // Byte-stable deterministic transcript: a fresh run produces identical wire bytes.
        check(encodeProof(prove(x, {Fr(8), Fr(3), Fr(5), Fr(8), Fr(3)})) == valid,
              "deterministic transcript replay");
        std::cout << "TOTAL " << passed << " checks PASS\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
