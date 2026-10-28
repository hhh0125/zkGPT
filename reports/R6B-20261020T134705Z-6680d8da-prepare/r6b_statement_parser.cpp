// Bounded decoding adapter; no builder, witness, verifyR2 or proof acceptance.
#include "wire.hpp"
#include "r5b_trust.hpp"
#include <filesystem>
#include <iostream>
using namespace zkgpt;
int main(int argc,char**argv){try{
 r1::require(argc>=3,"usage: --decode STATEMENT [PROOF] | --admit STATEMENT ANCHOR [PROOF]");
 const bool admit=std::string(argv[1])=="--admit";
 r1::require(admit || std::string(argv[1])=="--decode","decoder mode");
 r1::require(argc==(admit?4:3) || argc==(admit?5:4),"decoder args");
 r1::require(std::filesystem::file_size(argv[2])<=16*1024*1024,"statement cap");
 r1::init();auto b=r1::readFile(argv[2]);auto [g,s]=r2::parseStatement(b);
 r1::require(s.input.public_values.size()<=80000 && s.input.children.size()<=32,"bounded decoded metadata");
 r1::require(r2::serializeStatement(g,s)==b,"statement canonical roundtrip");
 if(admit){r1::require(std::filesystem::file_size(argv[3])<=2*1024*1024,"anchor cap");
  auto a=r5b::decodeAnchor(r1::readFile(argv[3]),"ZKGPT-R6-OPERATOR-ANCHOR-v1",2*1024*1024);
  r1::require(!s.input.children.empty(),"no W");r5b::requireAnchorW(a,s.input.children.back().commitment);}
 const bool proof=argc==(admit?5:4);if(proof){auto file=argv[admit?4:3];r1::require(std::filesystem::file_size(file)<=64*1024*1024,"proof cap");
  auto pb=r1::readFile(file);auto p=r2::parseProof(g,s,pb);r1::require(r2::serializeProof(g,s,p)==pb,"proof canonical roundtrip");}
 std::cout<<"{\"decoded\":true,\"proof_decoded\":"<<(proof?"true":"false")<<",\"anchor_W_equal\":"<<(admit?"true":"false")<<",\"cryptographic_verified\":false}\n";return 0;
 }catch(const std::exception&e){std::cerr<<"DECODE_REJECT "<<e.what()<<'\n';return 1;}}
