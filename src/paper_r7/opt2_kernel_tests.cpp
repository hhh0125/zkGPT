#include "classified_commitments.hpp"
#include <iostream>
#include <chrono>
using namespace zkgpt::r7;using namespace oneblock;using namespace classified;
namespace zkgpt::r7::oneblock::classified {
struct AuthenticationTestAccess {
 static void setup(Commitments&c,std::size_t n){
  c.roots_.resize(6);for(std::size_t k=0;k<6;++k){c.roots_[k].spec={"test/"+std::to_string(k),n,n,false,false};c.roots_[k].value=[k](std::size_t j){return Fr(1+j%29+k);};}
  for(unsigned i=0;i<48;++i){Claim a;a.id="test/"+std::to_string(i);a.padded=n;for(unsigned k=0;k<r1::log2exact(n);++k)a.point.push_back(Fr(3+(i+k)%37));a.only_root=i<24?int(i%6):-1;
   a.map=[i,n](std::size_t j)->Located{if(i<24)return {{i%6,std::uint32_t(j)},Fr(0),false};if(j>=n-16)return {{},Fr(7),true};return {{std::uint32_t((j/(n/8)+i)%6),std::uint32_t(j)},Fr(int(i%5)-2),false};};
   a.map_digest=c.mapDigest(a);c.claims_.push_back(std::move(a));
  }
 }
 static void run(Commitments&c,bool bench,bool optimized){
  std::vector<Fr>lambda;for(unsigned i=0;i<c.claims_.size();++i)lambda.push_back(Fr(i+2));
  auto start=std::chrono::steady_clock::now();auto cpu=processCPU();Fr checksum=0,bias=0;
  Commitments::Authentication prepared(c.claims_.size());
  if(optimized)for(unsigned i=0;i<c.claims_.size();++i)bias+=lambda[i]*c.prepareClaim(c.claims_[i],prepared,i);
  else for(unsigned i=0;i<c.claims_.size();++i){auto&a=c.claims_[i];need(c.mapDigest(a)==a.map_digest,"reference barrier map");if(a.only_root<0){auto w=r1::weights(a.point);for(std::size_t j=0;j<a.padded;++j)bias+=lambda[i]*w[j]*a.map(j).bias;}}
  std::vector<Fr>dots(c.roots_.size(),Fr(0));
  if(optimized)dots=c.partialDot(lambda,prepared);
  else for(unsigned k=0;k<6;++k){auto kernel=c.kernel(k,lambda);for(std::size_t j=0;j<kernel.size();++j)dots[k]+=kernel[j]*c.roots_[k].value(j);}
  for(unsigned k=0;k<6;++k){auto got=optimized?c.kernel(k,lambda,prepared):c.kernel(k,lambda);if(!bench){auto ref=c.kernel(k,lambda);need(got==ref,"optimized/reference kernel mismatch");std::vector<Fr>want(got.size(),Fr(0));for(unsigned i=0;i<c.claims_.size();++i){auto&a=c.claims_[i];auto w=r1::weights(a.point);for(std::size_t j=0;j<a.padded;++j){auto l=a.map(j);if(!l.constant && l.coordinate.root==k)want[l.coordinate.index]+=lambda[i]*w[j];}}need(want==got,"kernel dense reference mismatch");Fr dot=0;for(std::size_t j=0;j<got.size();++j)dot+=got[j]*c.roots_[k].value(j);need(dot==dots[k],"partial-dot reference mismatch");}checksum+=got[0]+dots[k];}
  // A validated snapshot is independent of later mutations of callbacks.
  if(!bench && optimized){
   Commitments::Authentication uncached(c.claims_.size());uncached.map_bytes=Commitments::Authentication::map_limit;uncached.weight_bytes=Commitments::Authentication::weight_limit;
   for(unsigned i=0;i<c.claims_.size();++i){c.prepareClaim(c.claims_[i],uncached,i);need(!uncached.claims[i].frozen && !uncached.claims[i].weights,"budget fallback retained cache");}
   for(unsigned k=0;k<6;++k)need(c.kernel(k,lambda,prepared)==c.kernel(k,lambda,uncached),"budget fallback kernel mismatch");need(c.partialDot(lambda,uncached)==dots,"budget fallback dot mismatch");
   Commitments::Authentication tight(c.claims_.size());tight.map_bytes=Commitments::Authentication::map_limit-2*sizeof(Commitments::Authentication::Run);c.prepareClaim(c.claims_[24],tight,24);need(!tight.claims[24].frozen && tight.claims[24].runs.empty(),"fragmented map exceeded cache bound");
   auto good=c.kernel(0,lambda,prepared);c.claims_[0].map=[](std::size_t)->Located{throw std::runtime_error("callback must not be reused after freeze");};need(good==c.kernel(0,lambda,prepared),"snapshot retained mutable callback");
   need(prepared.map_bytes<=Commitments::Authentication::map_limit && prepared.weight_bytes<=Commitments::Authentication::weight_limit,"cache budget exceeded");
   // Force the generic checked path and ensure it detects a changed callback.
   prepared.claims[0].frozen=false;c.claims_[0].map=[](std::size_t j)->Located{return {{0,std::uint32_t(j^1)},Fr(0),false};};bool rejected=false;try{c.kernel(0,lambda,prepared);}catch(const std::exception&e){rejected=std::string(e.what())=="classified source map changed after defer";}need(rejected,"fallback mutable map accepted");}
  if(!bench){std::vector<Fr>q(24,Fr(7));auto t=processCPU();Commitments::Weights w(q);auto precompute=processCPU()-t;Fr sum=0;t=processCPU();for(std::size_t i=0;i<(1u<<20);++i)sum+=w.at(i);need(sum.isValid(),"weight arithmetic");std::cout<<"{\"test\":\"weights\",\"precompute_cpu_seconds\":"<<precompute<<",\"at_products\":1048576,\"Fr_multiply_add_cpu_seconds\":"<<processCPU()-t<<",\"cache_bytes\":"<<w.bytes()<<"}\n";}
  std::cout<<"{\"test\":\"kernel\",\"mode\":\""<<(optimized?"optimized":"reference")<<"\",\"n\":"<<c.roots_[0].spec.padded<<",\"wall_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<",\"cpu_seconds\":"<<processCPU()-cpu<<",\"checksum\":\""<<(checksum+bias).getStr()<<"\"}\n";
 }
};
}
struct Tape:IO {bool prover()const override{return true;}std::string session()const override{return std::string(64,'a');}Digest prefix()const override{return {};}Bytes message(const std::string&,const Bytes&b={})override{return b;}Fr coin(const std::string&)override{return Fr(3);}};
int main(int argc,char**argv){try{r1::init();bool bench=argc>1;bool optimized=argc<=1 || std::string(argv[1])!="reference";Layout l;l.native=std::string(64,'a');l.roots={{"C_aux",1,1,true,false}};l.base={{0,0}};Prepared p;SessionRandom rng;Tape t;Commitments c(t,l,[](std::size_t){return Fr(1);},&p,&rng);AuthenticationTestAccess::setup(c,bench?65536:256);AuthenticationTestAccess::run(c,bench,optimized);return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
