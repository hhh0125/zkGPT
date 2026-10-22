#include "wire.hpp"
#include <iostream>
using namespace zkgpt::r1;
int main(int argc,char **argv){try{
 init();require(argc==3,"usage: r2_lookup_verify APPROVED_STATEMENT PROOF");
 auto s=parseLookupSpec(readFile(argv[1]));auto p=parseLookupProof(s,readFile(argv[2]));
 Fs t("lookup");verifyLookupBatch(s,p,t);std::cout<<"ACCEPT Range/Exp/input consistency\n";return 0;
}catch(const std::exception&e){std::cerr<<"REJECT "<<e.what()<<'\n';return 2;}}
