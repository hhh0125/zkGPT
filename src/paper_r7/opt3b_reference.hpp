#pragma once
// Test-only exact OPT3-A snapshot from BASELINE_SOURCE_SHA.json. No production use.
#include "opt3a_reference.hpp"
#include <algorithm>
#include <iostream>
namespace zkgpt::r7::oneblock::classified::opt3b_reference {
using MapHash=opt3a_reference::MapHash;
using Clock=std::chrono::steady_clock;
struct Engine {
 const std::vector<Root>&roots_;const std::vector<Claim>&claims_;mutable AuthMetrics auth_metrics_;
 std::function<void(std::size_t,const Located&)>observe;
 Engine(const std::vector<Root>&r,const std::vector<Claim>&c):roots_(r),claims_(c){}
 bool usesBaseCoordinates(const Claim&)const{return false;}
 class Weights {
  std::vector<Fr>a_,b_;unsigned split_;
  public:explicit Weights(const std::vector<Fr>&,AuthMetrics* =nullptr);
  std::size_t bytes()const;Fr at(std::size_t)const;
  // Logical intervals share b[high]; lambda is supplied only after its FS draw.
  template<class F>std::size_t scaled(std::size_t begin,std::size_t count,const Fr&lambda,F&&consume)const{
   std::size_t groups=0,end=begin+count;
   while(begin<end){auto high=begin>>split_;auto stop=std::min(end,(high+1)*a_.size());Fr factor=lambda*b_[high];++groups;
    for(;begin<stop;++begin)consume(factor*a_[begin&(a_.size()-1)]);
   }return groups;
  }
 };
 struct Authentication {
 struct Run {std::size_t begin,count;Coordinate first;std::int64_t stride=0;};
 struct Prepared {std::vector<Run>runs;bool frozen=false;std::unique_ptr<Weights>weights;};
 static constexpr std::size_t map_limit=16u<<20,weight_limit=32u<<20;
 const std::size_t map_budget,weight_budget;
 std::vector<Prepared>claims;std::size_t map_bytes=0,weight_bytes=0;
 explicit Authentication(std::size_t n,bool full12=false):map_budget(full12?256u<<20:map_limit),weight_budget(full12?256u<<20:weight_limit),claims(n){}
};

