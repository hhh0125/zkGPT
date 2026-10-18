#include "range_sumcheck.hpp"
#include "zk_hyrax.hpp"
#include <iostream>
#include <stdexcept>
using namespace zkgpt::zk;
namespace {
int passed = 0, failed = 0;
void check(bool value, const char *text) {
    if (!value)
        throw std::runtime_error(text);
}
template <class Fn> void test(const std::string &name, Fn fn) {
    try {
        fn();
        ++passed;
        std::cout << "PASS " << name << '\n';
    } catch (const std::exception &e) {
        ++failed;
        std::cout << "FAIL " << name << ": " << e.what() << '\n';
    }
}
template <class Fn> void rejects(Fn fn) {
    bool rejected = false;
    try {
        fn();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, "expected reject");
}
Transcript session() { return Transcript("zkGPT/task12/sumcheck-tests/v1"); }
Fr mle(std::vector<Fr> v, const std::vector<Fr> &point) {
    for (const auto &r : point) {
        for (size_t i = 0; i < v.size() / 2; ++i)
            v[i] = v[2 * i] + r * (v[2 * i + 1] - v[2 * i]);
        v.resize(v.size() / 2);
    }
    return v.at(0);
}
Fr weightedProduct(const std::vector<Fr> &point, const std::vector<Fr> &f,
                   const std::vector<Fr> &g) {
    Fr total = 0;
    for (size_t i = 0; i < f.size(); ++i) {
        Fr weight = 1;
        for (size_t j = 0; j < point.size(); ++j)
            weight *= ((i >> j) & 1) ? point[j] : Fr(1) - point[j];
        total += weight * f[i] * g[i];
    }
    return total;
}
} // namespace
int main() {
    mcl::bn::initPairing(mcl::BN254);
    for (size_t L : {size_t(0), size_t(2), size_t(4), size_t(8)}) {
        const size_t n = size_t(1) << L;
        auto pp = makeProductionParameters(
            {"zkGPT/range", static_cast<uint32_t>(matrixShape(n).columns)});
        std::vector<Fr> f(n), g(n), eq(L);
        for (size_t i = 0; i < n; ++i) {
            f[i] = Fr(3 * i + 2);
            g[i] = Fr(i * i + 1);
        }
        for (size_t j = 0; j < L; ++j)
            eq[j] = Fr(j + 3);
        auto cf = commitMatrix(pp, f, Layout::ColumnMajor),
             cg = commitMatrix(pp, g, Layout::ColumnMajor);
        const auto parentSession = [&](const MatrixCommitment &first,
                                       const MatrixCommitment &second) {
            auto t = session();
            t.appendString("original.vector-domain", pp.vector_domain);
            for (const auto *c : {&first, &second}) {
                t.appendU64("original.size", c->size);
                t.appendU64("original.layout", static_cast<unsigned>(c->layout));
                t.appendU64("original.rows", c->rows.size());
                for (const auto &row : c->rows)
                    t.appendG1("original.row", row);
            }
            return t;
        };
        const auto boundSession = [&] {
            return parentSession(cf.public_commitment, cg.public_commitment);
        };

        test("real degree1 masked kernel L=" + std::to_string(L), [&] {
            Fr sum = 0;
            for (const auto &x : f)
                sum += x;
            ScalarWitness H{sum, sampleScalar()};
            auto DH = scalarCommit(pp, H);
            auto t = boundSession();
            auto result =
                proveDegree1SumcheckZk(pp, f, DH, H, t, "degree1", "circuit/layer/lookup/session");
            std::vector<Fr> point;
            auto v = boundSession();
            std::string error;
            check(verifyDegree1SumcheckZk(pp, L, DH, result.proof, v, "degree1",
                                          "circuit/layer/lookup/session", &point, &error),
                  error.c_str());
            check(t.stateDigest() == v.stateDigest(), "prover/verifier transcript differs");
            check(t.challengeCounter() == L + 5, "degree1 challenge count/order");
            check(point == result.point && result.terminal_witness.value == mle(f, point),
                  "actual kernel terminal mismatch");
            MleStatement s{cf.public_commitment, point, result.proof.terminal, "same-vector"};
            auto ot = boundSession();
            auto opening =
                proveMle(pp, s, cf.witness, result.terminal_witness.blinder, ot, "terminal");
            auto ov = boundSession();
            check(verifyMle(pp, s, opening, ov, "terminal"), "terminal vector binding");
            auto bytes = encodeDegree1Zk(result.proof, L);
            auto decoded = decodeDegree1Zk(bytes, L);
            auto dv = boundSession();
            check(verifyDegree1SumcheckZk(pp, L, DH, decoded, dv, "degree1",
                                          "circuit/layer/lookup/session"),
                  "codec roundtrip");
            check(result.proof.masked.coefficients.size() == 1 + L, "linear-log mask count");
            auto bad = result.proof;
            bad.masked.coefficients[0] += pp.U;
            auto a = boundSession();
            check(!verifyDegree1SumcheckZk(pp, L, DH, bad, a, "degree1",
                                           "circuit/layer/lookup/session"),
                  "wrong mask accepted");
            bad = result.proof;
            bad.terminal += pp.U;
            auto b = boundSession();
            check(!verifyDegree1SumcheckZk(pp, L, DH, bad, b, "degree1",
                                           "circuit/layer/lookup/session"),
                  "wrong terminal accepted");
            bad = result.proof;
            bad.masked.masked_total += Fr(1);
            auto c = boundSession();
            check(!verifyDegree1SumcheckZk(pp, L, DH, bad, c, "degree1",
                                           "circuit/layer/lookup/session"),
                  "wrong total accepted");
            if (L) {
                bad = result.proof;
                bad.masked.rounds[0][0] += Fr(1);
                auto d = boundSession();
                check(!verifyDegree1SumcheckZk(pp, L, DH, bad, d, "degree1",
                                               "circuit/layer/lookup/session"),
                      "wrong round accepted");
            }
            auto e = boundSession();
            check(!verifyDegree1SumcheckZk(pp, L, DH + pp.U, result.proof, e, "degree1",
                                           "circuit/layer/lookup/session"),
                  "wrong shared commitment accepted");
            auto z = boundSession();
            check(
                !verifyDegree1SumcheckZk(pp, L, DH, result.proof, z, "degree1", "replayed-context"),
                "context replay");
            auto changed_parent = cf.public_commitment;
            changed_parent.rows[0] += pp.U;
            auto cp = parentSession(changed_parent, cg.public_commitment);
            check(!verifyDegree1SumcheckZk(pp, L, DH, result.proof, cp, "degree1",
                                           "circuit/layer/lookup/session"),
                  "changed pre-challenge parent accepted");
            auto rp = parentSession(cg.public_commitment, cf.public_commitment);
            check(!verifyDegree1SumcheckZk(pp, L, DH, result.proof, rp, "degree1",
                                           "circuit/layer/lookup/session"),
                  "reordered parent accepted");
            if (L) {
                bad = result.proof;
                std::swap(bad.masked.coefficients[0], bad.masked.coefficients[1]);
                auto mp = boundSession();
                check(!verifyDegree1SumcheckZk(pp, L, DH, bad, mp, "degree1",
                                               "circuit/layer/lookup/session"),
                      "reordered mask accepted");
            }
            if (L > 1) {
                bad = result.proof;
                std::swap(bad.masked.rounds[0], bad.masked.rounds[1]);
                auto mr = boundSession();
                check(!verifyDegree1SumcheckZk(pp, L, DH, bad, mr, "degree1",
                                               "circuit/layer/lookup/session"),
                      "reordered rounds accepted");
            }
            auto u = boundSession();
            auto fresh =
                proveDegree1SumcheckZk(pp, f, DH, H, u, "degree1", "circuit/layer/lookup/session");
            check(encodeDegree1Zk(fresh.proof, L) != bytes, "fresh masks/nonces");
            bytes.push_back(0);
            rejects([&] { decodeDegree1Zk(bytes, L); });
            auto wrong = H;
            wrong.value += Fr(1);
            auto w = boundSession();
            rejects([&] {
                proveDegree1SumcheckZk(pp, f, DH, wrong, w, "degree1",
                                       "circuit/layer/lookup/session");
            });
            std::cout << "MEASURE degree=1 variables=" << L
                      << " bytes=" << encodeDegree1Zk(result.proof, L).size() << '\n';
        });
        test("real degree3 masked kernel L=" + std::to_string(L), [&] {
            ScalarWitness H{weightedProduct(eq, f, g), sampleScalar()};
            auto DH = scalarCommit(pp, H);
            auto t = boundSession();
            auto result = proveDegree3SumcheckZk(pp, eq, f, g, DH, H, t, "degree3", "same/session");
            std::vector<Fr> point;
            auto v = boundSession();
            std::string error;
            check(verifyDegree3SumcheckZk(pp, eq, DH, result.proof, v, "degree3", "same/session",
                                          &point, &error),
                  error.c_str());
            check(t.stateDigest() == v.stateDigest(), "degree3 transcript mismatch");
            check(t.challengeCounter() == L + 6, "degree3 challenge count/order");
            check(point == result.point && result.f_witness.value == mle(f, point) &&
                      result.g_witness.value == mle(g, point),
                  "raw production folding mismatch");
            for (bool is_f : {true, false}) {
                auto &c = is_f ? cf : cg;
                auto witness = is_f ? result.f_witness : result.g_witness;
                MleStatement s{c.public_commitment, point,
                               is_f ? result.proof.f_terminal : result.proof.g_terminal,
                               "same-vector"};
                auto a = boundSession();
                auto p = proveMle(pp, s, c.witness, witness.blinder, a, "terminal");
                auto b = boundSession();
                check(verifyMle(pp, s, p, b, "terminal"), "degree3 endpoint binding");
            }
            auto bytes = encodeDegree3Zk(result.proof, L);
            auto decoded = decodeDegree3Zk(bytes, L);
            auto cv = boundSession();
            check(verifyDegree3SumcheckZk(pp, eq, DH, decoded, cv, "degree3", "same/session"),
                  "degree3 codec");
            auto bad = result.proof;
            bad.product.st += Fr(1);
            auto a = boundSession();
            check(!verifyDegree3SumcheckZk(pp, eq, DH, bad, a, "degree3", "same/session"),
                  "wrong secret product accepted");
            bad = result.proof;
            bad.g_terminal += pp.U;
            auto b = boundSession();
            check(!verifyDegree3SumcheckZk(pp, eq, DH, bad, b, "degree3", "same/session"),
                  "wrong g accepted");
            check(result.proof.masked.coefficients.size() == 1 + 3 * L, "degree3 log mask count");
            bytes.pop_back();
            rejects([&] { decodeDegree3Zk(bytes, L); });
            std::cout << "MEASURE degree=3 variables=" << L
                      << " bytes=" << encodeDegree3Zk(result.proof, L).size() << '\n';
        });
    }
    test("degree2 streaming adapter zero eq and bounds", [&] {
        auto pp = makeParameters(1);
        ScalarWitness H{Fr(2), sampleScalar()};
        auto DH = scalarCommit(pp, H);
        MaskedSumcheckProof proof;
        auto t = session();
        MaskedSumcheckProver p(pp, 1, 2, DH, H, proof, t, "gkr", "same/session");
        auto r = p.round({Fr(1), Fr(1), Fr(3)}); // p(x)=x^2-x+1
        ScalarWitness F{r * r - r + Fr(1), sampleScalar()};
        auto DF = scalarCommit(pp, F);
        p.finish(DF, F);
        auto v = session();
        MaskedSumcheckVerifier q(pp, 1, 2, DH, proof, v, "gkr", "same/session");
        check(q.round() == r, "stream challenge mismatch");
        q.finish(DF);
        check(t.stateDigest() == v.stateDigest(), "stream transcript");
        // eq(0,1)=0: multiplication by zero, no inversion.
        check(equalityPolynomial({Fr(0)}, {Fr(1)}).isZero(), "zero eq factor");
        rejects([&] {
            auto w = session();
            MaskedSumcheckVerifier b(pp, 61, 2, DH, proof, w, "gkr", "same/session");
        });
        rejects([&] {
            auto w = session();
            MaskedSumcheckVerifier b(pp, 1, 0, DH, proof, w, "gkr", "same/session");
        });
    });

    test("zero terminal equality factor and exact mask challenge order", [&] {
        auto pp = makeParameters(1);
        ScalarWitness H{Fr(0), sampleScalar()};
        auto DH = scalarCommit(pp, H);
        MaskedSumcheckProof proof;
        ChallengeTraceRecorder pt, vt;
        Transcript t("zero-factor", &pt, "mask"), v("zero-factor", &vt, "mask");
        MaskedSumcheckProver p(pp, 1, 2, DH, H, proof, t, "zero", "same/session");
        p.round({Fr(0), Fr(0), Fr(0)});
        ScalarWitness x{Fr(2), sampleScalar()}, y{Fr(3), sampleScalar()},
            product{Fr(6), sampleScalar()};
        const auto X = scalarCommit(pp, x), Y = scalarCommit(pp, y), P = scalarCommit(pp, product);
        const auto knowledge =
            proveRepresentation(pp, {X, Y}, {x, y}, t, "zero.endpoints", "same/session");
        const auto multiplication =
            proveProduct(pp, X, Y, P, x, y, product, t, "zero.product", "same/session");
        const Fr factor = 0;
        const auto terminal = P * factor;
        p.finish(terminal, {product.value * factor, product.blinder * factor});
        rejects([&] {
            auto early = Transcript("zero-factor", nullptr, "mask");
            MaskedSumcheckVerifier early_verifier(pp, 1, 2, DH, proof, early, "zero",
                                                  "same/session");
            early_verifier.finish(terminal);
        });
        MaskedSumcheckVerifier q(pp, 1, 2, DH, proof, v, "zero", "same/session");
        q.round();
        check(verifyRepresentation(pp, {X, Y}, knowledge, v, "zero.endpoints", "same/session"),
              "zero-factor endpoint knowledge");
        check(verifyProduct(pp, X, Y, P, multiplication, v, "zero.product", "same/session"),
              "zero-factor multiplication");
        q.finish(terminal);
        std::string error;
        check(compareChallengeTraces(pt.entries, vt.entries, &error), error.c_str());
        check(pt.entries.size() == 7, "mask/product challenge count");
        check(pt.entries[0].label == "representation-e" && pt.entries[1].label == "zero.mask-rho" &&
                  pt.entries[2].label == "equality-e" &&
                  pt.entries[3].label == "zero.round.challenge" &&
                  pt.entries[4].label == "representation-e" && pt.entries[5].label == "product-e" &&
                  pt.entries[6].label == "equality-e",
              "mask challenge order");
        rejects([&] { q.finish(terminal); });
        rejects([&] { q.round(); });
        auto wrong_terminal = proof;
        wrong_terminal.terminal_relation.z += Fr(1);
        rejects([&] {
            auto w = Transcript("zero-factor", nullptr, "mask");
            MaskedSumcheckVerifier bad_verifier(pp, 1, 2, DH, wrong_terminal, w, "zero",
                                                "same/session");
            bad_verifier.round();
            check(verifyRepresentation(pp, {X, Y}, knowledge, w, "zero.endpoints", "same/session"),
                  "bad terminal prefix PoK");
            check(verifyProduct(pp, X, Y, P, multiplication, w, "zero.product", "same/session"),
                  "bad terminal prefix product");
            bad_verifier.finish(terminal);
        });
        auto bad = proof;
        bad.rounds[0][3] = Fr(1);
        rejects([&] { encodeMaskedSumcheck(bad, 1, 2); });
    });
    std::cout << passed << " passed, " << failed << " failed\n";
    return failed ? 1 : 0;
}
