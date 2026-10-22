#include "division.hpp"
#include <functional>
#include <iostream>
using namespace zkgpt::r1;
static unsigned checks = 0;
static void check(bool b, const char *label) {
    ++checks;
    require(b, label);
}
static void rejects(const std::function<void()> &f, const char *label) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, label);
}
int main() {
    try {
        init();
        DivisionSpec s{"div/unit",
                       PCSpec::make("div/A", 5, 8, Layout::ColumnMajor),
                       PCSpec::make("div/B", 5, 8, Layout::RowMajor),
                       PCSpec::make("div/q", 5, 8, Layout::ColumnMajor),
                       64,
                       64,
                       62};
        Fr half = 1;
        for (unsigned i = 0; i < 63; ++i)
            half *= Fr(2);
        std::vector<Fr> a{Fr(1), Fr(-1), Fr(7), Fr(-7), -half, Fr(0), Fr(0), Fr(0)};
        std::vector<Fr> b{Fr(2), Fr(2), Fr(1), Fr(3), Fr(std::uint64_t(1) << 62),
                          Fr(0), Fr(0), Fr(0)};
        std::vector<Fr> q{Fr(1), Fr(0), Fr(7), Fr(-2), Fr(-2), Fr(0), Fr(0), Fr(0)};
        auto ca = commit(s.numerator, a), cb = commit(s.denominator, b), cq = commit(s.quotient, q);
        Fs pt("variable-div"), vt("variable-div");
        auto p = proveDivision(s, ca, cb, cq, a, b, q, pt);
        verifyDivision(s, ca, cb, cq, p, vt);
        check(pt.digest() == vt.digest(), "variable B, signed64, ties, and zero tail");
        for (unsigned index : {0U, 1U}) {
            auto wrong = q;
            wrong[index] -= Fr(1);
            rejects(
                [&] {
                    Fs t("variable-div");
                    proveDivision(s, ca, cb, commit(s.quotient, wrong), a, b, wrong, t);
                },
                "ties must select the unique half-open quotient");
        }
        for (Fr wrong : {Fr(0), Fr(-1), Fr(std::uint64_t(1) << 62) + Fr(1)}) {
            auto bb = b;
            bb[0] = wrong;
            rejects(
                [&] {
                    Fs t("variable-div");
                    proveDivision(s, ca, commit(s.denominator, bb), cq, a, bb, q, t);
                },
                "denominator must be positive and bounded");
        }
        auto bad = p;
        bad.constraints[0].rounds[0][0] += Fr(1);
        rejects(
            [&] {
                Fs t("variable-div");
                verifyDivision(s, ca, cb, cq, bad, t);
            },
            "variable product sumcheck tamper");
        bad = p;
        bad.endpoint[0][1].value += Fr(1);
        rejects(
            [&] {
                Fs t("variable-div");
                verifyDivision(s, ca, cb, cq, bad, t);
            },
            "same denominator PC endpoint");
        auto bb = b;
        bb[0] = Fr(3);
        rejects(
            [&] {
                Fs t("variable-div");
                verifyDivision(s, ca, commit(s.denominator, bb), cq, p, t);
            },
            "source commitment substitution");
        auto aa = a;
        aa[7] = Fr(1);
        rejects(
            [&] {
                Fs t("variable-div");
                proveDivision(s, commit(s.numerator, aa), cb, cq, aa, b, q, t);
            },
            "numerator nonzero padding");
        bb = b;
        bb[7] = Fr(1);
        rejects(
            [&] {
                Fs t("variable-div");
                proveDivision(s, ca, commit(s.denominator, bb), cq, a, bb, q, t);
            },
            "denominator nonzero padding");
        DivisionSpec one{"div/one",
                         PCSpec::make("one/A", 1, 1, Layout::ColumnMajor),
                         PCSpec::make("one/B", 1, 1, Layout::ColumnMajor),
                         PCSpec::make("one/q", 1, 1, Layout::ColumnMajor),
                         1,
                         1,
                         1};
        std::vector<Fr> z{Fr(-1)}, unit{Fr(1)};
        auto az = commit(one.numerator, z), bz = commit(one.denominator, unit),
             qz = commit(one.quotient, z);
        Fs a1("singleton-div"), v1("singleton-div");
        auto p1 = proveDivision(one, az, bz, qz, z, unit, z, a1);
        verifyDivision(one, az, bz, qz, p1, v1);
        check(a1.digest() == v1.digest(), "zero-round singleton with signed one-bit minimum");
        std::cout << "PASS variable division " << checks << " checks\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << "\n";
        return 1;
    }
}
