#include "range_logup.hpp"
#include "tests/zk_logup_public_fixture.hpp"
#include <fstream>
#include <iostream>
namespace {
zkgpt::zk::Bytes readPublic(const char* path,size_t limit){
    std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::invalid_argument("cannot open public artifact");
    auto n=f.tellg();if(n<0||static_cast<uint64_t>(n)>limit)throw std::invalid_argument("public file bound");
    f.seekg(0);zkgpt::zk::Bytes b(static_cast<size_t>(n));
    if(!b.empty()&&!f.read(reinterpret_cast<char*>(b.data()),b.size()))throw std::invalid_argument("public file read");return b;
}
}
int main(int argc,char** argv){
    try {
        if(argc!=4)throw std::invalid_argument("usage: zk_logup_verify statement proof expected-context");
        mcl::bn::initPairing(mcl::BN254);
        auto s=zkgpt::zk::decodeFieldLogUpZkStatement(readPublic(argv[1],64ULL<<20));
        zkgpt::zk::testing::checkExpectedFixture(s,argv[3]);
        auto p=zkgpt::zk::decodeFieldLogUpZkProof(s,readPublic(argv[2],32ULL<<20));std::string error;
        if(!zkgpt::zk::verifyFieldLogUpZk(s,p,&error)){std::cout<<"CRYPTO_REJECT "<<error<<'\n';return 1;}
        std::cout<<"ACCEPT FieldLogUp hidden-v1 public fixture\n";return 0;
    }catch(const std::exception& e){std::cout<<"DECODE_CONTEXT_REJECT "<<e.what()<<'\n';return 2;}
}
