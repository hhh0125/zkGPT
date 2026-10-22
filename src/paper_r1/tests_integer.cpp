#include "integer_relations.hpp"
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
        RoundingSpec s{"round/unit",
                       PCSpec::make("round/A", 3, 4, Layout::ColumnMajor),
                       PCSpec::make("round/q", 3, 4, Layout::RowMajor),
                       2,
                       8,
                       8};
        std::vector<Fr> a{Fr(1), Fr(-1), Fr(3), Fr(0)}, q{Fr(1), Fr(0), Fr(2), Fr(0)};
        auto ca = commit(s.numerator, a), cq = commit(s.quotient, q);
        Fs pt("round"), vt("round");
        auto p = proveRounding(s, ca, cq, a, q, pt);
        verifyRounding(s, ca, cq, p, vt);
        check(pt.digest() == vt.digest(), "half-open rounding transcript");
        auto badq = q;
        badq[0] = Fr(0);
        rejects(
            [&] {
                Fs t("round");
                proveRounding(s, ca, commit(s.quotient, badq), a, badq, t);
            },
            "positive tie wrong q");
        badq = q;
        badq[1] = Fr(-1);
        rejects(
            [&] {
                Fs t("round");
                proveRounding(s, ca, commit(s.quotient, badq), a, badq, t);
            },
            "negative tie wrong q");
        auto bad = p;
        bad.relation[2].value += Fr(1);
        rejects(
            [&] {
                Fs t("round");
                verifyRounding(s, ca, cq, bad, t);
            },
            "round slack proof tamper");
        MaximumSpec ms{"max/unit", PCSpec::make("max/scores", 3, 4, Layout::ColumnMajor),
                       PCSpec::make("max/value", 1, 1, Layout::ColumnMajor), 8};
        std::vector<Fr> x{Fr(-3), Fr(-2), Fr(-5), Fr(0)}, m{Fr(-2)};
        auto cx = commit(ms.scores, x), cm = commit(ms.maximum, m);
        Fs mp("max"), mv("max");
        auto pm = proveMaximum(ms, cx, cm, x, m[0], mp);
        verifyMaximum(ms, cx, cm, pm, mv);
        check(mp.digest() == mv.digest(), "true maximum all-negative");
        rejects(
            [&] {
                Fs t("max");
                std::vector<Fr> mm{Fr(0)};
                proveMaximum(ms, cx, commit(ms.maximum, mm), x, mm[0], t);
            },
            "upper bound alone is not max; padding is product identity");
        rejects(
            [&] {
                Fs t("max");
                std::vector<Fr> mm{Fr(-4)};
                proveMaximum(ms, cx, commit(ms.maximum, mm), x, mm[0], t);
            },
            "too small maximum");
        auto bm = pm;
        bm.zero_product.layers.back().left += Fr(1);
        rejects(
            [&] {
                Fs t("max");
                verifyMaximum(ms, cx, cm, bm, t);
            },
            "maximum product tamper");
        Fr half64 = 1;
        for (unsigned i = 0; i < 63; ++i)
            half64 *= Fr(2);
        for (std::uint64_t B : {std::uint64_t(1), std::uint64_t(1) << 62}) {
            RoundingSpec edge{"round/edge",
                              PCSpec::make("edge/A", 1, 1, Layout::ColumnMajor),
                              PCSpec::make("edge/q", 1, 1, Layout::ColumnMajor),
                              B,
                              64,
                              64};
            std::vector<Fr> aa{B == 1 ? Fr(-1) : -half64}, qq{B == 1 ? Fr(-1) : Fr(-2)};
            auto ac = commit(edge.numerator, aa), qc = commit(edge.quotient, qq);
            Fs a1("edge"), v1("edge");
            auto ep = proveRounding(edge, ac, qc, aa, qq, a1);
            verifyRounding(edge, ac, qc, ep, v1);
            check(a1.digest() == v1.digest(), "B domain endpoints; signed64; singleton");
        }
        MaximumSpec edge{"max/edge", PCSpec::make("edge/scores", 2, 2, Layout::RowMajor),
                         PCSpec::make("edge/max", 1, 1, Layout::ColumnMajor), 64};
        std::vector<Fr> xx{-half64, half64 - Fr(1)}, mm{half64 - Fr(1)};
        auto xc = commit(edge.scores, xx), mc = commit(edge.maximum, mm);
        Fs e1("edge-max"), e2("edge-max");
        auto ep = proveMaximum(edge, xc, mc, xx, mm[0], e1);
        verifyMaximum(edge, xc, mc, ep, e2);
        check(e1.digest() == e2.digest(), "maximum gap 2^64-1");
        auto padded = a;
        padded[3] = Fr(1);
        rejects(
            [&] {
                Fs t("round");
                proveRounding(s, commit(s.numerator, padded), cq, padded, q, t);
            },
            "nonzero rounding source padding");
        std::cout << "PASS integer relations " << checks << " checks\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << "\n";
        return 1;
    }
}
