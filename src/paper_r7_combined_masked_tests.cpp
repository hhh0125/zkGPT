#include "paper_r7_native_masked.hpp"
#include "paper_r7_lasso_masked.hpp"
#include "paper_r7_oneblock_lookup.hpp"
#include "paper_r7_oneblock_vectors.hpp"
#include "paper_r7_fs_io.hpp"
#include <iostream>
using namespace zkgpt::r7;using namespace oneblock;
struct Count:IO {
 IO&wire;std::string attack;bool hit=false;std::size_t native=0,lasso=0;
 Count(IO&w,std::string a):wire(w),attack(std::move(a)){}
 bool prover()const override{return wire.prover();}std::string session()const override{return wire.session();}Digest prefix()const override{return wire.prefix();}
 Fr coin(const std::string&s)override{return wire.coin(s);}
 Bytes message(const std::string&s,const Bytes&b={})override{
  auto out=b;bool change=prover() && !hit && !attack.empty() && s.find(attack)!=std::string::npos;
  if(change){Reader r{out};auto scalar=r1::encode(r.scalar()+Fr(1));std::copy(scalar.begin(),scalar.end(),out.begin());hit=true;std::cerr<<"ATTACK_REACHED "<<s<<"\n";}
  auto received=wire.message(s,out);
  if(s.find("/masked/round/")!=std::string::npos && s.find("/coefficients")!=std::string::npos){if(s.find("fixture/native")!=std::string::npos)++native;else ++lasso;}
  return change?b:received;
 }
};
int main(int argc,char**argv){try{
 need(argc==4 || argc==5,"prove|verify sigma|native|both PATH [attack]");r1::init();std::string action=argv[1],mode=argv[2];bool p=action=="prove";need(p || action=="verify","action");need(mode=="sigma"||mode=="native"||mode=="both","mode");
 native_masked::ScopedMode ns(mode=="sigma"?native_masked::Mode::Sigma:native_masked::Mode::MaskedResearch);setenv("ZKGPT_LASSO_SUMCHECK",mode=="both"?"masked-research-v1":"sigma",1);masked::ScopedMode ls(masked::configuredMode());
 fs::Binding binding{mode=="both"?"Full12AllMasked":mode=="native"?"Full12NativeMasked":"Full12SingleHyrax","combined-masked-fixture-v1",fs::generatorIdentity(),Bytes{7,8,9}};
 std::unique_ptr<fs::FsIO>pipe;if(p)pipe=std::make_unique<fs::FsProverIO>(argv[3],binding);else pipe=std::make_unique<fs::FsVerifierIO>(argv[3],binding);
 Count io(*pipe,argc==5?argv[4]:"");OSRandom random;Random*rng=p?&random:nullptr;
 Value a=p?Value([](std::size_t i){return Fr(i+1);}):Value{},b=p?Value([](std::size_t i){return Fr(i%3+2);}):Value{};
 auto av=exchangeVector(io,"fixture/native/a",8,8,a,rng),bv=exchangeVector(io,"fixture/native/b",8,8,b,rng);Fr sum=0;if(p)for(unsigned i=0;i<8;++i)sum+=a(i)*b(i);auto h=exchangeHidden(io,"fixture/native/initial",sum,rng);
 auto reduction=mode!="sigma"?native_masked::matrix(io,8,h,p?std::vector<Value>{a,b}:std::vector<Value>{},rng,"fixture/native"):expressionSumcheck(io,8,h,p?std::vector<Value>{a,b}:std::vector<Value>{},Expression{2,{{Fr(1),{0,1}}}},rng,"fixture/native");
 av.open(io,{reduction.point,reduction.endpoints[0].handle},p?&reduction.endpoints[0]:nullptr,rng,"fixture/native/source/a");bv.open(io,{reduction.point,reduction.endpoints[1].handle},p?&reduction.endpoints[1]:nullptr,rng,"fixture/native/source/b");
 Value addresses=p?Value([](std::size_t i){return Fr(i%4);}):Value{},values=p?Value([](std::size_t i){return Fr(3*(i%4));}):Value{};
 auto aa=exchangeVector(io,"fixture/lasso/address",8,8,addresses,rng),vv=exchangeVector(io,"fixture/lasso/value",8,8,values,rng);
 VectorFactory factory=[&](IO&live,std::string id,std::size_t actual,std::size_t n,Value v){return exchangeVector(live,id,actual,n,v,rng);};
 hiddenMemory(io,aa,vv,{Fr(0),Fr(3),Fr(6),Fr(9)},factory,rng,"fixture/lasso");
 pipe->finish();need(mode=="sigma"?io.native==0:io.native>0,"Native path mismatch");need(mode=="both"?io.lasso>0:io.lasso==0,"Lasso path mismatch");
 if(argc==5)need(io.hit,"attack not reached");
 std::cout<<"PASS "<<action<<" "<<mode<<" native_masked_rounds="<<io.native<<" lasso_masked_rounds="<<io.lasso<<"\n";return 0;
 }catch(const std::exception&e){std::cerr<<"REJECT "<<e.what()<<"\n";return 1;}}
