#include "crypto.hpp"
#include <iostream>
using namespace zkgpt::r7::crypto;
int main(){try{
 r1::init();OSRandom rng;auto pp=parameters(32);validate(pp);
 auto check=[](bool ok,const char*why){need(ok,why);std::cout<<"PASS "<<why<<'\n';};
 auto rejects=[&](auto f,const char*why){bool rejected=false;try{f();}catch(const std::exception&e){rejected=true;std::cout<<"REJECT_REASON "<<why<<": "<<e.what()<<'\n';}check(rejected,why);};
 for(auto n:{16u,64u,256u,1024u}){auto spec=PCSpec::make("unit/W",n,n);std::vector<Fr>w(n,Fr(7));auto a=commit(spec,w,rng),b=commit(spec,w,rng);check(a.public_pc.rows!=b.public_pc.rows,"fresh independent row blinds");check(pcIdentity(a.public_pc)!=pcIdentity(b.public_pc),"PC identity includes commitment bytes");validate(a.public_pc);}
 rejects([&]{auto s=PCSpec::make("unit/W",3,4);commit(s,std::vector<Fr>(2,Fr(0)),rng);},"wrong witness length");
 rejects([&]{auto p=pp;p.suite="old-R6-PC";validate(p);},"wrong suite");
 rejects([&]{auto p=pp;p.columns[0]=p.scalar;validate(p);},"wrong generator");
 rejects([&]{decodePoint(Bytes(32,255));},"noncanonical point");
 struct Failed:Random{void fill(std::uint8_t*,std::size_t)override{throw std::runtime_error("entropy source failed");}}failed;
 rejects([&]{failed.scalar();},"random source failure");
 auto zero=commit(PCSpec::make("unit/zero",16,16),std::vector<Fr>(16,Fr(0)),rng);validate(zero.public_pc);check(zero.values.size()==16,"zero vector valid");
 auto encoded=encode(zero.public_pc);check(encode(decodePC(encoded))==encoded,"canonical PC roundtrip");encoded.push_back(0);rejects([&]{decodePC(encoded);},"trailing PC bytes");
 std::cout<<"LOCAL_CRYPTO_TESTED HidingPC unit arithmetic only; opening/session tests required separately\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
