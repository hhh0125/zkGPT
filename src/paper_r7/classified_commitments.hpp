#pragma once
#include "lookup_prepare.hpp"
#include "oneblock_vectors.hpp"
#include "oneblock_model.hpp"
namespace zkgpt::r7::oneblock::classified {
struct Claim {std::string id,map_id;Hidden value;std::vector<Fr>point;std::size_t padded=0;std::function<Located(std::size_t)>map;int only_root=-1;Digest map_digest{};};
struct Root {
 RootSpec spec;VectorPC pc;std::optional<VectorSecret>secret;Value value;
 std::function<void(IO&,const Query&,const Hidden*,Random*,const std::string&)>open;
 std::vector<G1>basis;std::vector<Fr>blinds;
};
struct AuthMetrics {std::uint64_t map_calls=0,map_coordinates=0,kernel_calls=0,kernel_coordinates=0,weights=0,weight_terms=0,weight_products=0,kernel_peak_bytes=0;double map_cpu=0,kernel_cpu=0,weights_cpu=0,partial_cpu=0;};
class Commitments {
 friend struct AuthenticationTestAccess;
 mutable AuthMetrics auth_metrics_;
 class Weights {
  std::vector<Fr>a_,b_;unsigned split_;
  public:explicit Weights(const std::vector<Fr>&,AuthMetrics* =nullptr);
  std::size_t bytes()const;Fr at(std::size_t)const;
 };
 struct Authentication {
 struct Run {std::size_t begin,count;Coordinate first;};
 struct Prepared {std::vector<Run>runs;bool frozen=false;std::unique_ptr<Weights>weights;};
 static constexpr std::size_t map_limit=16u<<20,weight_limit=32u<<20;
 const std::size_t map_budget,weight_budget;
 std::vector<Prepared>claims;std::size_t map_bytes=0,weight_bytes=0;
 explicit Authentication(std::size_t n,bool full12=false):map_budget(full12?256u<<20:map_limit),weight_budget(full12?256u<<20:weight_limit),claims(n){}
};
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
 void authenticate();
 bool authenticated()const{return authenticated_;}
 std::size_t claims()const{return claims_.size();}
 const std::vector<Root>&roots()const{return roots_;}
};
}