 Fr prepareClaim(const Claim&,Authentication&,std::size_t)const;
 std::vector<Fr>kernel(std::size_t,const std::vector<Fr>&,const Authentication&)const;
 std::vector<Fr>partialDot(const std::vector<Fr>&,const Authentication&)const;
};
Engine::Weights::Weights(const std::vector<Fr>&q,AuthMetrics*m):split_(q.size()/2){auto t=processCPU();a_=r1::weights({q.begin(),q.begin()+split_});b_=r1::weights({q.begin()+split_,q.end()});if(m){++m->weights;m->weight_terms+=a_.size()+b_.size();m->weights_cpu+=processCPU()-t;}}
std::size_t Engine::Weights::bytes()const{return (a_.capacity()+b_.capacity())*sizeof(Fr);}
Fr Engine::Weights::at(std::size_t i)const{return a_[i&(a_.size()-1)]*b_[i>>split_];}

Fr Engine::prepareClaim(const Claim&c,Authentication&auth,std::size_t i)const{
 auto started=processCPU();auto wall=Clock::now();++auth_metrics_.map_calls;auth_metrics_.map_coordinates+=c.padded;
 auto&p=auth.claims[i];auto maxRuns=(auth.map_budget-auth.map_bytes)/sizeof(Authentication::Run);bool fits=maxRuns>0;
 auto weights=std::make_unique<Weights>(c.point,&auth_metrics_);Fr bias=0;MapHash digest;const bool baseMapped=usesBaseCoordinates(c);
 for(std::size_t j=0;j<c.padded;++j){auto l=c.map(j);auth_metrics_.rowmajor_coordinate_lookups+=baseMapped && !l.constant;
  need(l.constant || (l.coordinate.root<roots_.size() && l.coordinate.index<roots_[l.coordinate.root].spec.actual),"classified map root/coordinate bounds");
  if(c.only_root>=0)need(!l.constant && l.bias.isZero() && l.coordinate.root==std::size_t(c.only_root),"classified invalid root-only map hint");
  if(observe)observe(j,l);digest.append(l);if(c.only_root<0 && !l.bias.isZero()){bias+=weights->at(j)*l.bias;++auth_metrics_.weight_products;}
  if(!fits || l.constant)continue;
  if(!p.runs.empty()){auto&last=p.runs.back();
   if(last.begin+last.count==j && last.first.root==l.coordinate.root){
    auto delta=std::int64_t(l.coordinate.index)-last.first.index;
    if(last.count==1){last.stride=delta;++last.count;continue;}
    if(delta==std::int64_t(last.count)*last.stride){++last.count;continue;}
   }
  }
  if(p.runs.size()==maxRuns){fits=false;std::vector<Authentication::Run>().swap(p.runs);continue;}
  if(p.runs.size()==p.runs.capacity())p.runs.reserve(std::min(maxRuns,std::max<std::size_t>(8,p.runs.capacity()*2)));
  p.runs.push_back({j,1,l.coordinate});
 }
 need(digest.finish()==c.map_digest,"classified source map changed after defer");
 p.frozen=fits;
 // Sort only fully validated immutable coordinates, retaining logical weight indices.
 if(fits)std::sort(p.runs.begin(),p.runs.end(),[](const auto&a,const auto&b){return a.first.root<b.first.root || (a.first.root==b.first.root && a.begin<b.begin);});
 auth.map_bytes+=p.runs.capacity()*sizeof(Authentication::Run);
 if(weights->bytes()<=auth.weight_budget-auth.weight_bytes){auth.weight_bytes+=weights->bytes();p.weights=std::move(weights);}
 auth_metrics_.map_cpu+=processCPU()-started;auth_metrics_.map_wall+=std::chrono::duration<double>(Clock::now()-wall).count();return bias;
}
std::vector<Fr>Engine::kernel(std::size_t k,const std::vector<Fr>&lambda,const Authentication&auth)const{
 auto started=processCPU();auto wall=Clock::now();crypto::AllocationTrace allocation;++auth_metrics_.kernel_calls;auto n=roots_.at(k).spec.padded;auth_metrics_.kernel_peak_bytes=std::max<std::uint64_t>(auth_metrics_.kernel_peak_bytes,n*sizeof(Fr));std::vector<Fr>result(n,Fr(0));std::uint64_t visited=0,matched=0,multiplies=0,fallback=0,frozen=0;
 for(std::size_t i=0;i<claims_.size();++i){auto&c=claims_[i];if(c.only_root>=0 && std::size_t(c.only_root)!=k)continue;auto&p=auth.claims[i];
  auto first=std::lower_bound(p.runs.begin(),p.runs.end(),k,[](const auto&r,std::size_t root){return r.first.root<root;});
  if(p.frozen && (first==p.runs.end() || first->first.root!=k))continue;
  std::unique_ptr<Weights>scratch;if(!p.weights)scratch=std::make_unique<Weights>(c.point,&auth_metrics_);auto&w=p.weights?*p.weights:*scratch;
  if(p.frozen){for(auto it=first;it!=p.runs.end() && it->first.root==k;++it){auto&run=*it;std::int64_t at=run.first.index;
    auto groups=w.scaled(run.begin,run.count,lambda[i],[&](const Fr&weight){result[std::size_t(at)]+=weight;at+=run.stride;});
    visited+=run.count;matched+=run.count;frozen+=run.count;multiplies+=run.count+groups;
   }}
  else{MapHash digest;const bool baseMapped=usesBaseCoordinates(c);for(std::size_t j=0;j<c.padded;++j){auto l=c.map(j);auth_metrics_.rowmajor_coordinate_lookups+=baseMapped && !l.constant;digest.append(l);++visited;++fallback;if(!l.constant && l.coordinate.root==k){need(l.coordinate.index<roots_[k].spec.actual,"classified mapped coordinate outside actual root");result[l.coordinate.index]+=lambda[i]*w.at(j);++matched;multiplies+=2;}}need(digest.finish()==c.map_digest,"classified source map changed after defer");}
 }
 auth_metrics_.cpp_allocations+=allocation.count();auth_metrics_.cpp_allocated_bytes+=allocation.bytes();
 auto cpu=processCPU()-started;auto seconds=std::chrono::duration<double>(Clock::now()-wall).count();
 auth_metrics_.kernel_coordinates+=visited;auth_metrics_.kernel_matched+=matched;auth_metrics_.frozen_coordinates+=frozen;auth_metrics_.fallback_map_queries+=fallback;auth_metrics_.fr_multiplications+=multiplies;auth_metrics_.fr_additions+=matched;auth_metrics_.weight_products+=matched;auth_metrics_.kernel_cpu+=cpu;auth_metrics_.kernel_wall+=seconds;
 if(opt2MetricsEnabled())std::cerr<<"OPT2_KERNEL {\"root\":"<<k<<",\"coordinates\":"<<visited<<",\"matched_coordinates\":"<<matched<<",\"fallback_map_queries\":"<<fallback<<",\"Fr_multiplications\":"<<multiplies<<",\"Fr_additions\":"<<matched<<",\"wall_seconds\":"<<seconds<<",\"cpu_seconds\":"<<cpu<<"}\n";
 return result;
}
std::vector<Fr>Engine::partialDot(const std::vector<Fr>&lambda,const Authentication&auth)const{
 auto started=processCPU();auto wall=Clock::now();crypto::AllocationTrace allocation;std::vector<Fr>values(roots_.size(),Fr(0));std::uint64_t visited=0,matched=0,multiplies=0,fallback=0;
 for(std::size_t i=0;i<claims_.size();++i){auto&c=claims_[i];auto&p=auth.claims[i];std::unique_ptr<Weights>scratch;if(!p.weights)scratch=std::make_unique<Weights>(c.point,&auth_metrics_);auto&w=p.weights?*p.weights:*scratch;
  if(p.frozen){for(auto&run:p.runs){std::int64_t at=run.first.index;
    auto groups=w.scaled(run.begin,run.count,lambda[i],[&](const Fr&weight){values[run.first.root]+=weight*roots_[run.first.root].value(std::size_t(at));at+=run.stride;});
    visited+=run.count;matched+=run.count;multiplies+=2*run.count+groups;
   }}
  else{MapHash digest;const bool baseMapped=usesBaseCoordinates(c);for(std::size_t j=0;j<c.padded;++j){auto l=c.map(j);auth_metrics_.rowmajor_coordinate_lookups+=baseMapped && !l.constant;digest.append(l);++visited;++fallback;if(!l.constant){need(l.coordinate.root<roots_.size() && l.coordinate.index<roots_[l.coordinate.root].spec.actual,"classified partial map bounds");values[l.coordinate.root]+=lambda[i]*w.at(j)*roots_[l.coordinate.root].value(l.coordinate.index);++matched;multiplies+=3;}}need(digest.finish()==c.map_digest,"classified source map changed after defer");}
 }
 auth_metrics_.cpp_allocations+=allocation.count();auth_metrics_.cpp_allocated_bytes+=allocation.bytes();
 auto cpu=processCPU()-started;auto seconds=std::chrono::duration<double>(Clock::now()-wall).count();auth_metrics_.partial_cpu+=cpu;auth_metrics_.partial_wall+=seconds;auth_metrics_.partial_coordinates+=visited;auth_metrics_.weight_products+=matched;auth_metrics_.fallback_map_queries+=fallback;auth_metrics_.fr_multiplications+=multiplies;auth_metrics_.fr_additions+=matched;
 if(opt2MetricsEnabled())std::cerr<<"OPT2_PARTIAL {\"coordinates\":"<<visited<<",\"matched_coordinates\":"<<matched<<",\"fallback_map_queries\":"<<fallback<<",\"Fr_multiplications\":"<<multiplies<<",\"Fr_additions\":"<<matched<<",\"wall_seconds\":"<<seconds<<",\"cpu_seconds\":"<<cpu<<",\"map_cache_bytes\":"<<auth.map_bytes<<",\"weight_cache_bytes\":"<<auth.weight_bytes<<"}\n";
 return values;
}
}
