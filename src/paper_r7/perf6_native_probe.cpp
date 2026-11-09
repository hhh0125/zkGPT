// Bounded synthetic witness; actual production Native/Lasso/deferred authentication.
#include "classified_commitments.hpp"
#include "oneblock_lookup.hpp"
#include "fs_io.hpp"
#include "paper_r1/wire.hpp"
#include <openssl/sha.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <set>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
using namespace zkgpt::r7;using namespace oneblock;using namespace classified;
namespace fs=zkgpt::r7::fs;
namespace {
bool fixedNonce=false;
struct NativeRandom:Random {
 std::uint64_t counter=0;
 void fill(std::uint8_t*out,std::size_t n)override{
  while(n){std::array<std::uint8_t,40>input{};constexpr char domain[]="PERF6-native-test-masks";std::copy_n(domain,sizeof(domain)-1,input.begin());for(unsigned j=0;j<8;++j)input[32+j]=std::uint8_t(counter>>(8*j));++counter;
   Digest d{};SHA256(input.data(),input.size(),d.data());auto take=std::min(n,d.size());std::copy_n(d.data(),take,out);out+=take;n-=take;
  }
 }
};
Layout nativeLayout(std::size_t n){
 Layout p;p.native=std::string(64,'6');p.roots={{"C_W",1,1,true,true},{"C_aux",2*n,2*n,true,false},{"C_lookup/read",n,n,false,false},{"C_lookup/final",512,512,false,false}};
 p.base.push_back({0,0});for(std::size_t j=0;j<2*n;++j)p.base.push_back({1,std::uint32_t(j)});
 View a;a.id="perf6/native/address";a.actual=a.padded=n;a.mapped=true;a.spans={{1,n,Fr(0)}};a.kind=ObjectKind::MappedChild;p.views.emplace(a.id,a);
 View v=a;v.id="perf6/native/value";v.spans={{1+n,n,Fr(0)}};p.views.emplace(v.id,v);
 View read;read.id="perf6/native/memory/read";read.actual=read.padded=n;read.start={2,0};read.kind=ObjectKind::MemoryRead;p.views.emplace(read.id,read);
 View final=read;final.id="perf6/native/memory/final";final.actual=final.padded=512;final.start={3,0};final.kind=ObjectKind::MemoryFinal;p.views.emplace(final.id,final);return p;
}
void executeNative(IO&io,std::size_t n,Random*rng,const std::string&seedPath,const std::string&attack){
 auto layout=nativeLayout(n);Prepared prepared;std::shared_ptr<std::vector<Fr>>base;
 std::vector<Fr>table(512);for(std::size_t j=0;j<512;++j)table[j]=Fr(7+3*j);
 if(io.prover()){
  auto seed=r1::readFile(seedPath);need(seed.size()==32,"PERF6 native synthetic input seed length");base=std::make_shared<std::vector<Fr>>(2*n+1,Fr(0));base->at(0)=Fr(1);
  auto read=std::make_shared<std::vector<Fr>>(n,Fr(0)),final=std::make_shared<std::vector<Fr>>(512,Fr(0));std::array<std::size_t,512>counts{};
  for(std::size_t j=0;j<n;++j){std::array<std::uint8_t,40>input{};std::copy(seed.begin(),seed.end(),input.begin());for(unsigned k=0;k<8;++k)input[32+k]=std::uint8_t(j>>(8*k));Digest d{};SHA256(input.data(),input.size(),d.data());auto index=(unsigned(d[0])+256*unsigned(d[1]))%512;base->at(1+j)=Fr(index);base->at(1+n+j)=table[index];read->at(j)=Fr(counts[index]++);}
  for(std::size_t j=0;j<512;++j)final->at(j)=Fr(counts[j]);
  if(attack=="read")read->at(0)+=Fr(1);
  prepared.emplace("perf6/native/memory/read",read);prepared.emplace("perf6/native/memory/final",final);
 }
 Value values;if(io.prover())values=[base](std::size_t j){return base->at(j);};
 Commitments pcs(io,layout,values,io.prover()?&prepared:nullptr,rng,std::vector<Fr>{Fr(1)});pcs.constrainBaseTails();
 auto address=pcs.view("perf6/native/address"),value=pcs.view("perf6/native/value");std::set<std::string>objects{address.id,value.id};
 VectorFactory factory=[&](IO&live,std::string id,std::size_t actual,std::size_t padded,Value){need(&live==&io && objects.insert(id).second,"PERF6 native duplicate/foreign helper");auto v=pcs.view(id);need(v.actual==actual && v.padded==padded,"PERF6 native helper shape");return v;};
 hiddenMemory(io,address,value,table,factory,rng,"perf6/native/memory",true);need(objects.size()==layout.views.size(),"PERF6 native missing helper");
 for(const auto&v:{address,value}){const auto name=v.id+"/source";std::vector<Fr>q;for(unsigned k=0;k<r1::log2exact(n);++k)q.push_back(io.coin(name+"/q/"+std::to_string(k)));Fr y=0;if(io.prover()){auto w=r1::weights(q);for(std::size_t j=0;j<n;++j)y+=w[j]*v.value(j);}auto h=exchangeHidden(io,name+"/value",y,rng);v.open(io,{q,h.handle},io.prover()?&h:nullptr,rng,name+"/open");}
 need(pcs.claims()==9 && !pcs.authenticated(),"PERF6 native deferred source claim count");
 auto begin=std::chrono::steady_clock::now();if(attack!="omit-auth")pcs.authenticateUnifiedInput({Fr(1)});need(pcs.authenticated(),"PERF6 native required authentication barrier absent");
 std::cerr<<"PERF6_NATIVE_AUTH {\"claims\":"<<pcs.claims()<<",\"authenticated\":true,\"seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()<<"}\n";
}
fs::Binding nativeBinding(std::size_t n,bool other){Writer w;w.text(identity(nativeLayout(n)));w.number(n,8);return {"OneblockSingleHyrax",other?"perf6-native-other":"perf6-native-bounded",std::string(64,'6'),w.b};}
}
extern "C" ssize_t getrandom(void*out,size_t n,unsigned flags){if(fixedNonce){need(n==32,"PERF6 native nonce");for(std::size_t j=0;j<n;++j)static_cast<std::uint8_t*>(out)[j]=std::uint8_t(j+17);fixedNonce=false;return n;}return ::syscall(SYS_getrandom,out,n,flags);}
int main(int argc,char**argv){try{
 need(argc>=4,"mode proof n [synthetic-input-seed] [attack]");auto begin=std::chrono::steady_clock::now();r1::init();auto n=std::stoull(argv[3]);need(n>=512 && n<=32768 && !(n&(n-1)),"PERF6 native bounded domain");bool prover=std::string(argv[1])=="prove";need(prover || std::string(argv[1])=="verify","PERF6 native mode");
 if(prover){need(argc>=5,"PERF6 native private synthetic seed required");auto attack=argc>5?argv[5]:"";fixedNonce=true;fs::FsProverIO io(argv[2],nativeBinding(n,false));NativeRandom random;executeNative(io,n,&random,argv[4],attack);io.finish();std::cout<<io.metadataJson()<<'\n';}
 else{auto attack=argc>4?argv[4]:"";fs::FsVerifierIO io(argv[2],nativeBinding(n,std::string(attack)=="session"));executeNative(io,n,nullptr,"",attack);io.finish();std::cout<<io.metadataJson()<<'\n';}
 rusage usage{};getrusage(RUSAGE_SELF,&usage);std::cout.precision(12);std::cout<<"PERF6_NATIVE_WHOLE {\"n\":"<<n<<",\"prover\":"<<(prover?"true":"false")<<",\"wall_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()<<",\"peak_rss_kib\":"<<usage.ru_maxrss<<",\"private_synthetic_seed_P_only\":true,\"actual_Full12_witness\":false}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
