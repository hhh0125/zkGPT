#include "full12_model.hpp"
#include "master_layout.hpp"
#include "paper_r1/wire.hpp"
#include <iostream>
int main(int argc,char**argv){try{if(argc!=2)return 3;zkgpt::r1::init();auto c=zkgpt::r7::master3::trustedContext(12);auto t=zkgpt::r7::full12::loadTrust(c,argv[1]);std::cout<<"TRUST_ACCEPT chunks="<<t.chunks.size()<<"\n";return 0;}catch(const std::exception&e){std::cerr<<"TRUST_REJECT "<<e.what()<<"\n";return 2;}}
