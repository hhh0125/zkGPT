#include "input.hpp"
#include <iostream>
#include <functional>
using namespace zkgpt::r1;
static unsigned checks = 0;
static void check(bool b, const char *l) {
    ++checks;
    require(b, l);
}
static void rejects(std::function<void()> f, const char *l) {
    bool bad = false;
    try {
        f();
    } catch (const std::exception &) {
        bad = true;
    }
    check(bad, l);
}
int main() {
    try {
        init();
        for (unsigned d : {0, 1, 3, 9}) {
            std::size_t n = std::size_t(1) << d;
            InputSpec s;
            s.context = "unit/model-v1/public-X-Y";
            s.x.resize(n);
            std::array<std::vector<Fr>, 3> w;
            for (std::size_t i = 0; i < n; ++i)
                s.x[i] = Fr(i + 1);
            for (unsigned b = 0; b < 3; ++b) {
                s.spec[b] = PCSpec::make("input/block/" + std::to_string(b), n, n,
                                         b == 1 ? Layout::RowMajor : Layout::ColumnMajor);
                w[b].resize(n);
                for (std::size_t i = 0; i < n; ++i)
                    w[b][i] = Fr(3 + i * i + 7 * b);
                s.commitment[b] = commit(s.spec[b], w[b]);
            }
            std::vector<Fr> all = s.x;
            for (auto &v : w)
                all.insert(all.end(), v.begin(), v.end());
            std::vector<InputClaim> claims;
            for (unsigned j = 0; j < 7; ++j) {
                std::vector<Fr> r(d + 2);
                for (unsigned i = 0; i < d + 2; ++i)
                    r[i] = Fr(j + i + 2);
                claims.push_back({r, mle(all, r)});
            }
            // Exact selector vertices cover X/W/aux/L; last term must be a*b, never X's selector.
            for (unsigned block = 0; block < 4; ++block) {
                std::vector<Fr> r(d + 2, Fr(0));
                r[d] = Fr(block & 1);
                r[d + 1] = Fr(block >> 1);
                claims.push_back({r, mle(all, r)});
            }
            Fs pt("input"), vt("input");
            auto p = proveInput(s, w, claims, pt);
            verifyInput(s, claims, p, vt);
            check(pt.digest() == vt.digest(), "input FS");
            auto bad = p;
            bad.opening[2].value += Fr(1);
            rejects(
                [&] {
                    Fs t("input");
                    verifyInput(s, claims, bad, t);
                },
                "L terminal opening required");
            auto cc = claims;
            cc[0].value += Fr(1);
            rejects(
                [&] {
                    Fs t("input");
                    verifyInput(s, cc, p, t);
                },
                "input claim tamper");
            auto ss = s;
            ss.x[0] += Fr(1);
            rejects(
                [&] {
                    Fs t("input");
                    verifyInput(ss, claims, p, t);
                },
                "public X tamper");
            ss = s;
            ss.context += "/different-Y";
            rejects(
                [&] {
                    Fs t("input");
                    verifyInput(ss, claims, p, t);
                },
                "public context tamper");
            bad = p;
            bad.selectors.rounds[0][0] += Fr(1);
            rejects(
                [&] {
                    Fs t("input");
                    verifyInput(s, claims, bad, t);
                },
                "selector round tamper");
        }
        for (unsigned bad_block : {0, 1}) {
            InputSpec s;
            s.context = "padding-counterexample";
            s.x = std::vector<Fr>(4, Fr(0));
            std::array<std::vector<Fr>, 3> w;
            for (unsigned b = 0; b < 3; ++b) {
                s.spec[b] =
                    PCSpec::make("padding/block/" + std::to_string(b), 3, 4, Layout::ColumnMajor);
                w[b] = std::vector<Fr>(4, Fr(0));
            }
            w[bad_block][3] = Fr(1);
            for (unsigned b = 0; b < 3; ++b)
                s.commitment[b] = commit(s.spec[b], w[b]);
            std::vector<Fr> all = s.x;
            for (auto &v : w)
                all.insert(all.end(), v.begin(), v.end());
            std::vector<Fr> r(4, Fr(2));
            std::vector<InputClaim> claims{{r, mle(all, r)}};
            rejects(
                [&] {
                    Fs t("bad-tail");
                    proveInput(s, w, claims, t);
                },
                bad_block == 0 ? "nonzero W tail" : "nonzero aux tail");
        }
        std::cout << "PASS input " << checks << " checks\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << "\n";
        return 1;
    }
}
