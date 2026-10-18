#pragma once
#include "production_layout.hpp"
#include <ostream>

namespace route_b::production {
struct SourceId { unsigned family=0;std::size_t index=0;unsigned port=0,point=0; };
struct PCSpec {
  std::string scheme,domain,oracle,relation;
  std::size_t rows=0,columns=0;
};
struct SourceSpec {
  SourceId id;
  std::string kind,alias="unique-within-source;reuse-across-sources",padding="0",value_rule="field";
  std::size_t m=0,p=0,d=0,support=0;
  std::vector<std::size_t> shape;
  unsigned bits=0;
  bool is_signed=false;
  std::vector<Span> projection;
  PCSpec pc;
  std::vector<std::string> dependencies;
};
struct ObligationSpec {
  std::string id,kind,relation,table;
  std::string child_id,native_padding;
  std::size_t index=0,subindex=0,m=0,p=0,shift=0;
  // m remains the parent's actual length; local membership consumes the full
  // committed padded vector. dim_pc is an exact parent oracle reference when
  // deployment policy requires reuse (notably Lasso's Exp C_x address).
  std::size_t query_length=0,table_size=0;
  unsigned bits=0;
  PCSpec pc,dim_pc;
  std::vector<std::string> dependencies;
};
struct ExpectedPlan {
  std::string policy_id,model_id,implementation_profile;
  Mode mode=Mode::BridgeV1;
  PCSpec input_pc;
  std::vector<SourceSpec> sources;
  std::vector<ObligationSpec> obligations;
};
ExpectedPlan buildExpectedPlan(const TrustedDeployment&,const PublicModelLayout&,const layeredCircuit&,const WitnessRegistry&);
// This function never samples a challenge. The trusted expected side is rebuilt
// from verifier-owned objects; an artifact cannot supply its own expected plan.
void requirePlanMatchesTrusted(const TrustedDeployment&,const PublicModelLayout&,const layeredCircuit&,const WitnessRegistry&,const ExpectedPlan&);
class TrustedPlan {
public:
  TrustedPlan(const TrustedDeployment&,const PublicModelLayout&,const layeredCircuit&,const WitnessRegistry&);
  void requireMatches(const ExpectedPlan&) const;
private:
  ExpectedPlan expected_;
};
void writePlan(std::ostream&,const ExpectedPlan&);
void writeLayout(std::ostream&,const PublicModelLayout&,const layeredCircuit&);
}
