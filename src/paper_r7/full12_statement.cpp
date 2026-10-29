#include "oneblock_plan.hpp"
#include "wire.hpp"
#include "paper_r1/wire.hpp"
#include "paper_r2/r5b_trust.hpp"
#include <filesystem>
#include <cstring>
#include <iostream>
int main(int argc,char**argv){try{using namespace zkgpt::r7;zkgpt::r1::init();crypto::need(argc==2 && !std::filesystem::exists(argv[1]),"usage: full12_statement NEW_PUBLIC_STATEMENT");auto c=master3::trustedContext(12);auto read=[](const char*name){auto b=zkgpt::r1::readFile(std::string("reports/R6-FULL12-P0AB-20261021/D-EXECUTION-20261022/")+name);crypto::need(b.size()==262144,"Full12 public X/Y padded shape");std::vector<Fr>v;v.reserve(32768);for(std::size_t i=0;i<32768;++i){std::uint64_t bits=0;for(unsigned j=0;j<8;++j)bits|=std::uint64_t(b[8*i+j])<<(8*j);std::int64_t value;std::memcpy(&value,&bits,8);v.emplace_back(value);}return v;};auto s=makeStatement(c,read("FULL12_PUBLIC_X_SIGNED64_LE.bin"),read("FULL12_FINAL_BLOCK_Y_SIGNED64_LE.bin"));crypto::need(validateStatement(s,c).code==Code::Ok,"Full12 public statement native binding");auto b=R7Wire::encodeStatement(s);zkgpt::r1::writeFile(argv[1],b);std::cout<<"FULL12_PUBLIC_STATEMENT_CREATED sha256="<<zkgpt::r5b::digestHex(b)<<" bytes="<<b.size()<<'\n';return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 2;}}
