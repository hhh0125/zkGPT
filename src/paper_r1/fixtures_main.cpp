#include "wire.hpp"
#include "division.hpp"
#include "positive_sqrt.hpp"
#include <filesystem>
#include <iostream>
using namespace zkgpt::r1;
int main(int argc, char **argv) {
    try {
        init();
        require(argc == 2, "fixture output directory required");
        std::filesystem::path dir(argv[1]);
        std::filesystem::create_directories(dir);
        BundleStatement s;
        s.input.context = "FOUNDATION-FIXTURE-NOT-GPT2-v1";
        s.input.x.resize(8);
        std::array<std::vector<Fr>, 3> w;
        for (auto &v : w)
            v.resize(8);
        std::vector<Fr> table{Fr(0), Fr(3), Fr(5), Fr(7)};
        for (unsigned i = 0; i < 8; ++i) {
            s.input.x[i] = Fr(i + 2);
            w[0][i] = Fr(i + 3);
            w[1][i] = Fr(i < 5 ? i % 4 : 0);
            w[2][i] = table[i < 5 ? i % 4 : 0];
        }
        for (unsigned b = 0; b < 3; ++b) {
            s.input.spec[b] = PCSpec::make("fixture/block/" + std::to_string(b), b ? 5 : 8, 8,
                                           Layout::ColumnMajor);
            s.input.commitment[b] = commit(s.input.spec[b], w[b]);
        }
        s.range = {"fixture/range", s.input.spec[2], 3};
        s.indexed = {"fixture/indexed", s.input.spec[1], s.input.spec[2], table};
        s.gkr.context = s.input.context;
        s.gkr.input_size = 32;
        std::vector<Gate> first, second;
        for (unsigned i = 0; i < 8; ++i)
            first.push_back({GateKind::Multiply, i, 8 + i, Fr(1)});
        for (unsigned i = 0; i < 8; ++i)
            first.push_back({GateKind::Multiply, 16 + i, 24 + i, Fr(1)});
        for (unsigned i = 0; i < 8; ++i)
            second.push_back({GateKind::Add, i, 8 + i, Fr(1)});
        s.gkr.layers = {first, second};
        std::vector<Fr> input = s.input.x;
        for (auto &v : w)
            input.insert(input.end(), v.begin(), v.end());
        s.gkr.output = evaluate(s.gkr, input).back();
        auto p = proveBundle(s, w);
        verifyBundle(s, p);
        auto save = [&](const std::string &name, const BundleStatement &ss, const BundleProof &pp) {
            writeFile((dir / (name + ".statement")).string(), serialize(ss));
            writeFile((dir / (name + ".proof")).string(), serialize(ss, pp));
        };
        save("valid", s, p);
        auto bad = p;
        bad.range.query.value += Fr(1);
        save("query", s, bad);
        bad = p;
        bad.memory.roots[1] += Fr(1);
        save("memory-root", s, bad);
        bad = p;
        bad.memory.products[0].leaf[2].value += Fr(1);
        save("read-counter", s, bad);
        bad = p;
        bad.gkr.layers[0].left += Fr(1);
        save("gkr", s, bad);
        bad = p;
        bad.input.opening[2].value += Fr(1);
        save("final-L", s, bad);
        bad = p;
        bad.indexed_value.value += Fr(1);
        save("indexed-value", s, bad);
        bad = p;
        bad.input.selectors.rounds[0][0] += Fr(1);
        save("transcript", s, bad);
        auto ss = s;
        ss.input.x[0] += Fr(1);
        save("public-X", ss, p);
        ss = s;
        ss.gkr.output[0] += Fr(1);
        save("public-Y", ss, p);
        ss = s;
        ss.input.commitment[0].rows[0] += generators(ss.input.spec[0]).u;
        save("model-commitment", ss, p);
        ss = s;
        ss.indexed.table[0] += Fr(1);
        save("table", ss, p);
        ss = s;
        auto other_w = w;
        other_w[0][0] += Fr(1);
        ss.input.commitment[0] = commit(ss.input.spec[0], other_w[0]);
        std::vector<Fr> other_input = ss.input.x;
        for (auto &values : other_w)
            other_input.insert(other_input.end(), values.begin(), values.end());
        ss.gkr.output = evaluate(ss.gkr, other_input).back();
        auto other_proof = proveBundle(ss, other_w);
        verifyBundle(ss, other_proof);
        save("other-model", ss, other_proof);
        save("model-substitution", s, other_proof);
        auto malformed = serialize(s, p);
        malformed.push_back(0);
        writeFile((dir / "trailing.proof").string(), malformed);
        malformed = serialize(s, p);
        malformed.resize(malformed.size() - 1);
        writeFile((dir / "truncated.proof").string(), malformed);
        // The final scalar of InputProof is an Fr; all-ones is never canonical.
        malformed = serialize(s, p);
        std::fill(malformed.end() - 32, malformed.end(), 255);
        writeFile((dir / "noncanonical-Fr.proof").string(), malformed);
        auto b = serialize(s);
        for (unsigned j = 31; j < 39; ++j)
            b[j] = 255;
        writeFile((dir / "oversized.statement").string(), b);
        ExpStatement es{{"fixture/exp", PCSpec::make("fixture/exp-t", 6, 8, Layout::ColumnMajor),
                         PCSpec::make("fixture/exp-E", 6, 8, Layout::RowMajor)},
                        {},
                        {}};
        std::array<std::uint32_t, 6> ti{0, 1, 7452, 8191, 8192, 0xffffffff};
        std::vector<Fr> tx(8, Fr(0)), ex(8, Fr(0));
        for (unsigned i = 0; i < 6; ++i) {
            tx[i] = Fr(ti[i]);
            ex[i] = Fr(expValue(ti[i]));
        }
        es.index = commit(es.spec.index, tx);
        es.value = commit(es.spec.value, ex);
        Fs et("zkGPT.paper-r1.exp.artifact.v1");
        auto ep = proveExp(es.spec, es.index, es.value, tx, ex, et);
        writeFile((dir / "exp-valid.statement").string(), serializeExp(es));
        writeFile((dir / "exp-valid.proof").string(), serializeExp(es, ep));
        auto ebad = ep;
        ebad.constraints[0].rounds[0][0] += Fr(1);
        writeFile((dir / "exp-branch.proof").string(), serializeExp(es, ebad));
        ebad = ep;
        ebad.memory.roots[0] += Fr(1);
        writeFile((dir / "exp-memory.proof").string(), serializeExp(es, ebad));
        DivisionStatement ds{{"fixture/div",
                              PCSpec::make("fixture/div-A", 3, 4, Layout::ColumnMajor),
                              PCSpec::make("fixture/div-B", 3, 4, Layout::RowMajor),
                              PCSpec::make("fixture/div-q", 3, 4, Layout::ColumnMajor), 8, 8, 4},
                             {},
                             {},
                             {}};
        std::vector<Fr> da{Fr(1), Fr(-1), Fr(7), Fr(0)}, db{Fr(2), Fr(2), Fr(3), Fr(0)},
            dq{Fr(1), Fr(0), Fr(2), Fr(0)};
        ds.numerator = commit(ds.spec.numerator, da);
        ds.denominator = commit(ds.spec.denominator, db);
        ds.quotient = commit(ds.spec.quotient, dq);
        Fs dt("zkGPT.paper-r1.division.artifact.v1");
        auto dp = proveDivision(ds.spec, ds.numerator, ds.denominator, ds.quotient, da, db, dq, dt);
        writeFile((dir / "div-valid.statement").string(), serializeDivision(ds));
        writeFile((dir / "div-valid.proof").string(), serializeDivision(ds, dp));
        auto dbad = dp;
        dbad.constraints[0].rounds[0][0] += Fr(1);
        writeFile((dir / "div-relation.proof").string(), serializeDivision(ds, dbad));
        dbad = dp;
        dbad.positive_binding[0].value += Fr(1);
        writeFile((dir / "div-positivity.proof").string(), serializeDivision(ds, dbad));
        auto dsource = ds;
        dsource.denominator.rows[0] += generators(ds.spec.denominator).u;
        writeFile((dir / "div-source.statement").string(), serializeDivision(dsource));
        auto draw = serializeDivision(ds, dp);
        draw.push_back(0);
        writeFile((dir / "div-trailing.proof").string(), draw);
        PositiveSqrtStatement ps{{"fixture/sqrt",
                                  PCSpec::make("fixture/sqrt-x", 3, 4, Layout::RowMajor),
                                  PCSpec::make("fixture/sqrt-y", 3, 4, Layout::ColumnMajor), 8, 4},
                                 {},
                                 {}};
        std::vector<Fr> sx{Fr(2), Fr(3), Fr(7), Fr(0)}, sy{Fr(1), Fr(2), Fr(3), Fr(0)};
        ps.input = commit(ps.spec.input, sx);
        ps.root = commit(ps.spec.root, sy);
        Fs st("zkGPT.paper-r1.positive-sqrt.artifact.v1");
        auto sp = provePositiveSqrt(ps.spec, ps.input, ps.root, sx, sy, st);
        writeFile((dir / "sqrt-valid.statement").string(), serializePositiveSqrt(ps));
        writeFile((dir / "sqrt-valid.proof").string(), serializePositiveSqrt(ps, sp));
        auto sbad = sp;
        sbad.constraints[1].rounds[0][0] += Fr(1);
        writeFile((dir / "sqrt-relation.proof").string(), serializePositiveSqrt(ps, sbad));
        sbad = sp;
        sbad.positive_binding[0].value += Fr(1);
        writeFile((dir / "sqrt-positivity.proof").string(), serializePositiveSqrt(ps, sbad));
        auto ssource = ps;
        ssource.root.rows[0] += generators(ps.spec.root).u;
        writeFile((dir / "sqrt-source.statement").string(), serializePositiveSqrt(ssource));
        auto sraw = serializePositiveSqrt(ps, sp);
        sraw.pop_back();
        writeFile((dir / "sqrt-truncated.proof").string(), sraw);
        std::cout << "PASS wrote foundation-only fixtures to " << dir << "\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << "\n";
        return 1;
    }
}
