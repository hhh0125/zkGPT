// Standalone diagnostic. Calls unchanged production APIs; no protocol patch.
#include "single_hyrax.hpp"
#include "fs_io.hpp"
#include "streaming_master.hpp"
#include <chrono>
#include <iostream>
#include <cmath>
using namespace zkgpt::r7;
using namespace oneblock;
namespace sh=oneblock::single;
struct Tape:IO {
 bool p;std::size_t at=0;std::vector<std::pair<std::string,Bytes>>data;
 fs::CanonicalTranscript transcript{{1,4,7},{2,5,8}};
 explicit Tape(bool b):p(b){}
 bool prover()const override{return p;}
 std::string session()const override{return std::string(64,'a');}
 Digest prefix()const override{return transcript.prefix();}
 Bytes message(const std::string&n,const Bytes&b={})override{Bytes out=b;if(p)data.emplace_back(n,b);else{need(at<data.size() && data[at].first==n,"diagnostic replay mismatch");out=data[at++].second;}transcript.message(n,out);return out;}
 Fr coin(const std::string&n)override{return transcript.coin(n);}
};
template<class F>void timed(const char*name,std::size_t rows,F f){auto t=std::chrono::steady_clock::now();auto c=crypto::processCPU();auto m=metrics();f();std::cout<<"{\"operation\":\""<<name<<"\",\"rows\":"<<rows<<",\"wall_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-t).count()<<",\"cpu_seconds\":"<<crypto::processCPU()-c<<",\"msm_calls\":"<<metrics().msm_calls-m.msm_calls<<",\"msm_terms\":"<<metrics().msm_terms-m.msm_terms<<"}\n";}
int main(){try{
 r1::init();SessionRandom rng;const auto H=parameters(1).hiding;
 // Synthetic inputs only. No Full12 or private model/witness is loaded.
 for(unsigned exponent:{16u,20u,24u}){
  auto plan=sh::makePlan(unified::pack({{1,std::size_t(1)<<(exponent-2)}},4),{4,1},std::string(64,'b'));
  sh::Secret secret;secret.pc={plan,std::vector<G1>(plan.rows,H)};secret.blinds.assign(plan.rows,Fr(1));secret.sources={[](std::size_t){return Fr(0);},[](std::size_t){return Fr(0);}};
  std::string id;
  timed("identity-cold",plan.rows,[&]{id=sh::identity(secret.pc);});
  timed("identity-repeat",plan.rows,[&]{need(sh::identity(secret.pc)==id,"identity unstable");});
  std::vector<Fr>q(sh::bits(plan.domain),Fr(2));const auto d=sh::bits(plan.columns);auto L=r1::weights({q.begin()+d,q.end()});
  timed("rowCombination-equivalent",plan.rows,[&]{G1 result;result.clear();for(std::size_t i=0;i<plan.rows;i+=16384){auto n=std::min<std::size_t>(16384,plan.rows-i);result+=master3::masterMSM(secret.pc.rows.data()+i,L.data()+i,n);}need(result==H,"row weights do not sum to one");});
  Tape p(true);
  timed("exchange-P",plan.rows,[&]{sh::exchange(p,plan,&secret);});
  timed("fixed-P",plan.rows,[&]{sh::constrainFixed(p,secret.pc,std::vector<Fr>(4,Fr(0)),&secret,&rng);});
  timed("opening-P",plan.rows,[&]{sh::evaluateAndOpen(p,secret.pc,q,&secret,&rng,"diagnostic/open");});
  Tape v(false);v.data=std::move(p.data);sh::Public pc;
  timed("exchange-V",plan.rows,[&]{pc=sh::exchange(v,plan,nullptr);});
  timed("fixed-V",plan.rows,[&]{sh::constrainFixed(v,pc,std::vector<Fr>(4,Fr(0)),nullptr,nullptr);});
  timed("opening-V",plan.rows,[&]{sh::evaluateAndOpen(v,pc,q,nullptr,nullptr,"diagnostic/open");});
  need(v.at==v.data.size() && v.prefix()==p.prefix(),"replayed transcript differs");
  std::cout<<"{\"operation\":\"transcript-equality\",\"rows\":"<<plan.rows<<",\"matched\":true,\"identity\":\""<<id<<"\"}\n";
 }
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
