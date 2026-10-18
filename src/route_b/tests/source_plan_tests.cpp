#include "source_plan.hpp"
#include "models.hpp"
#include <functional>
#include <iostream>
using namespace route_b::production;
static void check(bool b, const char *s) { if (!b) throw std::runtime_error(s); }
int main() {
  try {
    initPairing(mcl::BN254);
    const auto deployment = TrustedDeployment::full12(Mode::BridgeV1);
    LLM network(12,12,64,768,3072);
    const auto circuit = network.buildPublicCircuit();
    const auto &registry = network.getWitnessRegistry();
    auto layout = rebuildProductionLayout(deployment,circuit,registry);
    check(layout.n0==195414944 && layout.N0==268435456 && layout.ell==28,"full12 input size");
    check(layout.ranges.regions.size()==624 && layout.ranges.queries.size()==10,"full12 range coverage");
    check(layout.exp.size()==76032 && layout.constants.size()==96 && layout.output_offset==195382176,"Exp/constants/Y");
    check(layout.constants.front()==163735552 && layout.constants.back()==195251084,"constant append order");
    auto plan = buildExpectedPlan(deployment,layout,circuit,registry);
    check(plan.sources.size()==6735,"Bridge full source catalogue");
    check(plan.obligations.size()>81+6+144,"local obligations preserved");
    auto lasso = deployment; lasso.mode=Mode::LassoV2;
    auto final = buildExpectedPlan(lasso,layout,circuit,registry);
    check(final.sources.size()==591,"Lasso full source catalogue");
    for(auto &o:final.obligations) {
      if(o.kind=="range-chunk"||o.kind=="exp-membership")
        check(o.query_length==o.p && o.native_padding=="none;parent-tail-is-member-row","Lasso covers parent padded rows");
      if(o.id=="exp/membership/0")
        check(o.pc.oracle=="exp/vector/1" && o.dim_pc.oracle=="exp/vector/3" &&
              o.child_id=="lasso/exp/E-indexed" && o.query_length==131072 && o.table_size==8192,
              "Lasso E query and exact Cx address");
    }
    for(auto &s:final.sources) if(s.id.family==2)
      for(unsigned j=0;j<6;++j)check(std::find(s.dependencies.begin(),s.dependencies.end(),"exp/membership/"+std::to_string(j))!=s.dependencies.end(),"all six Exp memberships are prerequisites");
    std::size_t endpoints=0,chunks=0;
    for(auto &s: plan.sources) if(s.id.family==0) ++endpoints;
    for(auto &o: plan.obligations) if(o.kind=="range-chunk") ++chunks;
    check(endpoints==576 && chunks==81,"endpoint/chunk coverage");
    TrustedPlan verifier(deployment,layout,circuit,registry);
    verifier.requireMatches(plan);
    std::cout << "ACCEPT expected=accept actual=accept Bridge=6735 Lasso=591 endpoints=576 chunks=81\n";
    auto reject=[&](const char *label,const std::function<void(ExpectedPlan&)> &edit) {
      auto changed=plan; edit(changed); bool rejected=false;
      try { verifier.requireMatches(changed); }
      catch(const std::exception&) { rejected=true; }
      check(rejected,label);
      std::cout<<"REJECT expected=reject actual=reject "<<label<<" challenges=0\n";
    };
    reject("missing-FCONN-p1",[](auto &p){p.sources.erase(p.sources.begin()+8);});
    reject("continuation-as-input",[](auto &p){p.sources[0].kind="continuation";});
    reject("offset",[](auto &p){++p.sources[0].projection[0].offset;});
    reject("bits",[](auto &p){++p.sources[0].bits;});
    reject("signed",[](auto &p){p.sources[0].is_signed=!p.sources[0].is_signed;});
    reject("length",[](auto &p){++p.sources[0].m;});
    reject("duplicate-ID",[](auto &p){p.sources[1].id=p.sources[0].id;});
    reject("missing-ID",[](auto &p){p.sources.erase(p.sources.begin());});
    reject("order",[](auto &p){std::swap(p.sources[0],p.sources[1]);});
    reject("model-ID",[](auto &p){p.model_id="forged";});
    reject("implementation-profile",[](auto &p){p.implementation_profile="forged";});
    reject("mapping-out-of-bounds",[](auto &p){p.sources[0].projection[0].offset=268435456;});
    reject("alias",[](auto &p){p.sources[0].alias="allow-within-source";});
    reject("missing-zero-tail",[](auto &p){p.sources.erase(p.sources.end()-3);});
    reject("missing-constant",[](auto &p){p.sources[p.sources.size()-2].projection.pop_back();});
    reject("moved-constant",[](auto &p){++p.sources[p.sources.size()-2].projection[1].offset;});
    reject("missing-output",[](auto &p){p.sources.pop_back();});
    reject("output-offset",[](auto &p){++p.sources.back().projection[0].offset;});
    reject("correct-count-wrong-semantics",[](auto &p){p.sources[0].padding="1";});
    reject("unapproved-artifact-policy",[](auto &p){p.policy_id="artifact-self-declared";});
    reject("PC-identity",[](auto &p){p.sources[0].pc.oracle="other-C0";});
    reject("local-dependency",[](auto &p){p.sources[0].dependencies.clear();});
    reject("mode-downgrade",[&](auto &p){p=final;});
    reject("obligation-omission",[](auto &p){p.obligations.erase(p.obligations.begin());});
    std::cout<<"PASS full12 source-plan unit and 24 metadata rejection cases\n";
    return 0;
  } catch(const std::exception &e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
