#include "wire.hpp"
#include "fixture_public.hpp"
#include <iostream>
int main(int argc,char **argv){try{
 zkgpt::r1::init();zkgpt::r1::require(argc==4 && std::string(argv[1])=="--fixture",
    "usage: r2_verify --fixture APPROVED_STATEMENT PROOF; real-model policy is not yet approved");
 auto s=zkgpt::r2::parseStatement(zkgpt::r1::readFile(argv[2]));
 const auto c=r2FixtureCircuit();auto p=zkgpt::r2::parseProof(s.first,s.second,zkgpt::r1::readFile(argv[3]));
 auto claims=zkgpt::r2::verifyR2(c,s.first,s.second,p);
 std::cout<<"ACCEPT R2 fixture GKR+Lasso+consistency terminals="<<claims.size()<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"REJECT "<<e.what()<<'\n';return 2;}}
