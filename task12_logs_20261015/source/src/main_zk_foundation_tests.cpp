#include "zk_hyrax.hpp"
#include "zk_scalar.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace zkgpt::zk;
namespace {
int passed = 0, failed = 0;
void check(bool b, const char *s) {
    if (!b)
        throw std::runtime_error(s);
}
template <class F> void test(const std::string &s, F f) {
    try {
        f();
        ++passed;
        std::cout << "PASS " << s << '\n';
    } catch (const std::exception &e) {
        ++failed;
        std::cout << "FAIL " << s << ": " << e.what() << '\n';
    }
}
template <class F> void throws(F f) {
    bool b = false;
    try {
        f();
    } catch (const std::exception &) {
        b = true;
    }
    check(b, "expected local/decode rejection");
}
Transcript session(const std::string &s = "test/session") { return Transcript(s); }
std::vector<Fr> fields(size_t n) {
    std::vector<Fr> a(n);
    for (size_t i = 0; i < n; ++i)
        a[i] = Fr(i + 2);
    return a;
}
} // namespace
int main() {
    mcl::bn::initPairing(mcl::BN254);
    auto pp = makeParameters(32);
    test("canonical uniform scalar and transcript adapter", [&] {
        std::array<uint8_t, 32> raw{};
        Fr z;
        check(scalarCandidate(raw, z) && z.isZero(), "zero canonical");
        raw.fill(255);
        check(!scalarCandidate(raw, z), "noncanonical scalar accepted");
        auto a = session(), b = session(), c = session("other");
        auto x = a.challengeUniform("e");
        check(x == b.challengeUniform("e"), "determinism");
        check(x != c.challengeUniform("e"), "domain binding");
        check(x != a.challengeUniform("e"), "state binding");
        check(sampleScalar() != sampleScalar(), "fresh entropy");
    });
    ScalarWitness x{Fr(3), sampleScalar()}, y{Fr(5), sampleScalar()}, z{Fr(15), sampleScalar()};
    auto X = scalarCommit(pp, x), Y = scalarCommit(pp, y), Z = scalarCommit(pp, z);
    test("representation batch positives and verifier rejects", [&] {
        auto t = session();
        auto p = proveRepresentation(pp, {X, Y}, {x, y}, t, "coefficients", "ctx");
        auto v = session();
        check(verifyRepresentation(pp, {X, Y}, p, v, "coefficients", "ctx"), "representation");
        auto q = session();
        check(!verifyRepresentation(pp, {X, Z}, p, q, "coefficients", "ctx"),
              "wrong representation accepted");
        auto r = session();
        check(!verifyRepresentation(pp, {X, Y}, p, r, "coefficients", "spliced"), "context splice");
        auto d = session("different");
        check(!verifyRepresentation(pp, {X, Y}, p, d, "coefficients", "ctx"), "domain splice");
        auto u = session();
        auto fresh = proveRepresentation(pp, {X, Y}, {x, y}, u, "coefficients", "ctx");
        check(encode(p) != encode(fresh), "reused nonce");
        auto bytes = encode(p);
        check(encode(decodeRepresentation(bytes, 2)) == bytes, "representation codec");
        throws([&] { decodeRepresentation(bytes, 1); });
        p.z_value[0] += Fr(1);
        auto w = session();
        check(!verifyRepresentation(pp, {X, Y}, p, w, "coefficients", "ctx"),
              "wrong response accepted");
    });
    test("equality positives and independently rejected wrong public value", [&] {
        std::vector<Fr> a{Fr(2), Fr(1)};
        auto t = session();
        auto p = proveEquality(pp, {X, Y}, a, Fr(11), {x, y}, t, "eq", "ctx");
        auto v = session();
        check(verifyEquality(pp, {X, Y}, a, Fr(11), p, v, "eq", "ctx"), "equality");
        auto w = session();
        check(!verifyEquality(pp, {X, Y}, a, Fr(12), p, w, "eq", "ctx"), "wrong equality accepted");
        check(encode(decodeEquality(encode(p))) == encode(p), "equality codec");
    });
    test("product positives and independently rejected well formed wrong product", [&] {
        auto t = session();
        auto p = proveProduct(pp, X, Y, Z, x, y, z, t, "mul", "ctx");
        auto v = session();
        check(verifyProduct(pp, X, Y, Z, p, v, "mul", "ctx"), "product");
        auto w = session();
        check(!verifyProduct(pp, X, Y, Z + pp.U, p, w, "mul", "ctx"), "wrong product accepted");
        p.st += Fr(1);
        auto q = session();
        check(!verifyProduct(pp, X, Y, Z, p, q, "mul", "ctx"), "wrong product response");
        auto b = encode(p);
        check(encode(decodeProduct(b)) == b, "product codec");
    });
    for (auto layout : {Layout::RowMajor, Layout::ColumnMajor})
        for (size_t n :
             {size_t(1), size_t(4), size_t(8), size_t(16), size_t(32), size_t(256), size_t(1024)})
            test("Hyrax n=" + std::to_string(n) + " layout=" + std::to_string(int(layout)), [&] {
                auto shape = matrixShape(n);
                auto p = makeParameters(shape.columns);
                auto w = fields(n);
                auto c = commitMatrix(p, w, layout);
                std::vector<Fr> point(shape.log_size);
                for (size_t j = 0; j < point.size(); ++j)
                    point[j] = Fr(j + 2);
                auto folded = w;
                for (const auto &r : point) {
                    for (size_t i = 0; i < folded.size() / 2; ++i)
                        folded[i] = folded[2 * i] * (Fr(1) - r) + folded[2 * i + 1] * r;
                    folded.resize(folded.size() / 2);
                }
                auto value = folded[0];
                check(evaluateMle(w, point, layout) == value,
                      "MLE reduction differs from independent full fold");
                ScalarWitness yw{value, sampleScalar()};
                MleStatement s{c.public_commitment, point, scalarCommit(p, yw), "hyrax/session"};
                auto t = session();
                auto proof = proveMle(p, s, c.witness, yw.blinder, t, "mle");
                auto v = session();
                check(verifyMle(p, s, proof, v, "mle"), "Hyrax positive");
                check(proof.z.size() == shape.columns, "response is not collapsed columns");
                auto bytes = encode(proof);
                check(bytes.size() == 8 + 32 * (4 + shape.columns), "encoded bytes");
                check(encode(decodeLinear(bytes, shape.columns)) == bytes, "linear codec");
                std::cout << "MEASURE n=" << n << " cols=" << proof.z.size()
                          << " bytes=" << bytes.size() << '\n';
                auto changed = s;
                changed.evaluation += p.U;
                auto a = session();
                check(!verifyMle(p, changed, proof, a, "mle"), "changed Dy");
                changed = s;
                changed.commitment.rows[0] += p.U;
                auto b = session();
                check(!verifyMle(p, changed, proof, b, "mle"), "changed row");
                changed = s;
                changed.commitment.layout =
                    layout == Layout::RowMajor ? Layout::ColumnMajor : Layout::RowMajor;
                auto d = session();
                check(!verifyMle(p, changed, proof, d, "mle"), "layout splice");
                if (!point.empty()) {
                    changed = s;
                    changed.point[0] += Fr(1);
                    auto e = session();
                    check(!verifyMle(p, changed, proof, e, "mle"), "point splice");
                }
                auto bad = proof;
                bad.z[0] += Fr(1);
                auto f = session();
                check(!verifyMle(p, s, bad, f, "mle"), "wrong z");
                bad.z.push_back(Fr(0));
                auto g = session();
                check(!verifyMle(p, s, bad, g, "mle"), "response length");
                auto u = session();
                auto fresh = proveMle(p, s, c.witness, yw.blinder, u, "mle");
                check(encode(fresh) != bytes, "fresh opening randomness");
                auto wrong = c.witness;
                wrong.values[0] += Fr(1);
                auto h = session();
                throws([&] { proveMle(p, s, wrong, yw.blinder, h, "mle"); });
            });
    test("limits invalid points canonical codec and empty cases", [&] {
        throws([&] { matrixShape(0); });
        throws([&] { matrixShape(3); });
        throws([&] { matrixShape(size_t(1) << 31); });
        throws([&] { makeParameters(0); });
        auto bad = pp;
        bad.domain = "splice";
        throws([&] { validateParameters(bad); });
        bad = pp;
        bad.H = bad.U;
        throws([&] { validateParameters(bad); });
        G1 invalid;
        invalid.clear();
        invalid.x = 1;
        invalid.y = 1;
        invalid.z = 1;
        check(!validPoint(invalid), "fixture invalid");
        auto t = session();
        RepresentationProof empty;
        check(!verifyRepresentation(pp, {invalid}, empty, t, "r", "c"), "invalid point accepted");
        Bytes malformed(72, 255);
        throws([&] { decodeRepresentation(malformed, 1); });
        auto q = session();
        auto proof = proveProduct(pp, X, Y, Z, x, y, z, q, "p", "c");
        auto bytes = encode(proof);
        bytes.push_back(0);
        throws([&] { decodeProduct(bytes); });
        bytes = encode(proof);
        for (size_t i = 0; i < 32; ++i)
            bytes[i] = 255;
        throws([&] { decodeProduct(bytes); });
        bytes = encode(proof);
        for (size_t i = 64; i < 96; ++i)
            bytes[i] = 255;
        throws([&] { decodeProduct(bytes); });
        throws([&] { decodeLinear(Bytes(8, 255), 32); });
    });
    test("production basis preserves val0 anchor and global scalar bases", [&] {
        auto main_pp = makeProductionParameters({"zkGPT/main", 4});
        auto exp_pp = makeProductionParameters({"zkGPT/exp-lookup/v4", 8});
        check(main_pp.G == getGeneratorSet({"zkGPT/main", 4}).generators, "val0 basis changed");
        check(main_pp.U == pp.U && main_pp.H == pp.H && exp_pp.U == pp.U && exp_pp.H == pp.H,
              "scalar bases not shared");
        validateParameters(main_pp);
        validateParameters(exp_pp);
        auto bad = main_pp;
        bad.G[0] = bad.U;
        throws([&] { validateParameters(bad); });
        bad = main_pp;
        bad.vector_domain = "unsupported";
        throws([&] { validateParameters(bad); });
        auto values = fields(16);
        std::vector<Fr> blinders;
        auto c = commitMatrix(main_pp, HyraxFrWitnessView(values.data(), values.size()),
                              Layout::RowMajor, blinders);
        for (size_t i = 0; i < 4; ++i) {
            G1 expected;
            expected.clear();
            for (size_t j = 0; j < 4; ++j)
                expected += main_pp.G[j] * values[i * 4 + j];
            check(c.rows[i] == expected + main_pp.H * blinders[i], "anchor difference not pure H");
        }
        std::vector<Fr> point{Fr(2), Fr(3), Fr(5), Fr(7)};
        ScalarWitness yw{evaluateMle(values, point, Layout::RowMajor), sampleScalar()};
        MleStatement s{c, point, scalarCommit(main_pp, yw), "production/session"};
        auto t = session();
        auto proof =
            proveMle(main_pp, s,
                     MatrixWitnessView{HyraxFrWitnessView(values.data(), values.size()), blinders},
                     yw.blinder, t, "mle");
        auto v = session();
        check(verifyMle(main_pp, s, proof, v, "mle"), "borrowed production opening");
    });

    test("interactive Sigma special-HVZK and two-challenge extraction algebra", [&] {
        // Fixed-challenge algebra, not an FS simulator or a ZK certification.
        // Every simulator below takes public statements and e only.
        const Fr e = 5, e2 = 9;
        const auto simulateRep = [&](const G1 &D, const Fr &challenge) {
            Fr zv = sampleScalar(), zr = sampleScalar();
            G1 A = pp.U * zv + pp.H * zr - D * challenge;
            check(pp.U * zv + pp.H * zr == A + D * challenge, "representation simulator");
        };
        simulateRep(X, e);
        simulateRep(Y, e2);
        Fr nv = sampleScalar(), nr = sampleScalar();
        G1 A = pp.U * nv + pp.H * nr;
        Fr zv = nv + e * x.value, zr = nr + e * x.blinder;
        Fr zv2 = nv + e2 * x.value, zr2 = nr + e2 * x.blinder;
        check(pp.U * zv + pp.H * zr == A + X * e && pp.U * zv2 + pp.H * zr2 == A + X * e2,
              "representation forks");
        check((zv2 - zv) / (e2 - e) == x.value && (zr2 - zr) / (e2 - e) == x.blinder,
              "representation extraction");
        G1 E = X + Y - pp.U * Fr(8);
        Fr sz = sampleScalar();
        G1 SA = pp.H * sz - E * e;
        check(pp.H * sz == SA + E * e, "equality simulator");
        const Fr equality_nonce=sampleScalar();
        const G1 equality_A=pp.H*equality_nonce;
        const Fr equality_blind=x.blinder+y.blinder;
        const Fr equality_z=equality_nonce+e*equality_blind;
        const Fr equality_z2=equality_nonce+e2*equality_blind;
        check(pp.H*equality_z==equality_A+E*e &&
              pp.H*equality_z2==equality_A+E*e2,"equality shared-first-message forks");
        const Fr extracted_equality_blind=(equality_z2-equality_z)/(e2-e);
        check(extracted_equality_blind==equality_blind && E==pp.H*extracted_equality_blind,
              "equality extraction");
        const auto simulateProduct = [&](const G1 &Cx, const G1 &Cy, const G1 &Cz,
                                         const Fr &challenge) {
            Fr sx = sampleScalar(), sr = sampleScalar(), st = sampleScalar();
            G1 a = pp.U * sx + pp.H * sr - Cx * challenge;
            G1 b = Cy * sx + pp.H * st - Cz * challenge;
            check(pp.U * sx + pp.H * sr == a + Cx * challenge &&
                      Cy * sx + pp.H * st == b + Cz * challenge,
                  "product simulator");
        };
        simulateProduct(X, Y, Z, e);
        Fr a = sampleScalar(), b = sampleScalar(), c = sampleScalar();
        G1 PA = pp.U * a + pp.H * b, PB = Y * a + pp.H * c;
        auto response = [&](const Fr &challenge) {
            return std::array<Fr, 3>{a + challenge * x.value, b + challenge * x.blinder,
                                     c + challenge * (z.blinder - x.value * y.blinder)};
        };
        auto p = response(e), q = response(e2);
        for (const auto &pair : {std::make_pair(e, p), std::make_pair(e2, q)}) {
            check(pp.U * pair.second[0] + pp.H * pair.second[1] == PA + X * pair.first,
                  "product first equation");
            check(Y * pair.second[0] + pp.H * pair.second[2] == PB + Z * pair.first,
                  "product second equation");
        }
        Fr extracted_x = (q[0] - p[0]) / (e2 - e), delta = (q[2] - p[2]) / (e2 - e);
        check(extracted_x == x.value && Z == Y * extracted_x + pp.H * delta,
              "product extraction/provenance");
        check((q[1]-p[1])/(e2-e)==x.blinder,"product first-opening blinder extraction");
        check(scalarCommit(pp,{extracted_x*y.value,extracted_x*y.blinder+delta})==Z,
              "product Cz opening derived from extracted x and known Cy opening");
        auto hp = makeParameters(4);
        auto w = fields(4);
        std::vector<Fr> mu{Fr(2), Fr(3), Fr(5), Fr(7)};
        Fr rho = sampleScalar(), blind = sampleScalar(), value = 0;
        G1 C = hp.H * rho;
        for (size_t i = 0; i < 4; ++i) {
            C += hp.G[i] * w[i];
            value += mu[i] * w[i];
        }
        G1 Dy = scalarCommit(hp, {value, blind});
        std::vector<Fr> sim_z(4);
        Fr sim_r = sampleScalar(), sim_s = sampleScalar(), sim_dot = 0;
        G1 AC = hp.H * sim_r - C * e;
        for (size_t i = 0; i < 4; ++i) {
            sim_z[i] = sampleScalar();
            AC += hp.G[i] * sim_z[i];
            sim_dot += mu[i] * sim_z[i];
        }
        G1 Ay = hp.U * sim_dot + hp.H * sim_s - Dy * e;
        G1 lhs = hp.H * sim_r;
        for (size_t i = 0; i < 4; ++i)
            lhs += hp.G[i] * sim_z[i];
        check(lhs == AC + C * e && hp.U * sim_dot + hp.H * sim_s == Ay + Dy * e,
              "joint hidden-opening simulator");
        std::vector<Fr> nonce(4), first(4), second(4);
        Fr ra = sampleScalar(), rb = sampleScalar(), ndot = 0;
        G1 realA = hp.H * ra;
        for (size_t i = 0; i < 4; ++i) {
            nonce[i] = sampleScalar();
            realA += hp.G[i] * nonce[i];
            ndot += mu[i] * nonce[i];
            first[i] = nonce[i] + e * w[i];
            second[i] = nonce[i] + e2 * w[i];
            check((second[i] - first[i]) / (e2 - e) == w[i], "collapsed vector extraction");
        }
        G1 realB = hp.U * ndot + hp.H * rb;
        for (const auto &challenge : {e, e2}) {
            G1 lhs = hp.H * (ra + challenge * rho);
            Fr d = 0;
            for (size_t i = 0; i < 4; ++i) {
                Fr response = nonce[i] + challenge * w[i];
                lhs += hp.G[i] * response;
                d += mu[i] * response;
            }
            check(lhs == realA + C * challenge &&
                      hp.U * d + hp.H * (rb + challenge * blind) == realB + Dy * challenge,
                  "joint opening fork equations");
        }
        check(((ra + e2 * rho) - (ra + e * rho)) / (e2 - e) == rho &&
                  ((rb + e2 * blind) - (rb + e * blind)) / (e2 - e) == blind,
              "joint blinder extraction");
    });
    std::cout << passed << " passed, " << failed << " failed\n";
    return failed ? 1 : 0;
}
