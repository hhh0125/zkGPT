#include "paper_r7_lasso_masked.hpp"
#include "paper_r7_oneblock_lookup.hpp"
#include "paper_r7_oneblock_vectors.hpp"
#include "paper_r7_fs_io.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <sys/resource.h>
using namespace zkgpt::r7;
using namespace oneblock;
namespace {
struct NoMessages:IO {
 bool prover()const override{return true;}std::string session()const override{return "shape-test";}Digest prefix()const override{return {};}
 Bytes message(const std::string&,const Bytes& = {})override{throw std::runtime_error("entered protocol before rejecting shape");}
 Fr coin(const std::string&)override{throw std::runtime_error("coin before rejecting shape");}
};
void shapeGuards(){
 NoMessages io;OSRandom rng;masked::ScopedMode selected(masked::Mode::AllProductsResearch);auto h=publicHidden(io,"root",Fr(0));
 std::vector<Fr>eq(4,Fr(0)),children(8,Fr(0));ProductInputs input{eq,children};std::array<ProductInputs,2>inputs{{input,input}};
 for(unsigned b=0;b<2;++b){bool rejected=false;try{
  if(b)detail::batchedProductSumcheckContiguous(io,8,{h,h},{},&rng,"shape",inputs);
  else productSumcheck(io,8,h,{},&rng,"shape",&input);
 }catch(const std::exception&e){std::string text=e.what();rejected=text=="Product native input binding" || text=="Product01 continuous dimensions";}
 need(rejected,"masked dispatch bypassed input shape guard");}
 std::cout<<"PASS masked direct input guards\n";
}

struct Observed:IO,EvidenceSink {
 IO&base;std::string attack;bool hit=false;bool recurrence_pending=false;std::chrono::steady_clock::time_point recurrence_start{};double recurrence_observed_seconds=0;
 void finishRecurrence(){if(recurrence_pending){recurrence_observed_seconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-recurrence_start).count();recurrence_pending=false;}}
std::size_t masked_rounds=0,legacy_rounds=0,original_terminals=0;std::vector<std::string>coverage;
 Observed(IO&b,std::string a):base(b),attack(std::move(a)){}
 bool prover()const override{return base.prover();}std::string session()const override{return base.session();}Digest prefix()const override{return base.prefix();}
 Fr coin(const std::string&s)override{finishRecurrence();return base.coin(s);}
 Bytes message(const std::string&s,const Bytes&b={})override{
  finishRecurrence();
  if(s.find("/recurrence/statement")!=std::string::npos)recurrence_start=std::chrono::steady_clock::now();
  Bytes sent=b;bool forged=false;
  if(prover() && attack.rfind("forge:",0)==0 && !hit && s.find(attack.substr(6))!=std::string::npos){
   forged=true;hit=true;std::cerr<<"ATTACK_REACHED "<<s<<"\n";
   if(s.find("/mask-vector")!=std::string::npos){Reader r{sent};auto point=r.point();r.end();Writer w;w.point(point+scalarCommit(Fr(1),Fr(0)));sent=w.b;}
   else if(s.find("/coefficients")!=std::string::npos){Reader r{sent};Writer w;for(unsigned k=0;k<4;++k)w.scalar(r.scalar()+Fr(k==0?1:0));r.end();sent=w.b;}
   else if(s.find("/commitment")!=std::string::npos || s.find("/handle")!=std::string::npos){auto h=decodeHandle(sent);h.point+=scalarCommit(Fr(1),Fr(0));sent=encode(h);}
   else{Reader r{sent};auto encoded=r1::encode(r.scalar()+Fr(1));std::copy(encoded.begin(),encoded.end(),sent.begin());}
  }
  auto out=base.message(s,sent);
  if(forged && s.find("/mask-vector")==std::string::npos)out=b;
  if(s.find("/terminal/")!=std::string::npos && s.find("/responses")!=std::string::npos && (s.find("/term/0/mul/")!=std::string::npos || s.find("/terminal/expression/")!=std::string::npos))++original_terminals;
  if(s.find("/masked/round/")!=std::string::npos && s.find("/coefficients")!=std::string::npos)++masked_rounds;
  if(s.find("/round/")!=std::string::npos && s.find("/recurrence/responses")!=std::string::npos)++legacy_rounds;
  if(!prover() && !attack.empty() && !hit && s.find(attack)!=std::string::npos){
   std::cerr<<"ATTACK_REACHED "<<s<<"\n";hit=true;
   if(s.find("/coefficients")!=std::string::npos){Reader r{out};Writer w;for(unsigned k=0;k<4;++k)w.scalar(r.scalar()+Fr(k==0?1:0));r.end();out=w.b;}
   else if(s.find("/responses")!=std::string::npos || s.find("/z1-z2")!=std::string::npos){Reader r{out};auto x=r.scalar()+Fr(1);auto e=r1::encode(x);std::copy(e.begin(),e.end(),out.begin());}
   else if(s.find("/commitment")!=std::string::npos || s.find("/handle")!=std::string::npos){auto h=decodeHandle(out);h.point+=scalarCommit(Fr(1),Fr(0));out=encode(h);}
   else {throw std::runtime_error("unsupported attack target");}
  }
  if(s.find("/recurrence/responses")!=std::string::npos)recurrence_pending=true;
  return out;
 }
 void checkedRelation(const std::string&s)override{coverage.push_back(s);}
};
struct Fixture {
 std::string fault;
 VectorOracle oracle(IO&io,std::string id,std::size_t actual,std::size_t n,Value value,Random*rng){
  auto original=value;
  if(io.prover() && id==fault)value=[original](std::size_t i){return original(i)+Fr(1);};
  auto out=exchangeVector(io,id,actual,n,value,rng);
  if(io.prover() && id==fault)out.value=original;
  return out;
 }
 void run(IO&io,std::string kind,std::size_t n,std::size_t actual,Random*rng){
  if(kind=="batch" || kind=="single"){
   auto values=std::make_shared<std::vector<Fr>>(n);for(std::size_t i=0;i<n;++i)(*values)[i]=Fr(int(i%5)+2);
   if(fault=="zeros")for(auto&v:*values)v=Fr(0);
   Value val=io.prover()?Value([values](std::size_t i){return values->at(i);}):Value{};
   auto source=oracle(io,"fixture/base",n,n,val,rng);
   if(kind=="batch"){
    std::array<Hidden,2>roots;for(unsigned b=0;b<2;++b){Fr product=1;if(io.prover())for(auto&v:*values)product*=v+Fr(b==0?1:0);roots[b]=exchangeHidden(io,"fixture/root/"+std::to_string(b),product,rng);}
    BatchedLeafAuthenticator auth=[&](IO&live,const std::vector<Fr>&r,const std::array<Hidden,2>&leaves,Random*random,std::string role){
     auto base=exchangeHidden(live,role+"/base",live.prover()?r1::mle(*values,r):Fr(0),random);
     source.open(live,{r,base.handle},live.prover()?&base:nullptr,random,role+"/source");
     auto shifted=combine(live.session(),role+"/base-plus-one",{base,publicHidden(live,role+"/one",Fr(1))},{Fr(1),Fr(1)});
     Relations rel;rel.role=role+"/independent-leaves";std::vector<Hidden>w{leaves[0],leaves[1],shifted,base};for(auto&h:w)rel.handles.push_back(h.handle);rel.linear={{Fr(1),Fr(0),Fr(-1),Fr(0)},{Fr(0),Fr(1),Fr(0),Fr(-1)}};sigma(live,rel,live.prover()?&w:nullptr,random);
    };
    hiddenBatchedProduct(io,n,roots,io.prover()?values:nullptr,auth,rng,"fixture/product");
   }else{
    Fr product=1;if(io.prover())for(auto&v:*values)product*=v;
    auto root=exchangeHidden(io,"fixture/root/2",product,rng);
    LeafAuthenticator auth=[&](IO&live,const Query&q,const Hidden*h,Random*r,std::string role){source.open(live,q,h,r,role+"/source");};
    hiddenProduct(io,n,root,source.value,auth,rng,"fixture/product/2",nullptr,fault!="generic");
   }return;
  }
  Value av=io.prover()?Value([actual](std::size_t i){return Fr(i<actual?i%4:0);}):Value{};
  auto a=oracle(io,"fixture/address",actual,n,av,rng);
  VectorFactory factory=[&](IO&live,std::string id,std::size_t count,std::size_t domain,Value values){return oracle(live,id,count,domain,values,rng);};
  if(kind=="memory"){
   Value vv=io.prover()?Value([actual](std::size_t i){return Fr(i<actual?3*(i%4):0);}):Value{};
   auto v=oracle(io,"fixture/value",actual,n,vv,rng);
   hiddenMemory(io,a,v,{Fr(0),Fr(3),Fr(6),Fr(9)},factory,rng,"fixture/memory");
  }else if(kind=="range")hiddenRange(io,a,2,factory,rng,"fixture/range");
  else if(kind=="exp"){
   Value ev=io.prover()?Value([actual](std::size_t i){return Fr(i<actual?r1::expValue(i%4):0);}):Value{};
   auto v=oracle(io,"fixture/exp",actual,n,ev,rng);
   hiddenExp(io,a,v,r1::ExpProfile::Unclamped,factory,rng,"fixture/exp");
  }else need(false,"unknown fixture");
 }
};
}
int main(int argc,char**argv){
 try{
  if(argc==2 && std::string(argv[1])=="shape"){r1::init();shapeGuards();return 0;}
  need(argc==8 || argc==9,"prove|verify batch|single|memory|range|exp sigma|01|all n actual proof fault [attack]");
  r1::init();std::string action=argv[1],kind=argv[2],mode=argv[3],path=argv[6],fault=argv[7],attack=argc==9?argv[8]:"";
  auto n=std::stoull(argv[4]),actual=std::stoull(argv[5]);need(n && !(n&(n-1)) && n<=(1u<<20) && actual && actual<=n,"fixture dimensions");
  need(mode=="sigma" || mode=="01" || mode=="all","explicit research mode");
  masked::ScopedMode select(mode=="all"?masked::Mode::AllProductsResearch:mode=="01"?masked::Mode::Product01Research:masked::Mode::Sigma);
  fs::Binding binding{"Unit","LASSO-MASKED-RESEARCH-v1/"+kind+"/"+mode,fs::generatorIdentity(),Bytes{}};
  Writer instance;instance.text(kind);instance.text(mode);instance.number(n,8);instance.number(actual,8);binding.public_instance=instance.b;
  Fixture fixture;fixture.fault=fault;
  OSRandom rng;std::unique_ptr<fs::FsIO>wire;
  auto started=std::chrono::steady_clock::now();
  if(action=="prove")wire=std::make_unique<fs::FsProverIO>(path,binding);else{need(action=="verify","unknown action");wire=std::make_unique<fs::FsVerifierIO>(path,binding);}
  Observed io(*wire,attack);fixture.run(io,kind,n,actual,action=="prove"?&rng:nullptr);io.finishRecurrence();wire->finish();
  if(kind=="single" && fault=="legacy-terminal")need(io.original_terminals==3*r1::log2exact(n),"original single Product terminal protocol was replaced");
  const bool expected=mode=="all" || (mode=="01" && kind!="single");
  if(n>=4 && expected)need(io.masked_rounds>0,"masked Product rounds missing: legacy Sigma path executed");
  if(!attack.empty())need(io.hit,"attack target not reached");
  auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();rusage usage{};getrusage(RUSAGE_SELF,&usage);
  std::cout<<std::setprecision(12)<<"RESULT {\"action\":\""<<action<<"\",\"kind\":\""<<kind<<"\",\"mode\":\""<<mode<<"\",\"n\":"<<n<<",\"actual\":"<<actual<<",\"seconds\":"<<elapsed<<",\"proof_bytes\":"<<wire->proofBytes()<<",\"peak_rss_kib\":"<<usage.ru_maxrss<<",\"masked_rounds\":"<<io.masked_rounds<<",\"legacy_rounds\":"<<io.legacy_rounds<<",\"recurrence_observed_seconds\":"<<io.recurrence_observed_seconds<<",\"coverage\":"<<io.coverage.size()<<",\"completed\":true}\n";
  return 0;
 }catch(const std::exception&e){std::cerr<<"REJECT "<<e.what()<<"\n";return 1;}
}
