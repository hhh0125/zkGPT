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
class Commitments {
 IO&io_;const Layout&layout_;std::string identity_,session_;Random*rng_;std::vector<Root>roots_;std::vector<Claim>claims_;std::set<std::string>names_;bool sealed_=false,base_registered_=false,authenticated_=false;
 void rootOpening(std::size_t,const Query&,const Hidden*,const std::string&);
 std::vector<Fr>kernel(std::size_t,const std::vector<Fr>&)const;
 Digest mapDigest(const Claim&)const;
 public:
 std::uint64_t registration_bytes=0,registered_rows=0;
 Commitments(IO&,const Layout&,Value onlyP_base,Prepared*onlyP_prepared,Random*,const model::Trust*trust=nullptr,model::Secret*onlyP_model=nullptr);
 void registerBase();
 VectorOracle view(const std::string&);
 void defer(Claim);
 void deferBase(const std::string&,std::size_t actual,std::size_t padded,std::function<std::size_t(std::size_t)>,const Hidden&,const std::vector<Fr>&);
 void authenticate();
 bool authenticated()const{return authenticated_;}
 std::size_t claims()const{return claims_.size();}
 const std::vector<Root>&roots()const{return roots_;}
};
}
