#include "matrix.hpp"
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
        MatrixSpec s{"matrix/unit",
                     4,
                     8,
                     2,
                     3,
                     5,
                     2,
                     PCSpec::make("matrix/A", 32, 32, Layout::ColumnMajor),
                     PCSpec::make("matrix/B", 16, 16, Layout::RowMajor)};
        std::vector<Fr> a(32, Fr(0)), b(16, Fr(0));
        for (unsigned i = 0; i < 3; ++i)
            for (unsigned k = 0; k < 5; ++k)
                a[i * 8 + k] = Fr(int(i) - int(k));
        for (unsigned k = 0; k < 5; ++k)
            for (unsigned j = 0; j < 2; ++j)
                b[k * 2 + j] = Fr(k + 2 * j);
        std::vector<Fr> y(8, Fr(0));
        for (unsigned i = 0; i < 4; ++i)
            for (unsigned j = 0; j < 2; ++j)
                for (unsigned k = 0; k < 8; ++k)
                    y[i * 2 + j] += a[i * 8 + k] * b[k * 2 + j];
        auto ca = commit(s.a, a), cb = commit(s.b, b);
        Fs pt("matrix"), vt("matrix");
        auto p = proveMatrix(s, ca, cb, a, b, y, pt);
        verifyMatrix(s, ca, cb, y, p, vt);
        check(pt.digest() == vt.digest(), "dedicated matrix transcript");
        auto yy = y;
        yy[0] += Fr(1);
        rejects(
            [&] {
                Fs t("matrix");
                verifyMatrix(s, ca, cb, yy, p, t);
            },
            "matrix public output tamper");
        auto bad = p;
        bad.a.value += Fr(1);
        rejects(
            [&] {
                Fs t("matrix");
                verifyMatrix(s, ca, cb, y, bad, t);
            },
            "matrix source opening tamper");
        auto aa = a;
        aa[7] = Fr(1);
        rejects(
            [&] {
                Fs t("matrix");
                proveMatrix(s, commit(s.a, aa), cb, aa, b, y, t);
            },
            "matrix inner padding");
        MatrixSpec edge{"matrix/edge",
                        2,
                        1,
                        4,
                        1,
                        1,
                        3,
                        PCSpec::make("edge/A", 2, 2, Layout::ColumnMajor),
                        PCSpec::make("edge/B", 4, 4, Layout::RowMajor)};
        std::vector<Fr> ea{Fr(2), Fr(0)}, eb{Fr(3), Fr(4), Fr(5), Fr(0)};
        std::vector<Fr> ey{Fr(6), Fr(8), Fr(10), Fr(0), Fr(0), Fr(0), Fr(0), Fr(0)};
        auto eca = commit(edge.a, ea), ecb = commit(edge.b, eb);
        Fs et("edge"), ev("edge");
        auto ep = proveMatrix(edge, eca, ecb, ea, eb, ey, et);
        verifyMatrix(edge, eca, ecb, ey, ep, ev);
        check(et.digest() == ev.digest() && ep.sumcheck.rounds.empty(),
              "inner singleton and column padding");
        auto badb = eb;
        badb[3] = Fr(1);
        rejects(
            [&] {
                Fs t("edge");
                proveMatrix(edge, eca, commit(edge.b, badb), ea, badb, ey, t);
            },
            "B column padding");
        auto bady = ey;
        bady[3] = Fr(1);
        rejects(
            [&] {
                Fs t("edge");
                verifyMatrix(edge, eca, ecb, bady, ep, t);
            },
            "C column padding");
        std::cout << "PASS matrix " << checks << " checks\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << "\n";
        return 1;
    }
}
