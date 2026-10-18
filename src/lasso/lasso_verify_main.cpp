#include "lasso_serialization.hpp"
#include "lasso_verifier.hpp"
#include <chrono>
#include <iostream>
int main(int argc, char **argv) {
    using namespace route_b::lasso;
    if (argc != 3) {
        std::cerr << "usage: lasso_verify TRUSTED.instance proof.lasso\n";
        return 2;
    }
    try {
        mcl::bn::initPairing(mcl::BN254);
        auto start = std::chrono::steady_clock::now();
        auto x = decodeInstance(readFile(argv[1]));
        auto p = decodeProof(readFile(argv[2]));
        VerifiedLookupClaim claim;
        std::string e;
        if (!verify(x, p, &claim, &e)) {
            std::cerr << "REJECT " << e << '\n';
            return 1;
        }
        std::cout << "ACCEPT verified_lookup_claim m=" << x.query_length << " N=" << x.table.size()
                  << " row_dimensions=" << claim.row_point.size()
                  << " evaluation=" << claim.evaluation.getStr() << " verify_ms="
                  << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                               start)
                         .count()
                  << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "REJECT " << e.what() << '\n';
        return 1;
    }
}
