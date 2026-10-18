// Dedicated verifier: loads public Statement and Proof only, never any witness.
#include "zk_committed_linear_opening.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace cy=zkproto::committed;
namespace {
cy::Bytes readBounded(const char* path) {
    std::ifstream f(path,std::ios::binary|std::ios::ate);
    if(!f)throw std::runtime_error("public artifact open failed");
    auto n=f.tellg();if(n<0||n>4096)throw std::runtime_error("public artifact exceeds size bound");
    cy::Bytes bytes(static_cast<std::size_t>(n));f.seekg(0);
    if(n&&!f.read(reinterpret_cast<char*>(bytes.data()),n))throw std::runtime_error("public artifact short read");
    if(f.peek()!=std::char_traits<char>::eof())throw std::runtime_error("public artifact grew during read");
    return bytes;
}
}
int main(int argc,char** argv) {
    if(argc!=3&&argc!=4){std::cout<<"usage: zk_committed_opening_verify STATEMENT PROOF [EXPECTED_CONTEXT]\n";return 2;}
    try {
        mcl::bn::initPairing(mcl::BN254);
        cy::Statement s;cy::Proof p;
        try {
            s=cy::decodeStatement(readBounded(argv[1]));p=cy::decodeProof(readBounded(argv[2]));
            if(argc==4&&s.context!=argv[3])throw std::runtime_error("unexpected application context");
        }catch(const std::exception& e){std::cout<<"DECODE_REJECT "<<e.what()<<'\n';return 2;}
        std::string error;
        if(!cy::verify(cy::makeParameters(s.coefficients.size()),s,p,&error)){std::cout<<"CRYPTO_REJECT "<<error<<'\n';return 1;}
        std::cout<<"ACCEPT committed-y shared-preimage proof\n";return 0;
    }catch(const std::exception& e){std::cout<<"CRYPTO_REJECT "<<e.what()<<'\n';return 1;}
}
