// Measurement-only Memory fixture: real Product and Sigma arithmetic, synthetic
// vector oracles. It does not replace independent cryptographic verification.
#include "oneblock_lookup.hpp"
#include "perf5_profile.hpp"
#include "opt2_metrics.hpp"
#include <chrono>
#include <iostream>
#include <memory>
namespace zkgpt::r7::leaf_reference {using namespace zkgpt::r7::oneblock;}
#define oneblock leaf_reference
#define hiddenProduct referenceProduct
#define hiddenMemory referenceMemory
#define hiddenRange referenceRange
#define hiddenRangeBinding referenceRangeBinding
#define hiddenExp referenceExp
#include "product_leaf_reference.inc"
#undef oneblock
#undef hiddenProduct
#undef hiddenMemory
#undef hiddenRange
#undef hiddenRangeBinding
#undef hiddenExp
using namespace zkgpt::r7::crypto;
using namespace zkgpt::r7::oneblock;
namespace {
struct FixedRandom:Random {std::uint64_t counter=0;void fill(std::uint8_t*out,std::size_t n)override{while(n){Writer w;w.text("leaf-probe-test-only");w.number(counter++,8);auto d=r1::hash(w.b);auto take=std::min(n,d.size());std::copy_n(d.data(),take,out);out+=take;n-=take;}}};
struct ProbeIO:IO {std::uint64_t messages=0,coins=0; r1::Fs digest{"LEAF-PROBE-MESSAGES"};bool prover()const override{return true;}std::string session()const override{return std::string(64,'b');}Digest prefix()const override{return {};}
 Bytes message(const std::string&role,const Bytes&b={})override{++messages;digest.text("role",role);digest.bytes("bytes",b);return b;}
 Fr coin(const std::string&role)override{auto x=Fr(2+(coins++%97));digest.text("coin",role);digest.scalar("value",x);return x;}
};
}
int main(int argc,char**argv){try{
 r1::init();need(argc==3,"usage: product_leaf_probe A|B n");const std::string label=argv[1];const std::size_t n=std::stoull(argv[2]);need(label=="A" || label=="B","probe label");
 ProbeIO io;FixedRandom rng;const std::vector<Fr>table{Fr(7),Fr(11),Fr(5),Fr(19)};
 auto av=std::make_shared<std::vector<Fr>>(n),vv=std::make_shared<std::vector<Fr>>(n),reads=std::make_shared<std::vector<Fr>>(n),finals=std::make_shared<std::vector<Fr>>(4,Fr(0));
 for(std::size_t i=0;i<n;++i){av->at(i)=Fr(i%4);vv->at(i)=table[i%4];reads->at(i)=finals->at(i%4);finals->at(i%4)+=Fr(1);}
 std::size_t aReads=0,vReads=0,rReads=0;
 auto auth=[](IO&,const Query&,const Hidden*h,Random*,std::string){need(h,"probe source witness");};
 VectorOracle a{n,n,"probe/address",[&](std::size_t j){++aReads;return av->at(j);},auth};
 VectorOracle v{n,n,"probe/value",[&](std::size_t j){++vReads;return vv->at(j);},auth};
 VectorFactory factory=[&](IO&,std::string id,std::size_t actual,std::size_t padded,Value){VectorOracle out;out.actual=actual;out.padded=padded;out.id=id;out.open=auth;
  if(id=="probe/memory/read")out.value=[&](std::size_t j){++rReads;return reads->at(j);};
  else {need(id=="probe/memory/final","probe factory role");out.value=[&](std::size_t j){return finals->at(j);};}return out;};
 // Start tracking after input preparation, before all root/leaf work.
 auto start=std::chrono::steady_clock::now();std::uint64_t allocs,bytes;
 {AllocationTrace trace;if(label=="A")zkgpt::r7::leaf_reference::referenceMemory(io,a,v,table,factory,&rng,"probe/memory",true);else hiddenMemory(io,a,v,table,factory,&rng,"probe/memory",true);allocs=trace.count();bytes=trace.bytes();}
 auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();auto d=io.digest.digest();
 std::cout<<"LEAF_PROBE {\"label\":\""<<label<<"\",\"n\":"<<n<<",\"wall_seconds\":"<<elapsed<<",\"allocations\":"<<allocs<<",\"allocated_bytes\":"<<bytes<<",\"a_reads\":"<<aReads<<",\"v_reads\":"<<vReads<<",\"read_reads\":"<<rReads<<",\"messages\":"<<io.messages<<",\"coins\":"<<io.coins<<",\"digest\":\"";for(auto b:d)std::cout<<"0123456789abcdef"[b>>4]<<"0123456789abcdef"[b&15];std::cout<<"\"}\n";
 return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
