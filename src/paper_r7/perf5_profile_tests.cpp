#include "oneblock_sumcheck.hpp"
#include "perf5_profile.hpp"
#include <iostream>
using namespace zkgpt::r7;
int main(){try{crypto::r1::init();
 unsetenv("ZKGPT_PERF5_PROFILE");std::string role(128,'p');std::uint64_t allocations=0;
 {crypto::AllocationTrace trace;{perf5::Product product(role,2,true);perf5::Scope total(perf5::Total);}allocations=trace.count();}
 crypto::need(allocations==0,"disabled Product profiler allocates role storage");
 perf5::Context ctx;perf5::context=&ctx;
 {perf5::Scope total(perf5::Total);{perf5::Scope weights(perf5::Weights);std::vector<crypto::Fr>v(32,crypto::Fr(1));perf5::weights(v.size());}}
 perf5::context=nullptr;auto&t=ctx.stats[perf5::Total];auto&w=ctx.stats[perf5::Weights];
 crypto::need(t.calls==1 && w.calls==1 && w.allocs==1 && t.exclusive_allocs==0,"profile allocation hierarchy mismatch");
 crypto::need(t.wall>=w.wall && std::abs(t.wall-t.exclusive-w.wall)<1e-9,"profile time hierarchy mismatch");
 crypto::need(ctx.weight_calls==1 && ctx.weight_elements==32 && ctx.weight_muls==62,"profile analytical weights mismatch");
 perf5::context=&ctx;{perf5::Scope leaf(perf5::LeafWeights);perf5::leafWeights(8);perf5::leafWeights(1);}perf5::context=nullptr;
 crypto::need(ctx.leaf_weight_calls==2 && ctx.leaf_weight_elements==9 && ctx.leaf_weight_muls==14 && ctx.leaf_weight_payload_bytes==9*sizeof(crypto::Fr),"leaf Evaluate analytical weights mismatch");
 crypto::need(ctx.weight_calls==1 && ctx.weight_elements==32 && ctx.weight_muls==62,"leaf weights contaminated Product layer counters");
 std::cout<<"PASS disabled Product profiler has no allocations; nested time/allocation hierarchy and weight counters\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
