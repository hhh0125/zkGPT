#include "r6_cli.hpp"
#include <iostream>
int main(int argc,char**argv){try{zkgpt::r1::require(argc>=4,"usage: r6_model_plan NEW_REPORT --blocks1|12");auto o=zkgpt::r6::options(argc,argv,2);zkgpt::r1::init();zkgpt::r6::plan(argv[1],o);std::cout<<"PASS R6 actual builder metadata,zero field witness,no proof\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
