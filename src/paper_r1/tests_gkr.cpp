#include "gkr.hpp"
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
            std::vector<Fr> x(n);
            for (std::size_t i = 0; i < n; ++i)
                x[i] = Fr(i + 2);
            GkrSpec s;
            s.input_size = n;
            s.context = "arithmetic-gkr-unit";
            for (unsigned j = 0; j < 3; ++j) {
                std::vector<Gate> layer;
                for (std::size_t i = 0; i < n; ++i)
                    layer.push_back(
                        {i % 2 ? GateKind::Multiply : GateKind::Add, i, (i + 1) % n, Fr(j + 1)});
                s.layers.push_back(layer);
            }
            s.output = evaluate(s, x).back();
            Fs pt("gkr"), vt("gkr");
            auto p = proveGkr(s, x, pt);
            auto claims = verifyGkr(s, p, vt);
            check(pt.digest() == vt.digest(), "GKR transcript");
            for (auto &q : claims)
                check(q.value == mle(x, q.point), "GKR input endpoint");
            auto bad = p;
            bad.layers.back().left += Fr(1);
            rejects(
                [&] {
                    Fs t("gkr");
                    verifyGkr(s, bad, t);
                },
                "GKR endpoint tamper");
            auto ss = s;
            ss.output[0] += Fr(1);
            rejects(
                [&] {
                    Fs t("gkr");
                    verifyGkr(ss, p, t);
                },
                "GKR public Y tamper");
            ss = s;
            ss.layers[0][0].coefficient += Fr(1);
            rejects(
                [&] {
                    Fs t("gkr");
                    verifyGkr(ss, p, t);
                },
                "GKR circuit tamper");
            if (d) {
                bad = p;
                bad.layers[0].sumcheck.rounds[0][0] += Fr(1);
                rejects(
                    [&] {
                        Fs t("gkr");
                        verifyGkr(s, bad, t);
                    },
                    "GKR round tamper");
            }
        }
        // GKR endpoints are discharged through four-block input sumcheck and all PC openings.
        InputSpec in;
        in.context = "bound-circuit-model-public-X-Y";
        in.x = {Fr(2), Fr(3)};
        std::array<std::vector<Fr>, 3> w{{{Fr(4), Fr(5)}, {Fr(6), Fr(7)}, {Fr(8), Fr(9)}}};
        std::vector<Fr> input = in.x;
        for (unsigned b = 0; b < 3; ++b) {
            in.spec[b] = PCSpec::make("gkr-input/" + std::to_string(b), 2, 2, Layout::ColumnMajor);
            in.commitment[b] = commit(in.spec[b], w[b]);
            input.insert(input.end(), w[b].begin(), w[b].end());
        }
        GkrSpec s;
        s.input_size = 8;
        s.context = in.context;
        s.layers = {{{GateKind::Multiply, 0, 2, Fr(1)}, {GateKind::Add, 1, 3, Fr(1)}}};
        s.output = evaluate(s, input).back();
        Fs pt("combined"), vt("combined");
        appendInputStatement(in, pt);
        auto gp = proveGkr(s, input, pt);
        rejects([&] { verifyGkr(s, gp, vt); }, "missing initial input commitment");
        auto layers = evaluate(s, input);
        (void)layers;
        Fs p2("combined"), v2("combined");
        appendInputStatement(in, p2);
        appendInputStatement(in, v2);
        auto g2 = proveGkr(s, input, p2);
        auto terminal = verifyGkr(s, g2, v2);
        std::vector<InputClaim> honest;
        for (auto &q : terminal)
            honest.push_back({q.point, mle(input, q.point)});
        auto ip = proveInput(in, w, honest, p2);
        verifyInput(in, terminal, ip, v2);
        check(p2.digest() == v2.digest(), "GKR plus final input PC");
        std::cout << "PASS gkr " << checks << " checks\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << "\n";
        return 1;
    }
}
