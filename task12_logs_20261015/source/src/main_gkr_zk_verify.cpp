#include "gkr_serialization.hpp"
#include "gkr_verifier.hpp"
#include "tests/gkr_public_fixture.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
zkgpt::zk::Bytes readPublic(const char* path,std::size_t max_size) {
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if(!input)throw std::invalid_argument("cannot open public artifact");
    const auto size=input.tellg();
    if(size<0 || static_cast<std::uint64_t>(size)>max_size)
        throw std::invalid_argument("public artifact file size limit");
    input.seekg(0);zkgpt::zk::Bytes bytes(static_cast<std::size_t>(size));
    if(!bytes.empty()&&!input.read(reinterpret_cast<char*>(bytes.data()),bytes.size()))
        throw std::invalid_argument("cannot read public artifact");
    return bytes;
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=4)throw std::invalid_argument("usage: gkr_zk_verify statement proof expected-context");
        mcl::bn::initPairing(mcl::BN254);
        layeredCircuit circuit;gkr_test::buildSmallFconnPublicCircuit(circuit);
        GKRZkPublicStatement statement;GKRZkProof proof;std::string error;
        if(!deserializeGKRZkStatement(readPublic(argv[1],2ULL<<20),statement,&error))
            throw std::invalid_argument(error);
        if(statement.context!=argv[3])throw std::invalid_argument("unexpected application context");
        if(!deserializeGKRZkProof(readPublic(argv[2],128ULL<<20),circuit,proof,&error))
            throw std::invalid_argument(error);
        if(!verifyGKRZk(circuit,statement,proof,&error)) {
            std::cout<<"CRYPTO_REJECT "<<error<<'\n';return 1;
        }
        std::cout<<"ACCEPT GKR masked v1 public fixture\n";return 0;
    }catch(const std::exception& e) {std::cout<<"DECODE_CONTEXT_REJECT "<<e.what()<<'\n';return 2;}
}
