// Supplemental PUBLIC toy fixture: full28/32-round FS module Proof with45
// original VectorPC/Hyrax openings. Never a real model/Witness generator.
#include "unified_input.hpp"
#include "public_k.hpp"
#include "oneblock_vectors.hpp"
#include "fs_io.hpp"
#include <sys/random.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <iostream>
#include <algorithm>
using namespace zkgpt::r7;using namespace crypto;using namespace oneblock;
namespace ui=zkgpt::r7::oneblock::unified;namespace fs=zkgpt::r7::fs;
thread_local bool fixNonce=false;
extern "C" ssize_t getrandom(void*p,size_t n,unsigned flags){if(fixNonce){need(n==32,"module fixed nonce length");for(std::size_t j=0;j<n;++j)static_cast<unsigned char*>(p)[j]=std::uint8_t(j+17);fixNonce=false;return n;}return ::syscall(SYS_getrandom,p,n,flags);}
struct Fixed:Random {std::uint64_t counter=0;void fill(std::uint8_t*p,std::size_t n)override{while(n){Writer w;w.text("opt3d-module-test-only-fixed");w.number(counter++,8);auto d=r1::hash(w.b);auto count=std::min(n,d.size());std::copy_n(d.data(),count,p);p+=count;n-=count;}}};
constexpr std::size_t padded(unsigned k){return std::size_t(1)<<(1+k%4);}
constexpr std::size_t stored(unsigned k){return k%3?padded(k)-1:padded(k);}
constexpr int u(unsigned k,std::size_t j){return j>=stored(k) || k%11==0?0:int((k*5+j*3)%23)-11;}
constexpr int b(unsigned k,std::size_t j){return int((k*7+j*5)%19)-9;}
constexpr long initialA(){long out=0;for(unsigned k=0;k<45;++k)for(std::size_t j=0;j<padded(k);++j)out+=u(k,j)*b(k,j);return out;}
void protocol(IO&io,unsigned bits,unsigned mode,Random*rng){
 std::vector<ui::Block>blocks;for(unsigned k=0;k<45;++k)blocks.push_back({1+k%3,padded(k)});auto packing=ui::pack(blocks,4);auto segments=packing.segments;
 for(auto&s:segments){auto category=s.offset/packing.partition_size;s.offset=category*(std::size_t(1)<<(bits-2))+s.offset%packing.partition_size;}
 const auto domain=std::size_t(1)<<bits;ui::validate(domain,segments);
 Writer plan;plan.text("OPT3D-PUBLIC-TOY-MODULE-NOT-REAL-MODEL");plan.number(bits);plan.number(45);for(auto s:segments){plan.number(s.offset,8);plan.number(s.padded,8);}bindPublic(io,"Support/public-plan",plan.b);
 std::vector<VectorPC>pcs(45);std::vector<VectorSecret>secrets;secrets.reserve(io.prover()?45:0);
 for(unsigned k=0;k<45;++k){auto name="support/root/"+std::to_string(k);
  if(io.prover()){auto values=std::make_shared<std::vector<Fr>>(padded(k));for(std::size_t j=0;j<values->size();++j)(*values)[j]=Fr(u(k,j));secrets.push_back(commitVectorOwned(name,stored(k),padded(k),std::move(values),*rng));pcs[k]=secrets.back().pc;}
  auto bytes=io.message("Support/PC/"+std::to_string(k),io.prover()?encodeVector(pcs[k]):Bytes{});pcs[k]=decodeVector(bytes);need(pcs[k].id==name && pcs[k].actual==stored(k) && pcs[k].padded==padded(k) && pcs[k].layout==r1::Layout::RowMajor,"module root identity");
 }
 const std::vector<Fr>x{Fr(2),Fr(3),Fr(5)};
 auto build=[&](std::size_t root){std::vector<Fr>v(segments[root].padded,Fr(0));if(root)for(std::size_t j=0;j<v.size();++j)v[j]=Fr(b(unsigned(root-1),j));return v;};
 auto endpoint=[&](const std::vector<Fr>&point){Fr out=0;for(std::size_t k=1;k<segments.size();++k){auto d=r1::log2exact(segments[k].padded);out+=ui::selector(segments[k],point)*r1::mle(build(k),{point.begin(),point.begin()+d});}return out;};
 auto initial=publicHidden(io,"Support/initial-A",Fr(initialA()));std::unique_ptr<ui::PublicK>state;ui::Factors factors;ui::Inputs inputs;
 if(io.prover()){
  state=std::make_unique<ui::PublicK>(segments,build,ui::KBudget{1u<<20,0},io.session(),domain);
  factors=[&](std::size_t k,const ui::Consume&consume){state->withRoot(k,io.session(),[&](const Value&kernel){consume(k?Value([&,k](std::size_t j){return secrets[k-1].values->at(j);}):Value([&](std::size_t j){return j<x.size()?x[j]:Fr(0);}),kernel);});};
  if(mode)inputs=[&](std::size_t k){return k?ui::InputSpan{secrets[k-1].values->data(),stored(unsigned(k-1)),segments[k].padded}:ui::InputSpan{x.data(),x.size(),4};};
 }
 if(io.prover())setenv("ZKGPT_U_THREADS",mode==1?"1":mode==2?"2":"4",1);
 auto reduced=ui::reduce(io,domain,segments,initial,factors,endpoint,rng,"UnifiedInput/sumcheck",state.get(),inputs);state.reset();need(reduced.point.size()==bits,"module round count");
 std::vector<Hidden>opened;std::vector<Fr>selectors;
 for(unsigned k=0;k<45;++k){auto d=r1::log2exact(padded(k));std::vector<Fr>q(reduced.point.begin(),reduced.point.begin()+d);auto name="UnifiedInput/opening/root/"+std::to_string(k);auto value=io.prover()?r1::mle(*secrets[k].values,q):Fr(0);auto h=exchangeHidden(io,name+"/evaluation",value,rng);openVector(io,pcs[k],{q,h.handle},io.prover()?&secrets[k]:nullptr,io.prover()?&h:nullptr,rng,name+"/hyrax");opened.push_back(h);selectors.push_back(ui::selector(segments[k+1],reduced.point));}
 auto stitched=combine(io.session(),"UnifiedInput/opening/stitched",opened,selectors);bindPublic(io,"UnifiedInput/opening/stitched",encode(stitched.handle));auto q=std::vector<Fr>(reduced.point.begin(),reduced.point.begin()+2);auto weights=r1::weights(q);Fr publicValue=0;for(std::size_t j=0;j<x.size();++j)publicValue+=weights[j]*x[j];publicValue*=ui::selector(segments[0],reduced.point);auto pub=publicHidden(io,"UnifiedInput/opening/public-X",publicValue);
 std::vector<Hidden>all{stitched,pub,reduced.endpoints.at(0)};Relations relation;relation.role="UnifiedInput/opening/input-consistency";for(auto&h:all)relation.handles.push_back(h.handle);relation.linear={{Fr(1),Fr(1),Fr(-1)}};sigma(io,relation,io.prover()?&all:nullptr,rng);
}
int main(int argc,char**argv){try{r1::init();need(argc>=4,"module prove/verify path bits [mode]");const bool prover=std::string(argv[1])=="prove";need(prover || std::string(argv[1])=="verify","module role");need((prover && argc==5) || (!prover && argc==4),"module arguments");unsigned bits=std::stoul(argv[3]);need(bits==28 || bits==32,"module dimension");unsigned mode=prover?unsigned(std::stoul(argv[4])):0;need(!prover || (argc==5 && mode<=3),"module variant");fs::Binding binding{bits==32?"Full12Unified":"OneblockUnified","opt3d-public-module-"+std::to_string(bits),std::string(64,'a'),{2,7,1,8}};Fixed rng;
 if(prover){fixNonce=true;fs::FsProverIO io(argv[2],binding);protocol(io,bits,mode,&rng);io.finish();std::cout<<"OPT3D_MODULE "<<io.metadataJson()<<'\n';}else{fs::FsVerifierIO io(argv[2],binding);protocol(io,bits,0,nullptr);io.finish();std::cout<<"OPT3D_MODULE "<<io.metadataJson()<<'\n';}return 0;
 }catch(const std::exception&e){std::cerr<<"OPT3D_MODULE_FAILED "<<e.what()<<'\n';return 1;}}
