#include "memory.hpp"
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
        for (std::size_t n : {1, 2, 8, 512}) {
            MemorySpec s{"memory/test",
                         PCSpec::make("address", n, n, Layout::ColumnMajor),
                         PCSpec::make("value", n, n, Layout::RowMajor),
                         {Fr(7), Fr(7), Fr(11), Fr(0)}};
            std::vector<Fr> a(n), v(n);
            for (std::size_t i = 0; i < n; ++i) {
                a[i] = Fr(i % 4);
                v[i] = s.table[i % 4];
            }
            auto ca = commit(s.address, a), cv = commit(s.value, v);
            Fs pt("memory"), vt("memory");
            auto p = proveMemory(s, ca, cv, a, v, pt);
            verifyMemory(s, ca, cv, p, vt);
            check(pt.digest() == vt.digest(), "memory transcript");
            auto bad = p;
            bad.roots[0] += Fr(1);
            rejects(
                [&] {
                    Fs t("memory");
                    verifyMemory(s, ca, cv, bad, t);
                },
                "root tamper");
            bad = p;
            bad.products[0].leaf[0].value += Fr(1);
            rejects(
                [&] {
                    Fs t("memory");
                    verifyMemory(s, ca, cv, bad, t);
                },
                "leaf address tamper");
            bad = p;
            bad.products[1].leaf[1].value += Fr(1);
            rejects(
                [&] {
                    Fs t("memory");
                    verifyMemory(s, ca, cv, bad, t);
                },
                "leaf value tamper");
            auto ss = s;
            ss.table[0] += Fr(1);
            rejects(
                [&] {
                    Fs t("memory");
                    verifyMemory(ss, ca, cv, p, t);
                },
                "public table tamper");
            auto vv = v;
            vv[0] += Fr(1);
            rejects(
                [&] {
                    Fs t("memory");
                    proveMemory(s, ca, cv, a, vv, t);
                },
                "bad read value");
            auto aa = a;
            aa[0] = Fr(4);
            rejects(
                [&] {
                    Fs t("memory");
                    proveMemory(s, ca, cv, aa, v, t);
                },
                "out of table address");
            if (n > 1) {
                bad = p;
                bad.products[0].tree.layers.back().left += Fr(1);
                rejects(
                    [&] {
                        Fs t("memory");
                        verifyMemory(s, ca, cv, bad, t);
                    },
                    "tree splice");
            }
        }
        // The product argument must handle zero roots without inversion.
        std::vector<Fr> leaves{Fr(0), Fr(2), Fr(3), Fr(0)};
        Fs a("zero-product"), b("zero-product");
        ProductProof p;
        auto end = proveProduct(leaves, p, a, "p");
        auto ve = verifyProduct(4, Fr(0), p, b, "p");
        check(ve.value == mle(leaves, ve.point) && end.value == ve.value, "zero product");
        std::cout << "PASS memory " << checks << " checks\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << "\n";
        return 1;
    }
}
