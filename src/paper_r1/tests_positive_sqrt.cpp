#include "positive_sqrt.hpp"
#include <functional>
#include <iostream>
using namespace zkgpt::r1;
static unsigned checks = 0;
static void check(bool b, const char *l) {
    ++checks;
    require(b, l);
}
static void rejects(const std::function<void()> &f, const char *l) {
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
        PositiveSqrtSpec s{"sqrt/unit", PCSpec::make("sqrt/x", 7, 8, Layout::RowMajor),
                           PCSpec::make("sqrt/y", 7, 8, Layout::ColumnMajor), 64, 32};
        // Hand-derived threshold cases on either side of y^2+y.
        std::vector<Fr> x{Fr(1), Fr(2), Fr(3), Fr(6), Fr(7), Fr(12), Fr(13), Fr(0)};
        std::vector<Fr> y{Fr(1), Fr(1), Fr(2), Fr(2), Fr(3), Fr(3), Fr(4), Fr(0)};
        auto cx = commit(s.input, x), cy = commit(s.root, y);
        Fs pt("positive-sqrt"), vt("positive-sqrt");
        auto p = provePositiveSqrt(s, cx, cy, x, y, pt);
        verifyPositiveSqrt(s, cx, cy, p, vt);
        check(pt.digest() == vt.digest(), "positive sqrt transition boundaries");
        for (unsigned i : {1U, 2U}) {
            auto yy = y;
            yy[i] += i == 1 ? Fr(1) : Fr(-1);
            rejects(
                [&] {
                    Fs t("positive-sqrt");
                    provePositiveSqrt(s, cx, commit(s.root, yy), x, yy, t);
                },
                "wrong adjacent rounded root");
        }
        for (Fr value : {Fr(0), Fr(-1)}) {
            auto yy = y;
            yy[0] = value;
            rejects(
                [&] {
                    Fs t("positive-sqrt");
                    provePositiveSqrt(s, cx, commit(s.root, yy), x, yy, t);
                },
                "positive-root domain excludes zero and negative root");
        }
        auto xx = x;
        xx[0] = Fr(0);
        rejects(
            [&] {
                Fs t("positive-sqrt");
                provePositiveSqrt(s, commit(s.input, xx), cy, xx, y, t);
            },
            "positive input domain excludes zero");
        auto bad = p;
        bad.constraints[0].rounds[0][0] += Fr(1);
        rejects(
            [&] {
                Fs t("positive-sqrt");
                verifyPositiveSqrt(s, cx, cy, bad, t);
            },
            "quadratic relation sumcheck tamper");
        bad = p;
        bad.endpoint[1][1].value += Fr(1);
        rejects(
            [&] {
                Fs t("positive-sqrt");
                verifyPositiveSqrt(s, cx, cy, bad, t);
            },
            "same source root PC endpoint");
        xx = x;
        xx[7] = Fr(1);
        rejects(
            [&] {
                Fs t("positive-sqrt");
                provePositiveSqrt(s, commit(s.input, xx), cy, xx, y, t);
            },
            "input padding rejected");
        auto yy = y;
        yy[7] = Fr(1);
        rejects(
            [&] {
                Fs t("positive-sqrt");
                provePositiveSqrt(s, cx, commit(s.root, yy), x, yy, t);
            },
            "root padding rejected");
        PositiveSqrtSpec edge{"sqrt/edge", PCSpec::make("edge/x", 1, 1, Layout::ColumnMajor),
                              PCSpec::make("edge/y", 1, 1, Layout::ColumnMajor), 64, 32};
        // MCL's direct Fr constructor accepts int64: UINT64_MAX would become -1.
        Bytes max_u64(32, 0);
        for (unsigned i = 0; i < 8; ++i)
            max_u64[i] = 255;
        std::vector<Fr> ex{decodeFr(max_u64)}, ey{Fr(std::uint64_t(1) << 32)};
        auto ecx = commit(edge.input, ex), ecy = commit(edge.root, ey);
        Fs ep("sqrt-edge"), ev("sqrt-edge");
        auto pe = provePositiveSqrt(edge, ecx, ecy, ex, ey, ep);
        verifyPositiveSqrt(edge, ecx, ecy, pe, ev);
        check(ep.digest() == ev.digest(), "max u64 input rounds to 2^32 without overflow");
        std::cout << "PASS positive sqrt " << checks << " checks\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << "\n";
        return 1;
    }
}
