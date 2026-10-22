#include "pc.hpp"
#include <iostream>
#include <functional>
using namespace zkgpt::r1;
static unsigned tests = 0;
static void check(bool ok, const char *name) {
    ++tests;
    if (!ok)
        throw std::runtime_error(name);
}
static void rejects(const std::function<void()> &f, const char *name) {
    bool bad = false;
    try {
        f();
    } catch (const std::exception &) {
        bad = true;
    }
    check(bad, name);
}
int main() {
    try {
        init();
        check(Fr::getModulo() == kModulus, "runtime field");
        Fs ta("unit"), tb("unit"), tc("other");
        ta.text("x", "abc");
        tb.text("x", "abc");
        tc.text("x", "abc");
        check(ta.challenge("r") == tb.challenge("r"), "deterministic FS");
        Fs domain_a("unit"), domain_b("other");
        domain_a.text("x", "abc");
        domain_b.text("x", "abc");
        check(domain_a.challenge("s") != domain_b.challenge("s"),
              "domain FS with identical history");
        Fs td("unit"), te("unit");
        td.text("a", "bc");
        te.text("ab", "c");
        check(td.challenge("r") != te.challenge("r"), "framed FS");
        rejects([] { PCSpec::make("bad", 0, 0, Layout::ColumnMajor); }, "zero PC");
        rejects([] { PCSpec::make("bad", 3, 3, Layout::ColumnMajor); }, "non-power PC");
        for (auto layout : {Layout::ColumnMajor, Layout::RowMajor})
            for (unsigned d = 0; d <= 9; ++d) {
                std::size_t n = std::size_t(1) << d;
                auto s = PCSpec::make("pc/unit", n, n, layout);
                check(s.rows == (std::size_t(1) << (d / 2)), "rows");
                check(s.columns == n / s.rows, "columns");
                std::vector<Fr> w(n);
                for (std::size_t i = 0; i < n; ++i)
                    w[i] = Fr(i * i + 3 * i + 7);
                std::vector<Fr> r(d);
                for (unsigned i = 0; i < d; ++i)
                    r[i] = Fr(i + 2);
                auto c = commit(s, w);
                Fs pt("pc"), vt("pc");
                auto p = open(s, c, w, r, pt, "claim");
                check(p.value == mle(w, r), "PC vs direct MLE");
                verify(s, c, r, p, vt, "claim");
                check(pt.digest() == vt.digest(), "PC FS convergence");
                auto bad = p;
                bad.value += Fr(1);
                rejects(
                    [&] {
                        Fs t("pc");
                        verify(s, c, r, bad, t, "claim");
                    },
                    "wrong value");
                auto cc = c;
                cc.rows[0] += generators(s).u;
                rejects(
                    [&] {
                        Fs t("pc");
                        verify(s, cc, r, p, t, "claim");
                    },
                    "wrong commitment");
                auto other = s;
                other.role = "pc/other";
                rejects(
                    [&] {
                        Fs t("pc");
                        verify(other, c, r, p, t, "claim");
                    },
                    "expected PC role");
                auto ww = w;
                ww[0] += Fr(1);
                rejects(
                    [&] {
                        Fs t("pc");
                        open(s, c, ww, r, t, "claim");
                    },
                    "wrong witness");
                if (d) {
                    auto rr = r;
                    rr[0] += Fr(1);
                    rejects(
                        [&] {
                            Fs t("pc");
                            verify(s, c, rr, p, t, "claim");
                        },
                        "wrong point");
                }
            }
        auto a = PCSpec::make("x", 1 << 22, 1 << 22, Layout::ColumnMajor);
        check(a.rows == 2048 && a.columns == 2048, "actual Range balanced shape");
        std::cout << "PASS core " << tests << " checks\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << "\n";
        return 1;
    }
}
