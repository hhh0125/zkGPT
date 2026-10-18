#include "range_serialization.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> readArtifact(const std::string &path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("cannot open artifact: "+path);
    const std::streamoff end=input.tellg();
    if (end<0 || static_cast<std::uint64_t>(end)>(1ULL<<30))
        throw std::runtime_error("artifact size is outside protocol limits: "+path);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    input.seekg(0);
    if (!bytes.empty())
        input.read(reinterpret_cast<char *>(bytes.data()), bytes.size());
    if (!input) throw std::runtime_error("failed to read artifact: "+path);
    return bytes;
}

}  // namespace

int main(int argc, char **argv) {
    if (argc!=3) {
        std::cerr << "usage: range_artifact_verify STATEMENT PROOF\n";
        return 2;
    }
    try {
        initPairing(mcl::BN254);
        const auto statement_bytes=readArtifact(argv[1]);
        const auto proof_bytes=readArtifact(argv[2]);
        RangePublicStatement statement;
        RangeProof proof;
        std::string error;
        if (!deserializeRangePublicStatement(
                statement_bytes, statement, &error)) {
            std::cerr << "statement deserialization failed: " << error << '\n';
            return 1;
        }
        if (!deserializeRangeProof(proof_bytes, proof, &error)) {
            std::cerr << "proof deserialization failed: " << error << '\n';
            return 1;
        }
        range_verifier verifier;
        if (!verifier.verify(statement, proof, &error)) {
            std::cerr << "Range artifact verification failed: " << error << '\n';
            return 1;
        }
        std::cout << "Range artifact verified: statement_bytes="
                  << statement_bytes.size() << ", proof_bytes="
                  << proof_bytes.size() << '\n';
        return 0;
    } catch (const std::exception &exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}

