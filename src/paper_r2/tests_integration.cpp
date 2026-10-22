#include "protocol.hpp"
#include "tests/gkr_test_fixture.hpp"
#include "wire.hpp"
#include "hostile_integration.hpp"
#include <functional>
#include <iostream>
namespace r1=zkgpt::r1;
using namespace zkgpt::r2;
static void rejects(const std::function<void()> &f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}r1::require(bad,"integration attack accepted");}
int main(int argc,char **argv){try{
 r1::init();gkr_test::SmallFconnFixture f;const auto &w=f.proverInstance().val[0];
 auto root=r1::PCSpec::make("r2/gkr-master",32,32,r1::Layout::RowMajor);
 auto q=r1::PCSpec::make("r2/gkr-range",1,1,r1::Layout::ColumnMajor);
 auto a=r1::PCSpec::make("r2/gkr-t",1,1,r1::Layout::ColumnMajor);
 auto e=r1::PCSpec::make("r2/gkr-E",1,1,r1::Layout::RowMajor);
 std::vector<std::vector<Fr>>v{{Fr(3)},{Fr(0)},{Fr(1048576)}};
 r1::LookupBatchSpec s;
 s.input={"R2-four-FCONN-integration-fixture-not-GPT2",r1::commit(root,w),
  {{r1::commit(q,v[0]),{{0,1,Fr(0)}}},{r1::commit(a,v[1]),{{1,1,Fr(0)}}},
   {r1::commit(e,v[2]),{{2,1,Fr(1048576)}}}},{{0,Fr(3)},{4,Fr(1)}}};
 s.ranges={{0,"r2/GKRrange",4}};s.exps={{1,2,"r2/GKRexp"}};
 auto g=f.statement();
 auto p=proveR2(f.proverInstance(),g,s,w,v);
 r1::require(g.integration_context!=std::array<std::uint8_t,32>{},"new GKR context required");
 auto replay=p;replay.gkr=f.proof();
 rejects([&]{verifyR2(f.circuit(),g,s,replay);});
 auto malformed=g;malformed.val0_log_size=64;
 rejects([&]{verifyR2(f.circuit(),malformed,s,p);});
 auto detached=s;auto other=v;other[0][0]=Fr(4);
 detached.input.children[0].commitment=r1::commit(q,other[0]);
 auto dg=g;auto detached_proof=testing::hostileR2(f.proverInstance(),dg,detached,w,other);
 rejects([&]{verifyR2(f.circuit(),dg,detached,detached_proof);});
 auto bytes=serializeProof(g,s,p);
 auto parsed=parseStatement(serializeStatement(g,s));
 r1::require(parsed.first.integration_context==g.integration_context,"GKR seed wire roundtrip");
 auto v1=serializeStatement(g,s);v1.at(15)=1;rejects([&]{parseStatement(v1);});
 auto decoded=parseProof(parsed.first,parsed.second,bytes);
 verifyR2(f.circuit(),parsed.first,parsed.second,decoded);
 if(argc==2){
  auto save=[&](const std::string &n,const GKRPublicStatement &g,const r1::LookupBatchSpec &ss,const Proof &pp){r1::writeFile(std::string(argv[1])+"/"+n+".statement",serializeStatement(g,ss));r1::writeFile(std::string(argv[1])+"/"+n+".proof",serializeProof(g,ss,pp));};
  save("valid",g,s,p);
  save("detached-local",dg,detached,detached_proof);
  save("old-unseeded-GKR",g,s,replay);
  auto seed=g;seed.integration_context={};save("zero-context",seed,s,p);
  seed=g;seed.integration_context[0]^=1;save("wrong-context",seed,s,p);
  auto profile=s;profile.exps[0].profile=r1::ExpProfile::LegacyClampOne;
  // t=0 has the same E in both profiles: rejection must bind profile identity.
  save("profile-same-values",g,profile,p);
  auto bad=p;bad.gkr_input_binding.ipa.final_witness+=Fr(1);save("bridge",g,s,bad);
  bad=p;bad.gkr.layers[0].final_claim_u0+=Fr(1);save("gkr",g,s,bad);
  bad=p;bad.lookup.exps[0].memory.roots[0]+=Fr(1);save("memory",g,s,bad);
  bad=p;bad.lookup.input.master.value+=Fr(1);save("input",g,s,bad);
  auto changed=s;changed.input.public_values[0].value+=Fr(1);save("public-X",g,changed,p);
  changed=s;changed.input.context+="model";save("model",g,changed,p);
  changed=s;changed.input.children[1].spans[0].offset=3;save("address-map",g,changed,p);
  auto truncated=bytes;truncated.pop_back();r1::writeFile(std::string(argv[1])+"/truncated.statement",serializeStatement(g,s));r1::writeFile(std::string(argv[1])+"/truncated.proof",truncated);
 }
 auto claims=verifyR2(f.circuit(),g,s,p);r1::require(claims.size()==16,"all GKR terminals");
 auto b=p;b.master_binding.value+=Fr(1);rejects([&]{verifyR2(f.circuit(),g,s,b);});
 b=p;b.gkr_input_binding.ipa.final_witness+=Fr(1);rejects([&]{verifyR2(f.circuit(),g,s,b);});
 b=p;b.gkr.layers[0].final_claim_u0+=Fr(1);rejects([&]{verifyR2(f.circuit(),g,s,b);});
 b=p;b.lookup.exps[0].memory.roots[1]+=Fr(1);rejects([&]{verifyR2(f.circuit(),g,s,b);});
 auto ss=s;ss.input.public_values[0].value=Fr(4);rejects([&]{verifyR2(f.circuit(),g,ss,p);});
 ss=s;ss.input.context+="other-model";rejects([&]{verifyR2(f.circuit(),g,ss,p);});
 auto ww=w;ww[31]=Fr(1);ss=s;ss.input.master=r1::commit(root,ww);
 rejects([&]{proveR2(f.proverInstance(),g,ss,ww,v);});
 std::cout<<"PASS reused GKR + Range + indexed Exp + input consistency + cross-domain opening bridge\n";
 return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
