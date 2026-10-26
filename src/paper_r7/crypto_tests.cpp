#include "crypto.hpp"
#include "r6_adapter.hpp"
#include "wire.hpp"
#include <iostream>
using namespace zkgpt::r7::crypto;
int main(){try{
 r1::init();OSRandom rng;auto pp=parameters(32);validate(pp);
 std::cout<<"MASTER_STATIC_PLAN {\"N\":16777216,\"rows\":4096,\"columns\":4096,\"generators\":4098,\"generator_bytes\":"<<4098*sizeof(G1)<<",\"commitment_points\":4096,\"serialized_commitment_bytes\":131072,\"witness_bytes\":"<<16777216*sizeof(Fr)<<",\"row_blind_bytes\":"<<4096*sizeof(Fr)<<",\"row_msm_count\":4096,\"max_row_msm_bases\":4097,\"opening_cores_at_K4\":5,\"IPA_rounds_per_core\":12,\"full_master_operation_enabled\":false}\n";
 auto check=[](bool ok,const char*why){need(ok,why);std::cout<<"PASS "<<why<<'\n';};
 auto rejects=[&](auto f,const char*why){bool rejected=false;try{f();}catch(const std::exception&e){rejected=true;std::cout<<"REJECT_REASON "<<why<<": "<<e.what()<<'\n';}check(rejected,why);};
 for(auto n:{16u,64u,256u,1024u}){auto spec=PCSpec::make("unit/W",n,n);std::vector<Fr>w(n,Fr(7));auto a=commit(spec,w,rng),b=commit(spec,w,rng);check(a.public_pc.rows!=b.public_pc.rows,"fresh independent row blinds");check(pcIdentity(a.public_pc)!=pcIdentity(b.public_pc),"PC identity includes commitment bytes");validate(a.public_pc);}
 rejects([&]{auto s=PCSpec::make("unit/W",3,4);commit(s,std::vector<Fr>(2,Fr(0)),rng);},"wrong witness length");
 rejects([&]{auto p=pp;p.suite="old-R6-PC";validate(p);},"wrong suite");
 rejects([&]{auto p=pp;p.columns[0]=p.scalar;validate(p);},"wrong generator");
 rejects([&]{auto p=PCSpec::make("unit/W",16,16);p.layout="row-major";validate(p);},"wrong layout");
 rejects([&]{auto p=PCSpec::make("unit/W",16,16);p.rows=8;validate(p);},"wrong rows/columns");
 rejects([&]{PCSpec::make("Master-disabled",16642424,16777216);},"full Master cryptography remains resource gated");
 rejects([&]{decodePoint(Bytes(32,255));},"noncanonical point");
 struct Failed:Random{void fill(std::uint8_t*,std::size_t)override{throw std::runtime_error("entropy source failed");}}failed;
 rejects([&]{failed.scalar();},"random source failure");
 auto zero=commit(PCSpec::make("unit/zero",16,16),std::vector<Fr>(16,Fr(0)),rng);validate(zero.public_pc);check(zero.values.size()==16,"zero vector valid");
 auto encoded=encode(zero.public_pc);check(encode(decodePC(encoded))==encoded,"canonical PC roundtrip");encoded.push_back(0);rejects([&]{decodePC(encoded);},"trailing PC bytes");
 auto hidden=hide(std::string(64,'a'),"batch-a/secret",Fr(0),rng);
 rejects([&]{validate(hidden.handle,std::string(64,'b'),"batch-a/secret");},"cross-session hidden handle");
 rejects([&]{validate(hidden.handle,std::string(64,'a'),"batch-b/secret");},"cross-batch hidden handle");
 auto context=zkgpt::r7::buildR6Context(std::string(64,'a'));
 auto statement=zkgpt::r7::makeStatement(context,std::vector<Fr>(32768,Fr(0)),std::vector<Fr>(32768,Fr(0)));
 zkgpt::r7::R7Proof offline;offline.statement_digest=r1::hash(zkgpt::r7::R7Wire::encodeStatement(statement));
 const std::string receipt="{\"challenge\":1,\"receipt\":\"prover-claims-valid\"}";
 for(auto&part:offline.parts)part=Bytes(receipt.begin(),receipt.end());
 auto result=zkgpt::r7::R7OneblockVerifier{}.verify(statement,offline,context);
 check(result.code==zkgpt::r7::Code::Unimplemented,"offline envelope cannot import prover challenge/receipt");
 std::cout<<"REJECT_REASON offline receipt: "<<zkgpt::r7::name(result.code)<<" "<<result.detail<<'\n';
 std::cout<<"LOCAL_CRYPTO_TESTED HidingPC unit arithmetic only; opening/session tests required separately\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
