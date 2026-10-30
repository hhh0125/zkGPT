#pragma once
// Test-only snapshot of the optimized pre-OPT3 No-FullBase implementation.
// Keep this independent of the production Run/Weights representation.
#include "classified_commitments.hpp"
#include <openssl/evp.h>
namespace zkgpt::r7::oneblock::classified::opt3a_reference {
class MapHash {
 std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)>ctx_{EVP_MD_CTX_new(),EVP_MD_CTX_free};
 std::array<unsigned char,32768>buffer_{};std::size_t used_=0;
 void flush(){if(used_){need(EVP_DigestUpdate(ctx_.get(),buffer_.data(),used_)==1,"reference map digest update");used_=0;}}
 public:MapHash(){need(ctx_ && EVP_DigestInit_ex(ctx_.get(),EVP_sha256(),nullptr)==1,"reference digest init");}
 void append(const Located&l){if(used_+41>buffer_.size())flush();buffer_[used_++]=l.constant?1:0;for(auto n:{l.coordinate.root,l.coordinate.index})for(unsigned j=0;j<4;++j)buffer_[used_++]=std::uint8_t(n>>(8*j));need(l.bias.isValid() && l.bias.serialize(buffer_.data()+used_,32)==32,"reference canonical bias");used_+=32;}
 Digest finish(){flush();Digest d{};unsigned n=0;need(EVP_DigestFinal_ex(ctx_.get(),d.data(),&n)==1 && n==32,"reference digest end");return d;}
};
struct Weights {
 std::vector<Fr>a,b;unsigned split;
 explicit Weights(const std::vector<Fr>&q):split(q.size()/2){a=r1::weights({q.begin(),q.begin()+split});b=r1::weights({q.begin()+split,q.end()});}
 std::size_t bytes()const{return (a.capacity()+b.capacity())*sizeof(Fr);}
 Fr at(std::size_t i)const{return a[i&(a.size()-1)]*b[i>>split];}
};
struct Run {std::size_t begin,count;Coordinate first;};
struct Prepared {std::vector<Run>runs;bool frozen=false;std::unique_ptr<Weights>weights;};
struct Authentication {
 const std::size_t map_budget,weight_budget;std::vector<Prepared>claims;std::size_t map_bytes=0,weight_bytes=0;
 explicit Authentication(std::size_t n,bool full12):map_budget(full12?256u<<20:16u<<20),weight_budget(full12?256u<<20:32u<<20),claims(n){}
};
inline Fr prepare(const Claim&c,const std::vector<Root>&roots,Authentication&auth,std::size_t i){
 auto&p=auth.claims[i];auto maxRuns=(auth.map_budget-auth.map_bytes)/sizeof(Run);bool fits=maxRuns>0;
 auto weights=std::make_unique<Weights>(c.point);Fr bias=0;MapHash digest;
 for(std::size_t j=0;j<c.padded;++j){auto l=c.map(j);
  need(l.constant || (l.coordinate.root<roots.size() && l.coordinate.index<roots[l.coordinate.root].spec.actual),"reference map bounds");
  if(c.only_root>=0)need(!l.constant && l.bias.isZero() && l.coordinate.root==std::size_t(c.only_root),"reference root hint");
  digest.append(l);if(c.only_root<0 && !l.bias.isZero())bias+=weights->at(j)*l.bias;
  if(!fits || l.constant)continue;
  if(!p.runs.empty()){auto&last=p.runs.back();if(last.begin+last.count==j && last.first.root==l.coordinate.root && std::uint64_t(last.first.index)+last.count==l.coordinate.index){++last.count;continue;}}
  if(p.runs.size()==maxRuns){fits=false;std::vector<Run>().swap(p.runs);continue;}
  if(p.runs.size()==p.runs.capacity())p.runs.reserve(std::min(maxRuns,std::max<std::size_t>(8,p.runs.capacity()*2)));
  p.runs.push_back({j,1,l.coordinate});
 }
 need(digest.finish()==c.map_digest,"reference changed map");p.frozen=fits;auth.map_bytes+=p.runs.capacity()*sizeof(Run);
 if(weights->bytes()<=auth.weight_budget-auth.weight_bytes){auth.weight_bytes+=weights->bytes();p.weights=std::move(weights);}return bias;
}
inline std::vector<Fr>kernel(std::size_t k,const std::vector<Fr>&lambda,const std::vector<Root>&roots,const std::vector<Claim>&claims,const Authentication&auth){
 std::vector<Fr>result(roots.at(k).spec.padded,Fr(0));
 for(std::size_t i=0;i<claims.size();++i){auto&c=claims[i];if(c.only_root>=0 && std::size_t(c.only_root)!=k)continue;auto&p=auth.claims[i];std::unique_ptr<Weights>scratch;if(!p.weights)scratch=std::make_unique<Weights>(c.point);auto&w=p.weights?*p.weights:*scratch;
  if(p.frozen){for(auto&run:p.runs)if(run.first.root==k)for(std::size_t j=0;j<run.count;++j)result[run.first.index+j]+=lambda[i]*w.at(run.begin+j);}
  else{MapHash digest;for(std::size_t j=0;j<c.padded;++j){auto l=c.map(j);digest.append(l);if(!l.constant && l.coordinate.root==k){need(l.coordinate.index<roots[k].spec.actual,"reference kernel bounds");result[l.coordinate.index]+=lambda[i]*w.at(j);}}need(digest.finish()==c.map_digest,"reference changed map");}
 }return result;
}
inline std::vector<Fr>partialDot(const std::vector<Fr>&lambda,const std::vector<Root>&roots,const std::vector<Claim>&claims,const Authentication&auth){
 std::vector<Fr>values(roots.size(),Fr(0));
 for(std::size_t i=0;i<claims.size();++i){auto&c=claims[i];auto&p=auth.claims[i];std::unique_ptr<Weights>scratch;if(!p.weights)scratch=std::make_unique<Weights>(c.point);auto&w=p.weights?*p.weights:*scratch;
  if(p.frozen){for(auto&run:p.runs)for(std::size_t j=0;j<run.count;++j)values[run.first.root]+=lambda[i]*w.at(run.begin+j)*roots[run.first.root].value(run.first.index+j);}
  else{MapHash digest;for(std::size_t j=0;j<c.padded;++j){auto l=c.map(j);digest.append(l);if(!l.constant){need(l.coordinate.root<roots.size() && l.coordinate.index<roots[l.coordinate.root].spec.actual,"reference partial bounds");values[l.coordinate.root]+=lambda[i]*w.at(j)*roots[l.coordinate.root].value(l.coordinate.index);}}need(digest.finish()==c.map_digest,"reference changed map");}
 }return values;
}
}
