// Existing real A.3 fixture, deterministic entropy and complete event tape.
#include "product_leaf_fixture.inc"
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

namespace {
void singleFingerprintScan(){
 const std::size_t n=8;const std::vector<Fr> table{Fr(7),Fr(11),Fr(5),Fr(19)};
 WeightRandom rng;EventTape io(true);auto make=factory(rng);
 std::size_t aReads=0,vReads=0,readReads=0;
 auto a=make(io,"scan/address",n,n,[](std::size_t j){return Fr(j%4);});
 auto v=make(io,"scan/value",n,n,[&](std::size_t j){return table[j%4];});
 auto av=a.value,vv=v.value;
 a.value=[&](std::size_t j){++aReads;return av(j);};
 v.value=[&](std::size_t j){++vReads;return vv(j);};
 VectorFactory counted=[&](IO& live,std::string id,std::size_t actual,std::size_t padded,Value values){
  auto out=make(live,id,actual,padded,values);
  if(live.prover() && id=="scan/memory/read"){
   auto raw=out.value;out.value=[&,raw](std::size_t j){++readReads;return raw(j);};
  }return out;
 };
 hiddenMemory(io,a,v,table,counted,&rng,"scan/memory");
 // One witness-building pass, one fingerprint pass, two independent MLEs.
 need(aReads==4*n && vReads==4*n && readReads==3*n,
      ("Product 0/1 repeated fingerprint witness scans: a="+std::to_string(aReads)+" v="+std::to_string(vReads)+" read="+std::to_string(readReads)).c_str());
}
void exactLeafTrees(){
 for(std::size_t n:{1u,2u,4u,8u,16u,1024u})for(auto g:{Fr(0),Fr(1),Fr(-1),Fr(17)}){
  std::array<std::vector<Fr>,2>oldLeaves,newLeaves;
  for(auto&x:oldLeaves)x.resize(n);for(auto&x:newLeaves)x.resize(n);
  Fr old0=1,old1=1,new0=1,new1=1;
  for(std::size_t i=0;i<n;++i){Fr a=Fr(int(i%7)-3),v=Fr(int(i%13)-8),read=Fr(i%5),tau=Fr(19);
   oldLeaves[0][i]=g*g*a+g*v+(read+Fr(1))-tau;oldLeaves[1][i]=g*g*a+g*v+read-tau;
   auto z=fingerprint(a,v,read,g,g*g,tau);newLeaves[0][i]=z+Fr(1);newLeaves[1][i]=z;
   old0*=oldLeaves[0][i];old1*=oldLeaves[1][i];new0*=z+Fr(1);new1*=z;
  }
  need(old0==new0 && old1==new1,"exact leaf root equality");
  for(unsigned kind=0;kind<2;++kind){auto old=oldLeaves[kind],fresh=newLeaves[kind];
   for(auto width=n;;width/=2){need(old==fresh,"every Product tree layer must match");if(width==1)break;
    for(std::size_t j=0;j<width/2;++j){old[j]=old[2*j]*old[2*j+1];fresh[j]=fresh[2*j]*fresh[2*j+1];}old.resize(width/2);fresh.resize(width/2);
   }
  }
 }
}
void preparedProductEquivalence(){
 using zkgpt::r7::perf5::context;
 for(const char*c2:{"0","1"})for(std::size_t n:{1u,2u,4u,8u,16u,1024u}){
  setenv("ZKGPT_PERF6_C2",c2,1);
  auto base=std::make_shared<std::vector<Fr>>(n);for(std::size_t i=0;i<n;++i)base->at(i)=Fr(int(i%23)-11);
  const auto original=*base;const auto*pointer=base->data();
  for(unsigned kind=0;kind<2;++kind){
   WeightRandom oldR,newR;EventTape oldIO(true),newIO(true);Fr root=1;for(auto x:original)root*=x+Fr(kind==0?1:0);
   auto oldRoot=hide(oldIO.session(),"prepared/root",root,oldR),newRoot=hide(newIO.session(),"prepared/root",root,newR);
   auto auth=[&](IO&io,const Query&q,const Hidden*h,Random*,std::string){if(io.prover()){
    auto expected=r1::mle(original,q.point)+Fr(kind==0?1:0);need(h && h->value==expected,"prepared Product authenticated endpoint");
   }};
   Value oldLeaf=[&](std::size_t j){return original[j]+Fr(kind==0?1:0);};
   zkgpt::r7::leaf_reference::referenceProduct(oldIO,n,oldRoot,oldLeaf,auth,&oldR,"prepared/product");
   zkgpt::r7::perf5::Context profile;context=&profile;
   try{
    auto execute=[&]{hiddenProduct(newIO,n,newRoot,[](std::size_t)->Fr{throw std::runtime_error("prepared path called callback");},auth,&newR,"prepared/product",base);};
    if(kind==0){AddOneGuard guard(*base);execute();}else execute();
   }catch(...){context=nullptr;throw;}context=nullptr;
   need(base.use_count()==1 && base->data()==pointer && *base==original,"shared leaf storage did not survive Product intact");
   need(profile.stats[zkgpt::r7::perf5::TreeLeaf].calls==0,"prepared Product allocated/copied callback leaves");
   need(profile.stats[zkgpt::r7::perf5::TreeParent].calls==r1::log2exact(n),"parent calculations changed");
   need(oldIO.events==newIO.events && oldR.counter==newR.counter,"prepared Product layer/endpoint/handle/challenge/RNG equivalence");
   EventTape judge(false);judge.messages=newIO.messages;auto vr=newRoot;vr.value=Fr(0);vr.blind=Fr(0);
   hiddenProduct(judge,n,vr,{},auth,nullptr,"prepared/product");need(judge.pos==judge.messages.size(),"prepared proof independent witness-free verifier");
  }
 }
 unsetenv("ZKGPT_PERF6_C2");
}
void invalidPreparedAndExceptions(){
 WeightRandom rng;EventTape io(true);auto root=hide(io.session(),"bad/root",Fr(2),rng);
 LeafAuthenticator auth=[](IO&,const Query&,const Hidden*,Random*,std::string){};
 for(auto n:{0u,1u,3u}){
  auto bad=std::make_shared<std::vector<Fr>>(n,Fr(2));bool rejected=false;
  try{hiddenProduct(io,2,root,{},auth,&rng,"bad/product",bad);}catch(const std::exception&e){rejected=std::string(e.what()).find("prepared leaves")!=std::string::npos;}
  need(rejected && io.messages.empty(),"invalid prepared size must fail before messages");
 }
 for(std::size_t n:{1u,2u,1024u}){
  auto base=std::make_shared<std::vector<Fr>>(n,Fr(2));auto original=*base;bool threw=false;
  LeafAuthenticator interrupt=[](IO&,const Query&,const Hidden*,Random*,std::string){throw std::runtime_error("injected leaf authentication failure");};
  try{AddOneGuard guard(*base);auto r=hide(io.session(),"interrupt/root",r1::mle(std::vector<Fr>{Fr(3)},{}),rng);r.value=1;for(auto x:*base)r.value*=x;r=hide(io.session(),"interrupt/root",r.value,rng);hiddenProduct(io,n,r,{},interrupt,&rng,"interrupt/product",base);}
  catch(const std::exception&e){threw=std::string(e.what())=="injected leaf authentication failure";}
  need(threw && *base==original && base.use_count()==1,"RAII failed to restore interrupted Product 0");
  auto r=hide(io.session(),"reuse/root",Fr(1),rng);r.value=Fr(1);for(auto x:*base)r.value*=x;r=hide(io.session(),"reuse/root",r.value,rng);
  hiddenProduct(io,n,r,{},auth,&rng,"reuse/product",base);need(*base==original && base.use_count()==1,"Product 1 could not reuse after exception");
 }
 // Extra aliases are rejected by the exact existing ownership invariant.
 auto base=std::make_shared<std::vector<Fr>>(2,Fr(2));auto alias=base;auto r=hide(io.session(),"alias/root",Fr(4),rng);bool rejected=false;
 zkgpt::r7::perf5::Context profile;zkgpt::r7::perf5::context=&profile;
 try{hiddenProduct(io,2,r,{},auth,&rng,"alias/product",base);}catch(const std::exception&e){rejected=std::string(e.what()).find("final tree owner")!=std::string::npos;}
 zkgpt::r7::perf5::context=nullptr;need(rejected,"prepared ownership check was globally relaxed");
}
void postWeightMemoryEquivalence(){
 const std::vector<Fr>table{Fr(7),Fr(11),Fr(5),Fr(19)};
 for(const char*c2:{"0","1"})for(std::size_t n:{1u,2u,8u,16u}){
  setenv("ZKGPT_PERF6_C2",c2,1);WeightRandom oldR,newR;EventTape oldIO(true),newIO(true);
  auto execute=[&](EventTape&live,WeightRandom&random,bool reference){auto make=factory(random);auto a=make(live,"post/address",n,n,[](std::size_t j){return Fr(j%4);});auto v=make(live,"post/value",n,n,[&](std::size_t j){return table[j%4];});
   if(reference)zkgpt::r7::leaf_reference::referenceMemory(live,a,v,table,make,live.prover()?&random:nullptr,"post/memory",false);else hiddenMemory(live,a,v,table,make,live.prover()?&random:nullptr,"post/memory");};
  execute(oldIO,oldR,true);execute(newIO,newR,false);need(oldIO.events==newIO.events && oldR.counter==newR.counter,"post-weight complete Memory transcript/RNG mismatch");
  EventTape ov(false),nv(false);ov.messages=oldIO.messages;nv.messages=newIO.messages;WeightRandom vr1,vr2;execute(ov,vr1,true);execute(nv,vr2,false);
  need(ov.events==nv.events && nv.pos==nv.messages.size(),"post-weight verifier Evidence/transcript mismatch");
 }
 unsetenv("ZKGPT_PERF6_C2");
}
// Throws while f's child captures and, for C2, ProductInputs are still live.
void liveLayerExceptionRestoration(){
 struct InterruptIO final:IO {
  EventTape tape{true};std::string match;bool armed=true;
  bool prover()const override{return true;}std::string session()const override{return tape.session();}Digest prefix()const override{return tape.prefix();}
  Bytes message(const std::string&role,const Bytes&b={})override{if(armed && role.find(match)!=std::string::npos)throw std::runtime_error("injected live Product layer failure");return tape.message(role,b);}
  Fr coin(const std::string&role)override{if(armed && role.find(match)!=std::string::npos)throw std::runtime_error("injected live Product layer failure");return tape.coin(role);}
 };
 for(const char*c2:{"0","1"})for(const char*where:{"/level/3/sumcheck/","/level/9/fold"}){
  setenv("ZKGPT_PERF6_C2",c2,1);InterruptIO io;io.match=where;WeightRandom rng;
  auto base=std::make_shared<std::vector<Fr>>(1024,Fr(2));const auto original=*base;bool caught=false;
  Fr value=1;for(auto x:original)value*=x+Fr(1);auto root=hide(io.session(),"live/root",value,rng);
  LeafAuthenticator auth=[](IO&,const Query&,const Hidden*,Random*,std::string){};
  zkgpt::r7::perf5::Context profile;zkgpt::r7::perf5::context=&profile;
  try{AddOneGuard guard(*base);hiddenProduct(io,base->size(),root,{},auth,&rng,"live/product",base);}
  catch(const std::exception&e){caught=std::string(e.what())=="injected live Product layer failure";}
  zkgpt::r7::perf5::context=nullptr;
  need(caught && base.use_count()==1 && *base==original,"live layer unwind retained child owners or shifted data");
  io.armed=false;value=1;for(auto x:original)value*=x;root=hide(io.session(),"live/reuse-root",value,rng);
  hiddenProduct(io,base->size(),root,{},auth,&rng,"live/reuse",base);
  need(base.use_count()==1 && *base==original,"Product 1 reuse after live-layer exception failed");
 }
 unsetenv("ZKGPT_PERF6_C2");
}
}
int main(){try{r1::init();singleFingerprintScan();exactLeafTrees();preparedProductEquivalence();invalidPreparedAndExceptions();liveLayerExceptionRestoration();postWeightMemoryEquivalence();memoryTranscriptEquivalence();verifierWeights();std::cout<<"PASS single fingerprint scan; exact leaf/root/all tree layers n=1,2,4,8,16,1024; prepared alias/callback/size checks; C2 off/on; normal, leaf-auth and live-layer exceptional RAII reuse; real A.3 complete post-weight Memory messages/challenges/handles/endpoints/Sigma/openings/Evidence and entropy; retained zero Verifier weights\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
