#include "range.hpp"
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
        for (unsigned bits : {1, 9, 10, 64, 126}) {
            auto s =
                RangeSpec{"range/test", PCSpec::make("query", 5, 8, Layout::ColumnMajor), bits};
            Fr bound = 1;
            for (unsigned i = 0; i < bits; ++i)
                bound *= Fr(2);
            std::vector<Fr> w{Fr(0), Fr(1), bound - Fr(1), Fr(0), Fr(1), Fr(0), Fr(0), Fr(0)};
            auto c = commit(s.query, w);
            Fs pt("range"), vt("range");
            auto p = proveRange(s, c, w, pt);
            verifyRange(s, c, p, vt);
            check(pt.digest() == vt.digest(), "Range transcript");
            check(p.digits.size() == (bits + 8) / 9, "derived digit count");
            auto bad = p;
            bad.query.value += Fr(1);
            rejects(
                [&] {
                    Fs t("range");
                    verifyRange(s, c, bad, t);
                },
                "reconstruction tamper");
            bad = p;
            bad.digit_values.back().value += Fr(1);
            rejects(
                [&] {
                    Fs t("range");
                    verifyRange(s, c, bad, t);
                },
                "partial digit tamper");
            auto ww = w;
            ww[0] = bound;
            rejects(
                [&] {
                    Fs t("range");
                    proveRange(s, commit(s.query, ww), ww, t);
                },
                "2^b boundary");
            auto source = PCSpec::make("source", 5, 8, Layout::RowMajor);
            auto raw = w;
            Fr bias = bound / Fr(2);
            for (unsigned i = 0; i < 5; ++i)
                raw[i] -= bias;
            auto cs = commit(source, raw);
            Fs bt("bind"), bv("bind");
            auto bp = proveRangeBinding(s, source, cs, c, raw, w, true, bt);
            verifyRangeBinding(s, source, cs, c, true, bp, bv);
            check(bt.digest() == bv.digest(), "signed active mask");
        }
        auto s = PCSpec::make("tail", 5, 8, Layout::ColumnMajor);
        std::vector<Fr> w(8, Fr(0));
        auto c = commit(s, w);
        Fs a("tail"), b("tail");
        auto p = proveZeroTail(s, c, w, a);
        verifyZeroTail(s, c, p, b);
        check(a.digest() == b.digest(), "zero tail");
        w[5] = Fr(1);
        w[6] = Fr(-1);
        auto badc = commit(s, w);
        rejects(
            [&] {
                Fs t("tail");
                proveZeroTail(s, badc, w, t);
            },
            "cancelling nonzero tail");
        std::cout << "PASS range " << checks << " checks\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << "\n";
        return 1;
    }
}
