#include "zkgpt_serialization.hpp"

#include "global_var.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {

std::vector<std::uint8_t> readFile(const char *path) {
    std::ifstream input(path,std::ios::binary);
    if (!input) throw std::runtime_error(std::string("cannot open ")+path);
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}

} // namespace

int main(int argc,char **argv) {
    if (argc!=3) {
        std::cerr << "usage: zkgpt_artifact_verify STATEMENT PROOF\n";
        return 2;
    }
    try {
        initPairing(mcl::BN254);
        const auto statement_bytes=readFile(argv[1]);
        const auto proof_bytes=readFile(argv[2]);
        ZkGPTPublicStatement statement;
        ZkGPTProof proof;
        std::string error;
        if (!deserializeZkGPTPublicStatement(
                statement_bytes,statement,&error))
            throw std::runtime_error("statement decode failed: "+error);
        if (!deserializeZkGPTProof(proof_bytes,proof,&error))
            throw std::runtime_error("proof decode failed: "+error);
        if (!verifyZkGPT(statement,proof,&error))
            throw std::runtime_error(error);
        std::cout << "zkGPT artifact verified: statement_bytes="
                  << statement_bytes.size() << ", proof_bytes="
                  << proof_bytes.size() << '\n';
        return 0;
    } catch (const std::exception &exception) {
        std::cerr << "zkGPT artifact rejected: " << exception.what() << '\n';
        return 1;
    }
}

