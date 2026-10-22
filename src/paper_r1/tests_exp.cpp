#include "exp.hpp"
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
        ExpSpec s{"exp/unit", PCSpec::make("exp/t", 6, 8, Layout::ColumnMajor),
                  PCSpec::make("exp/E", 6, 8, Layout::RowMajor)};
        std::vector<Fr> index{Fr(0),    Fr(6891), Fr(7453),
                              Fr(8191), Fr(8192), Fr(std::uint64_t(0xffffffff)),
                              Fr(0),    Fr(0)},
            value(8, Fr(0));
        for (unsigned i = 0; i < 6; ++i)
            value[i] = Fr(expValue(i == 5 ? 0xffffffff : std::uint32_t(index[i].getInt64())));
        auto ct = commit(s.index, index), ce = commit(s.value, value);
        Fs pt("exp"), vt("exp");
        auto p = proveExp(s, ct, ce, index, value, pt);
        verifyExp(s, ct, ce, p, vt);
        check(pt.digest() == vt.digest(), "unclamped Exp transcript");
        check(expValue(0) == 1048576 && expValue(8192) == 0 && expValue(0xffffffff) == 0,
              "exact Exp tail");
        auto wrong = value;
        wrong[4] = Fr(1);
        rejects(
            [&] {
                Fs t("exp");
                proveExp(s, ct, commit(s.value, wrong), index, wrong, t);
            },
            "reject old clamp1 E(8192)");
        auto bad = p;
        bad.constraints[0].rounds[0][0] += Fr(1);
        rejects(
            [&] {
                Fs t("exp");
                verifyExp(s, ct, ce, bad, t);
            },
            "Exp branch sumcheck tamper");
        auto out = index;
        out[0] = Fr(std::uint64_t(1) << 32);
        rejects(
            [&] {
                Fs t("exp");
                proveExp(s, commit(s.index, out), ce, out, value, t);
            },
            "Exp index outside u32");
        std::cout << "PASS Exp " << checks << " checks\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << "\n";
        return 1;
    }
}
