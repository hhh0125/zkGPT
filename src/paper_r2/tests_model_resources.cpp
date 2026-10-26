#include "model_resources.hpp"
#include "models.hpp"
#include <iostream>
using namespace zkgpt::r2;
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}zkgpt::r1::require(bad,"unsafe model resource/policy accepted");}
int main(){try{
 zkgpt::r1::init();
 zkgpt::r1::require(modelProfile(1)=="local-quantized-clamp1-oneblock-constraints-v1","R2 profile changed");
 zkgpt::r1::require(modelProfile(12)=="local-quantized-clamp1-full12-constraints-v1","full12 identity");
 rejects([]{modelProfile(0);});rejects([]{modelProfile(13);});
 zkgpt::r1::require(parseModelBlocks("12")==12 && parseModelBudgetGiB("192")==192,"strict run policy parser");
 for(const auto &v:{"4294967297","4294967308","12junk","-1","0",""})rejects([&]{parseModelBlocks(v);});
 for(const auto &v:{"0","192junk","257","-1",""})rejects([&]{parseModelBudgetGiB(v);});
 LLM nn(1,12,64,768,3072);auto c=nn.buildPublicCircuit();
 auto p=planModelQueries(c,nn.getWitnessRegistry(),nn.parameterRegions(),zkgpt::r1::ExpProfile::LegacyClampOne);
 auto r=modelResources(c,p);
 zkgpt::r1::require(r.terminals==48 && r.range_values==2553984 && r.exp_values==6336,"trusted actual model coverage");
 zkgpt::r1::require(r.prover_peak_bytes>=r.lookup_prover_bytes && r.proof_upper_bytes>4260851,"R2 measured envelope");
 rejects([&]{requireModelBudget(r,1,std::size_t(256)<<30);});
 rejects([&]{requireModelBudget(r,std::size_t(256)<<30,1);});
 requireModelBudget(r,std::size_t(256)<<30,std::size_t(300)<<30);
 std::cout<<"PASS actual oneblock resource gate, fixed profiles and low-memory rejection\n";return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
