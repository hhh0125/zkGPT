#include "model_resources.hpp"
#include "paper_r1/wire.hpp"
#include <fstream>
#include <limits>
#include <algorithm>
namespace zkgpt::r2 {
namespace {
std::size_t add(std::size_t a,std::size_t b){r1::require(b<=std::numeric_limits<std::size_t>::max()-a,"model resource overflow");return a+b;}
std::size_t mul(std::size_t a,std::size_t b){r1::require(!a || b<=std::numeric_limits<std::size_t>::max()/a,"model resource overflow");return a*b;}
std::size_t memoryProofBound(const r1::PCSpec &p,std::size_t table){
 auto d=r1::log2exact(p.padded),t=r1::log2exact(table);
 // Includes row PCs, all product-tree polynomial messages and 7 leaf IPAs.
 return add(mul(64,p.rows+table),add(mul(160,2*d*d+t*t),16384));
}
std::size_t rangeProofBound(const r1::PCSpec &p,unsigned bits){
 auto digits=(bits+8)/9;return add(mul(64,p.rows*digits),add(mul(digits,memoryProofBound(p,512)+8192),32768));
}
}
unsigned parseModelBlocks(const std::string &text){
 r1::require(text=="1" || text=="12","blocks must be exactly 1 or 12");return text=="1"?1:12;
}
std::size_t parseModelBudgetGiB(const std::string &text){
 r1::require(!text.empty(),"missing memory budget");std::size_t n=0;
 for(char c:text){r1::require(c>='0' && c<='9',"memory budget must contain only decimal digits");n=n*10+std::size_t(c-'0');r1::require(n<=256,"memory budget cap");}
 r1::require(n>0,"memory budget must be positive");return n;
}
bool parseModelNumericalProfile(const std::string &text){
 r1::require(text=="legacy-clamp1" || text=="r4-partial-candidate-v1","unsupported numerical profile");
 return text=="r4-partial-candidate-v1";
}
std::string modelProfile(unsigned blocks,bool r4_candidate){
 r1::require(blocks==1 || blocks==12,"only explicit oneblock/full12 engineering profiles are approved");
 if(r4_candidate)return blocks==1?"local-r4-partial-clamp1-oneblock-constraints-v1":"local-r4-partial-clamp1-full12-constraints-v1";
 return blocks==1?"local-quantized-clamp1-oneblock-constraints-v1":"local-quantized-clamp1-full12-constraints-v1";
}
ModelResources modelResources(const layeredCircuit &c,const ModelQueryPlan &p){
 modelProfile(p.shape.layer_count);ModelResources out;
 for(std::size_t i=0;i<c.circuit.size();++i){const auto &l=c.circuit[i];
  if(i)out.terminals+=(l.bit_length_u[0]>=0)+(l.bit_length_v[0]>=0)+(l.ty==layerType::FCONN?2:0);
  out.gate_bytes=add(out.gate_bytes,add(mul(l.uni_gates.size(),sizeof(uniGate)),mul(l.bin_gates.size(),sizeof(binGate))));
  out.gate_bytes=add(out.gate_bytes,mul(l.ori_id_u.size()+l.ori_id_v.size(),sizeof(u32)));
 }
 G1 zero;zero.clear();r1::LookupBatchSpec s;s.input.context="resource-only-trusted-model-plan";
 s.input.master={p.master,std::vector<G1>(p.master.rows,zero)};
 for(const auto &q:p.sources){s.input.children.push_back({{q.spec,std::vector<G1>(q.spec.rows,zero)},q.spans});out.child_elements=add(out.child_elements,q.spec.padded);}
 s.ranges=p.ranges;s.exps=p.exps;out.lookup_prover_bytes=r1::planLookupBatch(s).estimated_prover_bytes;
 // R2 full12 arithmetic-only measured peak: 41,025,072 KiB. Keep a 25%
 // envelope, plus the live R1 phase workspace (not a linear block estimate).
 const std::size_t retained=p.shape.layer_count==12?std::size_t(41025072)*1280:std::size_t(4)<<30;
 out.prover_peak_bytes=add(out.lookup_prover_bytes,std::max(retained,mul(out.gate_bytes,3)));
 std::size_t maxq=0;out.proof_upper_bytes=add(1<<20,mul(out.terminals,8192));
 for(const auto &q:s.ranges){const auto &pc=s.input.children[q.child].commitment.spec;out.range_values=add(out.range_values,pc.actual);maxq=std::max(maxq,pc.padded);out.proof_upper_bytes=add(out.proof_upper_bytes,rangeProofBound(pc,q.bits));}
 for(const auto &q:s.exps){const auto &pc=s.input.children[q.index_child].commitment.spec;out.exp_values=add(out.exp_values,pc.actual);for(unsigned b:{32,13,1,32})out.proof_upper_bytes=add(out.proof_upper_bytes,rangeProofBound(pc,b));out.proof_upper_bytes=add(out.proof_upper_bytes,memoryProofBound(pc,8192)+65536);}
 out.verifier_peak_bytes=add(mul(p.master.padded,64),add(mul(out.gate_bytes,3),add(mul(maxq,128),mul(out.proof_upper_bytes,4))));
 return out;
}
void requireModelBudget(const ModelResources &r,std::size_t budget,std::size_t available){
 r1::require(r.prover_peak_bytes<=budget,"model conservative prover peak exceeds selected budget");
 r1::require(add(r.prover_peak_bytes,std::size_t(16)<<30)<=available,"model lacks available memory plus 16GiB reserve");
 r1::require(r.proof_upper_bytes<r1::kMaxWireBytes,"model estimated proof exceeds existing wire budget");
}
std::size_t availableMemory(){std::ifstream f("/proc/meminfo");std::string k,unit;std::size_t n;while(f>>k>>n>>unit)if(k=="MemAvailable:")return mul(n,1024);throw std::runtime_error("cannot read available memory");}
}
