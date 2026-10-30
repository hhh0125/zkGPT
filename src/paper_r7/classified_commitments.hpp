#pragma once
#include "lookup_prepare.hpp"
#include "oneblock_vectors.hpp"
#include "oneblock_model.hpp"
#include "classified_mapping_plan.hpp"
namespace zkgpt::r7::oneblock::classified {
struct Claim {std::string id,map_id;Hidden value;std::vector<Fr>point;std::size_t padded=0;std::function<Located(std::size_t)>map;int only_root=-1;Digest map_digest{};};
struct Root {
 RootSpec spec;VectorPC pc;std::optional<VectorSecret>secret;Value value;
 std::function<void(IO&,const Query&,const Hidden*,Random*,const std::string&)>open;
 std::vector<G1>basis;std::vector<Fr>blinds;
};
struct AuthMetrics {std::uint64_t map_calls=0,map_coordinates=0,kernel_calls=0,kernel_coordinates=0,weights=0,weight_terms=0,weight_products=0,kernel_peak_bytes=0;
 std::uint64_t rowmajor_coordinate_lookups=0,kernel_matched=0,partial_coordinates=0,fallback_map_queries=0,frozen_coordinates=0,fr_multiplications=0,fr_additions=0,cpp_allocations=0,cpp_allocated_bytes=0;
 double map_cpu=0,kernel_cpu=0,weights_cpu=0,partial_cpu=0,kernel_wall=0,partial_wall=0,map_wall=0;};
std::string unifiedInputIdentity(const Layout&,const std::vector<Fr>&public_x);
class Commitments {
 friend struct AuthenticationTestAccess;
 mutable AuthMetrics auth_metrics_;
 class Weights {
  std::vector<Fr>a_,b_;unsigned split_;
  public:explicit Weights(const std::vector<Fr>&,AuthMetrics* =nullptr);
  std::size_t bytes()const;Fr at(std::size_t)const;
  // Logical intervals share b[high]; lambda is supplied only after its FS draw.
  template<class F>std::size_t scaled(std::size_t begin,std::size_t count,const Fr&lambda,F&&consume,std::size_t step=1)const{
   if(step!=1){std::size_t groups=0,previous=std::size_t(-1);Fr factor;for(std::size_t j=0;j<count;++j,begin+=step){auto high=begin>>split_;if(high!=previous){factor=lambda*b_[high];previous=high;++groups;}consume(factor*a_[begin&(a_.size()-1)]);}return groups;}
   std::size_t groups=0,end=begin+count;
   while(begin<end){auto high=begin>>split_;auto stop=std::min(end,(high+1)*a_.size());Fr factor=lambda*b_[high];++groups;
    for(;begin<stop;++begin)consume(factor*a_[begin&(a_.size()-1)]);
   }return groups;
  }
 };
 struct Authentication {
 using Run=mapping::Run;
 struct Prepared:mapping::Plan {std::unique_ptr<Weights>weights;};
 static constexpr std::size_t map_limit=16u<<20,weight_limit=32u<<20;
 const std::size_t map_budget,weight_budget;
 std::vector<Prepared>claims;std::size_t map_bytes=0,weight_bytes=0;
 mapping::Store store;
 explicit Authentication(std::size_t n,bool full12=false):map_budget(full12?256u<<20:map_limit),weight_budget(full12?256u<<20:weight_limit),claims(n),store(4*map_budget){}
};
 bool usesBaseCoordinates(const Claim&)const;
 Fr prepareClaim(const Claim&,Authentication&,std::size_t)const;
 std::vector<Fr>kernel(std::size_t,const std::vector<Fr>&,const Authentication&)const;
 std::vector<Fr>partialDot(const std::vector<Fr>&,const Authentication&)const;
 IO&io_;const Layout&layout_;std::string identity_,session_;Random*rng_;std::vector<Root>roots_;std::vector<Claim>claims_;std::set<std::string>names_;bool sealed_=false,tails_constrained_=false,authenticated_=false;
 void rootOpening(std::size_t,const Query&,const Hidden*,const std::string&);
 std::vector<Fr>kernel(std::size_t,const std::vector<Fr>&)const;
 Digest mapDigest(const Claim&)const;
 public:
 // Full base row knowledge registration is disabled; retain truthful legacy metrics.
 static constexpr std::uint64_t registration_bytes=0,registered_rows=0;
 std::uint64_t tail_response_bytes=0,tail_rows=0;
 Commitments(IO&,const Layout&,Value onlyP_base,Prepared*onlyP_prepared,Random*,const model::Trust*trust=nullptr,model::Secret*onlyP_model=nullptr,const std::vector<model::Trust>*chunks=nullptr,std::vector<model::Secret>*onlyP_chunks=nullptr);
 ~Commitments();
 void constrainBaseTails();
 VectorOracle view(const std::string&);
 void defer(Claim);
 void deferBase(const std::string&,std::size_t actual,std::size_t padded,std::function<std::size_t(std::size_t)>,const Hidden&,const std::vector<Fr>&);
 // Legacy OPT3-B barrier, retained for explicit reference/unit/probe targets.
 // Current complete Oneblock/Full12 entrypoints use the unified path below.
 void authenticate();
 // Protocol1(6),(7) candidate; real Oneblock replacement gate passed.
 void authenticateUnifiedInput(const std::vector<Fr>&public_x);
 bool authenticated()const{return authenticated_;}
 std::size_t claims()const{return claims_.size();}
 const std::vector<Root>&roots()const{return roots_;}
};
}
