// Test-only contemporaneous comparison against exact pre-OPT1 hot-path bodies.
#include "paper_r7/oneblock_vectors.hpp"
#include "paper_r7/streaming_master.hpp"
#include <chrono>
#include <iostream>
using namespace zkgpt::r7;using namespace oneblock;
namespace {
struct LegacyRandom:SessionRandom {
 Fr scalar(){
  need(mcl::bn::BN::param.cp.curveType==MCL_BN254 && Fr::getModulo()==r1::kModulus && mcl::bn::Fp::getModulo()=="16798108731015832284940804142231733909889187121439069848933715426072753864723" && G1::order_==Fr::getOp().mp && G1::a_.isZero() && G1::b_==G1::Fp(2) && !G1::Fp::isETHserialization() && G1::getSerializedByteSize()==32 && mcl::bn::getMapToMode()==MCL_MAP_TO_MODE_ORIGINAL,"legacy curve");
  for(unsigned i=0;i<4096;++i){auto b=bytes(32);Fr f;if(f.deserialize(b.data(),b.size())==b.size() && f.isValid() && r1::encode(f)==b)return f;}
  throw std::runtime_error("legacy rejection budget");
 }
};
G1 legacyMSM(const std::vector<G1>&g,const std::vector<Fr>&x){
 G1 out;out.clear();for(std::size_t at=0;at<g.size();at+=1024){auto n=std::min<std::size_t>(1024,g.size()-at);std::vector<G1>b(g.begin()+at,g.begin()+at+n);std::vector<Fr>v(x.begin()+at,x.begin()+at+n);G1 p;G1::mulVec(p,b.data(),v.data(),n);out+=p;}return out;
}
void wire(bool old){for(unsigned block=0;block<8;++block){Writer w;for(unsigned j=0;j<65536;++j){if(old){Fr f(j);need(f.isValid(),"legacy scalar");w.raw(r1::encode(f));}else w.scalar(Fr(j));}Reader r{w.b};for(unsigned j=0;j<65536;++j){auto f=old?r1::decodeFr(r.raw(32)):r.scalar();need(f==Fr(j),"wire equivalence");}r.end();}}
template<class F>double timed(F f){auto t=std::chrono::steady_clock::now();f();return std::chrono::duration<double>(std::chrono::steady_clock::now()-t).count();}
}
int main(){try{
 r1::init();auto g=hyrax_a3::generators(1024);SessionRandom rng;std::vector<Fr>x(1024);for(auto&f:x)f=rng.scalar();
 auto expected=legacyMSM(g,x);need(expected==master3::masterMSM(g,x),"MSM equivalence");
 for(unsigned repeat=0;repeat<7;++repeat)for(auto mode:{"random","wire","msm"}){
  double seconds[2]{};for(unsigned order=0;order<2;++order){unsigned old=(order+repeat)%2;
   seconds[old]=timed([&]{
    if(std::string(mode)=="random"){Fr sum=0;if(old){LegacyRandom r;for(unsigned i=0;i<100000;++i)sum+=r.scalar();}else{SessionRandom r;for(unsigned i=0;i<100000;++i)sum+=r.scalar();}need(!sum.isZero(),"random checksum");}
    else if(std::string(mode)=="wire")wire(old);
    else for(unsigned i=0;i<32;++i)need((old?legacyMSM(g,x):master3::masterMSM(g,x))==expected,"MSM output changed");
   });
  }
  std::cout<<"{\"mode\":\""<<mode<<"\",\"repeat\":"<<repeat<<",\"old_seconds\":"<<seconds[1]<<",\"new_seconds\":"<<seconds[0]<<"}"<<std::endl;
 }
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}}
