#include "paper_r7_oneblock_lookup.hpp"
#include "paper_r7_oneblock_vectors.hpp"
#include "paper_r7_fs_io.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <unistd.h>
using namespace zkgpt::r7;
using namespace oneblock;
using namespace crypto;
namespace {
using Clock=std::chrono::steady_clock;
double seconds(Clock::time_point t){return std::chrono::duration<double>(Clock::now()-t).count();}
struct Observed:IO,EvidenceSink {
 IO&base;std::string attack;bool hit=false;std::set<std::string>evidence;
 std::set<std::string>messages;
 Observed(IO&b,std::string a={}):base(b),attack(std::move(a)){}
 bool prover()const override{return base.prover();}
 std::string session()const override{return base.session();}
 Digest prefix()const override{return base.prefix();}
 Fr coin(const std::string&s)override{
  if(s.find("/product/01/")!=std::string::npos && s.find("/sumcheck/round/")!=std::string::npos){
   auto prefix=s.substr(0,s.find('/',s.find("/sumcheck/round/")+16));
   for(unsigned b=0;b<2;++b)for(unsigned k=0;k<4;++k)
    need(messages.count(prefix+"/instance/"+std::to_string(b)+"/c/"+std::to_string(k)+"/commitment"),"challenge before both polynomials fixed");
  }
  if(s.find("/product/01/")!=std::string::npos && s.size()>5 && s.substr(s.size()-5)=="/fold"){
   auto prefix=s.substr(0,s.size()-5)+"/sumcheck";
   for(unsigned k=1;k<5;++k)need(messages.count(prefix+"/endpoint/"+std::to_string(k)+"/commitment"),"fold challenge before both child pairs fixed");
  }
  if(s=="r7/fixture/gamma" || s=="r7/fixture/tau")for(auto&object:{"fixture/address","fixture/value","r7/fixture/read","r7/fixture/final"})
   need(messages.count(std::string("Oneblock/object/")+object+"/PC"),"fingerprint challenge before source commitments");
  return base.coin(s);
 }
 Bytes message(const std::string&s,const Bytes&b={})override{
  auto out=base.message(s,b);
  messages.insert(s);
  if(!prover() && !hit && !attack.empty() && s==attack){auto h=decodeHandle(out);h.point+=scalarCommit(Fr(1),Fr(0));out=encode(h);hit=true;}
  return out;
 }
 void checkedRelation(const std::string&s)override{need(evidence.insert(s).second,"duplicate coverage");}
};
struct Run {
 std::map<std::string,std::size_t>opens;std::map<std::string,std::vector<std::vector<Fr>>>points;
 std::vector<std::function<void()>>pending;std::size_t claims=0;
 std::string bad_source;
 VectorOracle oracle(IO&io,std::string id,std::size_t actual,std::size_t padded,Value values,Random*rng){
  auto original=values;
  if(io.prover() && id==bad_source)values=[original](std::size_t i){return original(i)+Fr(1);};
  auto out=exchangeVector(io,id,actual,padded,values,rng);auto open=out.open;
  // Compute the whole memory proof consistently from the original source,
  // while its actual PC commits to a different vector. Deferred opening must
  // reject this even though the multiset and both leaf fingerprints agree.
  if(io.prover() && id==bad_source)out.value=original;
  out.open=[this,open,id](IO&live,const Query&q,const Hidden*h,Random*r,std::string role){
   ++opens[id];++claims;points[id].push_back(q.point);Hidden saved;if(h)saved=*h;
   pending.push_back([open,&live,q,saved,r,role]{open(live,q,live.prover()?&saved:nullptr,r,role);});
  };return out;
 }
 void memory(IO&io,std::size_t n,std::size_t actual,Random*rng){
  auto av=[actual](std::size_t i){return Fr(i<actual?i%4:0);};
  auto vv=[actual](std::size_t i){return Fr(i<actual?3*(i%4):0);};
  auto a=oracle(io,"fixture/address",actual,n,io.prover()?Value(av):Value{},rng);
  auto v=oracle(io,"fixture/value",actual,n,io.prover()?Value(vv):Value{},rng);
  VectorFactory f=[this,rng](IO&live,std::string id,std::size_t a,std::size_t p,Value val){return oracle(live,id,a,p,val,rng);};
  hiddenMemory(io,a,v,{Fr(0),Fr(3),Fr(6),Fr(9)},f,rng,"r7/fixture");
  for(auto&f:pending)f();
 }
};
void check(bool b,const char*s){need(b,s);}
#ifndef KIND01_REFERENCE
void direct(const std::filesystem::path&dir,std::size_t n,const std::string&fault){
 auto path=(dir/("direct-"+std::to_string(n)+"-"+fault+".proof")).string();
 fs::Binding binding{"Unit","kind01-direct",std::string(64,'b'),Bytes{4,5,6}};
 auto base=std::make_shared<std::vector<Fr>>(n);for(std::size_t i=0;i<n;++i)base->at(i)=Fr(int(i%5)-1);
 std::array<Fr,2>products{Fr(1),Fr(1)};for(auto&f:*base){products[0]*=f+Fr(1);products[1]*=f;}
 // Independent sequential product, also authenticated by the original reducer.
 auto protocol=[&](IO&io,Random*rng){
  std::array<Hidden,2>roots;for(unsigned b=0;b<2;++b){auto value=products[b];if(fault=="root"+std::to_string(b) || fault=="cancel")value+=b==0?Fr(1):Fr(-1);roots[b]=exchangeHidden(io,"direct/root/"+std::to_string(b),io.prover()?value:Fr(0),rng);}
  BatchedLeafAuthenticator auth=[&](IO&live,const std::vector<Fr>&point,const std::array<Hidden,2>&leaves,Random*random,std::string role){
   auto f=r1::mle(*base,point);std::vector<Hidden>w{leaves[0],leaves[1]};
   for(unsigned b=0;b<2;++b){auto expected=f+Fr(b==0?1:0);if(fault=="leaf"+std::to_string(b))expected+=Fr(1);w.push_back(publicHidden(live,role+"/expected/"+std::to_string(b),expected));}
   Relations rel;rel.role=role+"/leaf-relations";for(auto&h:w)rel.handles.push_back(h.handle);rel.linear={{Fr(1),Fr(0),Fr(-1),Fr(0)},{Fr(0),Fr(1),Fr(0),Fr(-1)}};sigma(live,rel,live.prover()?&w:nullptr,random);
  };
  hiddenBatchedProduct(io,n,roots,io.prover()?base:nullptr,auth,rng,"direct/product");
  if(fault.empty())for(unsigned b=0;b<2;++b){
   auto values=std::make_shared<std::vector<Fr>>(*base);if(b==0)for(auto&x:*values)x+=Fr(1);
   LeafAuthenticator old=[&,values](IO&live,const Query&q,const Hidden*h,Random*r,std::string name){Hidden claim;if(live.prover())claim=*h;else claim.handle=q.evaluation;equalHidden(live,claim,r1::mle(*values,q.point),r,name+"/expected");};
   hiddenProduct(io,n,roots[b],{},old,rng,"direct/old/"+std::to_string(b),io.prover()?values:nullptr);
  }
 };
 bool rejected=false;try{OSRandom rng;{fs::FsProverIO io(path,binding);protocol(io,&rng);io.finish();}{fs::FsVerifierIO io(path,binding);protocol(io,nullptr);io.finish();}}catch(const std::exception&e){if(fault.empty())throw;rejected=true;std::cout<<"REJECT semantic "<<fault<<" n="<<n<<": "<<e.what()<<'\n';}
 check(rejected==!fault.empty(),"false direct Product relation accepted");
 if(fault.empty())std::cout<<"PASS independent old/new roots n="<<n<<'\n';
}
#endif
}
int main(int argc,char**argv){try{
 r1::init();std::string mode=argc>1?argv[1]:"shared";std::size_t n=argc>2?std::stoull(argv[2]):8,actual=argc>3?std::stoull(argv[3]):n;
 if(mode=="identity"){
  Writer parameters;parameters.text(crypto::suite);parameters.text(r1::kModulus);auto p=crypto::parameters(1);parameters.point(p.scalar);parameters.point(p.hiding);
  auto g=hyrax_a3::generators(4096);parameters.number(g.size());for(auto&x:g)parameters.point(x);
  std::ofstream out("reports/kind01-20261116/public-parameters.bin",std::ios::binary);out.write(reinterpret_cast<const char*>(parameters.b.data()),parameters.b.size());check(bool(out),"parameter artifact write");
  std::cout<<"PARAMETERS {\"fs_generator_identity\":\""<<fs::generatorIdentity()<<"\",\"public_parameter_bytes_sha256\":\""<<hex(r1::hash(parameters.b))<<"\",\"generators\":4096}\n";return 0;
 }
 auto directory=std::filesystem::path("reports/kind01-tests")/(std::to_string(getpid())+"-"+std::to_string(Clock::now().time_since_epoch().count()));std::filesystem::create_directories(directory);
 auto path=(directory/"memory.proof").string();fs::Binding binding{"Unit","kind01-memory",std::string(64,'a'),Bytes{1,2,3}};
 if(mode=="verify"){
  check(argc==5,"verify requires external proof path");fs::FsVerifierIO wire(argv[4],binding);Observed io(wire);Run verifier;verifier.memory(io,n,actual,nullptr);wire.finish();
  std::cout<<"PASS independent-process verifier n="<<n<<" actual="<<actual<<" proof_bytes="<<wire.proofBytes()<<'\n';return 0;
 }
 if(mode=="range" || mode=="exp"){
  auto protocol=[&](IO&io,Random*rng){Run run;VectorFactory factory=[&](IO&live,std::string id,std::size_t a,std::size_t p,Value val){return run.oracle(live,id,a,p,val,rng);};
   if(mode=="range"){
    Value value=[actual](std::size_t i){return Fr(i<actual?i%4:0);};auto q=run.oracle(io,"integration/range",actual,n,io.prover()?value:Value{},rng);hiddenRange(io,q,2,factory,rng,"r7/integration/range");
   }else{
    Value index=[actual](std::size_t i){return Fr(i<actual?std::array<unsigned,4>{0,8190,8191,10000}[i%4]:0);};
    Value value=[actual](std::size_t i){return Fr(i<actual?r1::expValue(std::array<unsigned,4>{0,8190,8191,10000}[i%4]):0);};
    auto a=run.oracle(io,"integration/index",actual,n,io.prover()?index:Value{},rng);auto v=run.oracle(io,"integration/exp",actual,n,io.prover()?value:Value{},rng);
    hiddenExp(io,a,v,r1::ExpProfile::Unclamped,factory,rng,"r7/integration/exp");
   }
   for(auto&open:run.pending)open();
  };
  OSRandom rng;{fs::FsProverIO wire(path,binding);Observed io(wire);protocol(io,&rng);wire.finish();}
  {fs::FsVerifierIO wire(path,binding);Observed io(wire);protocol(io,nullptr);wire.finish();}
  std::cout<<"PASS integration "<<mode<<'\n';return 0;
 }
 OSRandom rng;Run p,v;double pt,vt;std::uint64_t bytes;
 {auto start=Clock::now();fs::FsProverIO wire(path,binding);Observed io(wire);p.memory(io,n,actual,&rng);wire.finish();bytes=wire.proofBytes();pt=seconds(start);}
 {auto start=Clock::now();fs::FsVerifierIO wire(path,binding);Observed io(wire);v.memory(io,n,actual,nullptr);wire.finish();vt=seconds(start);
  for(unsigned k=0;k<3;++k){auto prefix="fixture/product/"+std::to_string(k);check(io.evidence.count(prefix+"/root"),"missing independent root coverage");}
 }
 std::cout<<"RESULT {\"n\":"<<n<<",\"actual\":"<<actual<<",\"prover_seconds\":"<<pt<<",\"verifier_seconds\":"<<vt<<",\"proof_bytes\":"<<bytes<<",\"claims\":"<<p.claims<<",\"address_mle\":"<<p.opens["fixture/address"]<<",\"value_mle\":"<<p.opens["fixture/value"]<<",\"read_mle\":"<<p.opens["r7/fixture/read"]<<",\"proof\":\""<<path<<"\"}\n";
 if(mode=="shared"){
  check(p.opens["fixture/address"]==1 && p.opens["fixture/value"]==1 && p.opens["r7/fixture/read"]==1,"Kind01 must authenticate A/V/R once at shared point");
  check(v.claims==4,"batch must defer exactly A,V,R,final claims");
 }
 if(mode=="negative"){
  std::vector<std::string>attacks{"r7/fixture/root/0/handle","r7/fixture/root/1/handle",
   "r7/fixture/product/01/level/0/sumcheck/endpoint/1/commitment","r7/fixture/product/01/level/0/sumcheck/endpoint/3/commitment",
   "r7/fixture/product/01/leaf/address/handle","r7/fixture/product/01/leaf/value/handle","r7/fixture/product/01/leaf/read/handle"};
  for(unsigned b=0;b<2;++b){auto instance="r7/fixture/product/01/level/1/sumcheck/round/0/instance/"+std::to_string(b);
   attacks.push_back(instance+"/c/0/commitment");attacks.push_back(instance+"/derived");
   attacks.push_back("r7/fixture/product/01/level/1/instance/"+std::to_string(b)+"/carry");
   attacks.push_back("r7/fixture/product/01/level/1/sumcheck/terminal/product/"+std::to_string(b)+"/commitment");
  }
  for(auto&attack:attacks){fs::FsVerifierIO wire(path,binding);Observed io(wire,attack);Run bad;bool rejected=false;try{bad.memory(io,n,actual,nullptr);wire.finish();}catch(const std::exception&e){rejected=true;std::cout<<"REJECT verifier "<<attack<<": "<<e.what()<<'\n';}check(io.hit && rejected,"tamper accepted or target not reached");}
  for(auto&source:{std::string("fixture/address"),std::string("fixture/value"),std::string("r7/fixture/read")}){
   auto badpath=(directory/(source=="fixture/address"?"bad-A.proof":source=="fixture/value"?"bad-V.proof":"bad-R.proof")).string();Run bad;bad.bad_source=source;bool rejected=false;
   try{OSRandom random;{fs::FsProverIO wire(badpath,binding);Observed io(wire);bad.memory(io,n,actual,&random);wire.finish();}
    {fs::FsVerifierIO wire(badpath,binding);Observed io(wire);Run verifier;verifier.memory(io,n,actual,nullptr);wire.finish();}
   }catch(const std::exception&e){rejected=true;std::cout<<"REJECT source mismatch "<<source<<": "<<e.what()<<'\n';}check(rejected,"source commitment mismatch accepted");
  }
#ifndef KIND01_REFERENCE
  for(auto size:{1u,2u,4u,8u,16u})direct(directory,size,"");
  for(auto size:{1u,8u})for(auto fault:{"root0","root1","leaf0","leaf1","cancel"})direct(directory,size,fault);
#endif
 }
 std::cout<<"PASS "<<mode<<'\n';return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
