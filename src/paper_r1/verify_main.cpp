#include "wire.hpp"
#include <iostream>
int main(int argc, char **argv) {
    try {
        using namespace zkgpt::r1;
        const std::string flag = argc == 4 ? argv[1] : "";
        require(argc == 3 || (argc == 4 && (flag == "--exp" || flag == "--division" ||
                                            flag == "--positive-sqrt")),
                "usage: r1_verify [--exp|--division|--positive-sqrt] verifier-selected.statement "
                "untrusted.proof");
        init();
        if (flag == "--division") {
            auto s = parseDivisionStatement(readFile(argv[2]));
            auto p = parseDivisionProof(s, readFile(argv[3]));
            Fs t("zkGPT.paper-r1.division.artifact.v1");
            verifyDivision(s.spec, s.numerator, s.denominator, s.quotient, p, t);
            std::cout << "ACCEPT committed positive division relation (not full zkGPT)\n";
            return 0;
        }
        if (flag == "--positive-sqrt") {
            auto s = parsePositiveSqrtStatement(readFile(argv[2]));
            auto p = parsePositiveSqrtProof(s, readFile(argv[3]));
            Fs t("zkGPT.paper-r1.positive-sqrt.artifact.v1");
            verifyPositiveSqrt(s.spec, s.input, s.root, p, t);
            std::cout << "ACCEPT positive integer sqrt relation (not full zkGPT)\n";
            return 0;
        }
        if (flag == "--exp") {
            auto s = parseExpStatement(readFile(argv[2]));
            auto p = parseExpProof(s, readFile(argv[3]));
            Fs t("zkGPT.paper-r1.exp.artifact.v1");
            verifyExp(s.spec, s.index, s.value, p, t);
            std::cout << "ACCEPT exact unclamped Exp relation (not full zkGPT)\n";
            return 0;
        }
        auto s = parseStatement(readFile(argv[1]));
        auto p = parseProof(s, readFile(argv[2]));
        verifyBundle(s, p);
        std::cout << "ACCEPT foundation arithmetic/lookup relation (not full zkGPT)\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "REJECT " << e.what() << "\n";
        return 2;
    }
}
