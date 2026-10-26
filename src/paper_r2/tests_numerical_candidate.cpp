#include "model_queries.hpp"
#include "models.hpp"
#include <iostream>
#include <map>
namespace r1=zkgpt::r1;
using namespace zkgpt::r2;
class ExistingExpProducer : public LLM {
public:
 ExistingExpProducer():LLM(1,12,64,768,3072){}
 const std::vector<int> &values(){compute_e_table();return table;}
};
using Values=std::map<std::size_t,Fr>;
Fr value(const Values &w,std::size_t i){auto it=w.find(i);return it==w.end()?Fr(0):it->second;}
Fr evaluate(const layer &l,std::size_t output,const Values &w,const Values &previous={}){
 Fr sum=0;
 for(const auto &g:l.uni_gates)if(g.g==output)sum+=Fr(g.sc)*(g.lu?value(previous,g.u):value(w,l.ori_id_u.at(g.u)));
 for(const auto &g:l.bin_gates)if(g.g==output){
  const Fr a=g.l==0?value(w,l.ori_id_u.at(g.u)):value(previous,g.u);
  const Fr b=(g.l&1)?value(previous,g.v):value(w,l.ori_id_v.at(g.v));
  sum+=Fr(g.sc)*a*b;
 }
 return sum;
}
bool rangeAllows(const ModelQueryPlan &p,std::size_t index,const Fr &v){
 bool covered=false;
 for(const auto &r:p.ranges)for(const auto &s:p.sources.at(r.child).spans)if(index>=s.offset && index-s.offset<s.count){
  covered=true;auto bytes=r1::encode(v+s.bias);
  for(unsigned bit=r.bits;bit<256;++bit)if(bytes[bit/8]&(1U<<(bit%8)))return false;
 }
 r1::require(covered,"test coordinate absent from actual Lasso plan");return true;
}
int main(){try{
 r1::init();LLM nn(1,12,64,768,3072);
 nn.setR4NumericalCandidate();
 auto c=nn.buildPublicCircuit();
 const auto &reg=nn.getWitnessRegistry();
 auto named=[&](const std::string &name)->const RangeConstraint &{for(const auto &r:reg.constraints())if(r.name==name)return r;throw std::runtime_error("missing candidate region "+name);};
 auto p=planModelQueries(c,reg,nn.parameterRegions(),r1::ExpProfile::LegacyClampOne);
 const auto &maximum=named("softmax_0_pmax");
 r1::require(maximum.is_signed && maximum.bits==64,"negative true row maximum must be accepted by signed pmax Range");
 r1::require(rangeAllows(p,maximum.val0_offset,Fr(-1)),"negative maximum Range");
 const auto &selector=named("softmax_0_maximum_selector"),&gap=named("softmax_0_delta1"),&score=named("softmax_0_maximum_score");
 const auto &sm=*std::find_if(c.circuit.begin(),c.circuit.end(),[](const auto &l){return l.ty==layerType::SOFTMAX_1;});
 // Head0,row2 has scores [-2,-1]; hand-derived indices, not producer outputs.
 const std::size_t checks=24576+5*6336+384;
 Values w{{maximum.val0_offset-394,Fr(1)},{gap.val0_offset+1,Fr(1)},{gap.val0_offset+2,Fr(0)},{selector.val0_offset+1,Fr(0)},{selector.val0_offset+2,Fr(1)}};
 r1::require(evaluate(sm,checks+1,w).isZero() && evaluate(sm,checks+385,w).isZero(),"legal negative row must pass generated attainment gates");
 auto bad=w;bad[gap.val0_offset+1]=9;bad[gap.val0_offset+2]=8;
 r1::require(!evaluate(sm,checks+385,bad).isZero(),"wrong upper bound with no attained score accepted");
 bad=w;bad[selector.val0_offset+1]=1;bad[selector.val0_offset+2]=0;
 r1::require(!evaluate(sm,checks+385,bad).isZero(),"selected nonmaximum accepted");
 bad=w;bad[selector.val0_offset+2]=0;r1::require(!evaluate(sm,checks+1,bad).isZero(),"missing selector accepted");
 bad=w;bad[selector.val0_offset+1]=1;r1::require(!evaluate(sm,checks+1,bad).isZero(),"two selectors accepted");
 r1::require(!rangeAllows(p,selector.val0_offset,Fr(2)),"non-Boolean selector accepted by production query plan");
 w[gap.val0_offset+1]=0;r1::require(evaluate(sm,checks+385,w).isZero(),"tied maximum rejected");
 w[score.val0_offset+1]=-2;Values previous{{1,Fr(-2)}};
 r1::require(evaluate(sm,checks+768+1,w,previous).isZero(),"score advice not bound to previous QK");
 w[score.val0_offset+1]=-3;r1::require(!evaluate(sm,checks+768+1,w,previous).isZero(),"wrong score advice accepted");
 const auto &u=named("layer_norm_0_sqrt_upper"),&v=named("layer_norm_0_sqrt_lower"),&s=named("layer_norm_0_sigma"),&B=named("layer_norm_0_B");
 const auto &ln=*std::find_if(c.circuit.begin(),c.circuit.end(),[](const auto &l){return l.ty==layerType::LAYER_NORM_1;});
 Values root{{named("layer_norm_0_y").val0_offset-10,Fr(1)},{B.val0_offset,Fr(1)},{s.val0_offset,Fr(1)},{u.val0_offset,Fr(2)},{v.val0_offset,Fr(1)}};
 r1::require(evaluate(ln,32864,root).isZero() && evaluate(ln,32896,root).isZero(),"B=1 positive root must pass actual helper gates");
 r1::require(rangeAllows(p,s.val0_offset,Fr(1)) && !rangeAllows(p,s.val0_offset,Fr(0)),"zero sigma accepted by actual queries");
 r1::require(!rangeAllows(p,s.val0_offset,Fr(std::uint64_t(1)<<32)),"unbounded sqrt square domain");
 root[u.val0_offset]=3;r1::require(!evaluate(ln,32864,root).isZero(),"unbound sqrt factor");
 for(const auto *r:{&u,&v,&named("gelu_0_delta2"),&named("gelu_0_term1"),&named("gelu_0_term2"),&named("layer_norm_0_term1"),&named("layer_norm_0_term2")}){
  r1::require(rangeAllows(p,r->val0_offset,Fr(1)) && !rangeAllows(p,r->val0_offset,Fr(0)),"nonpositive nonlinear slack accepted");
  Fr upper=1;for(unsigned i=0;i<r->bits;++i)upper*=Fr(2);
  r1::require(rangeAllows(p,r->val0_offset,upper-Fr(1)) && !rangeAllows(p,r->val0_offset,upper),"positive shift enlarged original unsigned bound");
 }
 const auto &gl=*std::find_if(c.circuit.begin(),c.circuit.end(),[](const auto &l){return l.ty==layerType::GELU_1;});
 const auto &abs=named("gelu_0_abs"),&t=named("gelu_0_t"),&d=named("gelu_0_delta2");
 Values branch{{named("gelu_0_y").val0_offset-10,Fr(1)},{abs.val0_offset,Fr(88)},{t.val0_offset,Fr(0)},{d.val0_offset,Fr(1)}};
 r1::require(evaluate(gl,3*t.count,branch).isZero() && rangeAllows(p,d.val0_offset,Fr(1)),"x88 correct branch rejected");
 branch[t.val0_offset]=1;branch[d.val0_offset]=0;
 r1::require(evaluate(gl,3*t.count,branch).isZero() && !rangeAllows(p,d.val0_offset,Fr(0)),"x88 alternate branch was not rejected by added Range");
 // Actual verifier-owned statement rebuild must authenticate the new affine
 // views too, not merely the legacy spans covered by older tests.
 G1 zero;zero.clear();std::vector<r1::Commitment> commits;
 for(const auto &source:p.sources)commits.push_back({source.spec,std::vector<G1>(source.spec.rows,zero)});
 r1::Commitment cm{p.master,std::vector<G1>(p.master.rows,zero)};
 std::vector<Fr> x(32768,Fr(0)),y(32768,Fr(0));const std::string model="TEST-R4-PARTIAL-NOT-PAPER-APPROVED";
 auto statement=makeModelQueryStatement(p,cm,commits,commits.back(),x,y,model);
 requireModelQueryStatement(p,statement,commits.back(),x,y,model);
 auto reject=[&](const auto &ss){bool rejected=false;try{requireModelQueryStatement(p,ss,commits.back(),x,y,model);}catch(const std::exception&){rejected=true;}r1::require(rejected,"candidate statement mutation accepted");};
 std::size_t positive_views_mutated=0;
 for(auto index:{selector.val0_offset,score.val0_offset,u.val0_offset,d.val0_offset}){
  bool mutated=false;
  for(std::size_t child=0;child<p.sources.size();++child)for(std::size_t j=0;j<p.sources[child].spans.size();++j){
   const auto &span=p.sources[child].spans[j];if(index>=span.offset && index-span.offset<span.count){
    auto bad_statement=statement;bad_statement.input.children[child].spans[j].offset+=1;reject(bad_statement);
    bad_statement=statement;bad_statement.input.children[child].spans[j].bias+=Fr(1);reject(bad_statement);mutated=true;
    positive_views_mutated+=span.bias==Fr(-1);
   }
  }
  r1::require(mutated,"candidate source span missing from statement test");
 }
 r1::require(positive_views_mutated>=2,"new shifted positivity source mutations missing");
 auto altered=statement;altered.input.context="R2-legacy-source-adapter-NOT-PAPER-APPROVED/"+model+"/"+p.circuit_id;reject(altered);
 altered=statement;altered.ranges.pop_back();reject(altered);
 ExistingExpProducer exp_producer;const auto &actual_exp=exp_producer.values();
 r1::require(actual_exp.size()==655360,"production Exp table extent");
 for(std::size_t i=0;i<actual_exp.size();++i)
  r1::require(actual_exp[i]==int(r1::expValue(std::uint32_t(i),r1::ExpProfile::LegacyClampOne)),"producer/Verifier Exp table mismatch");
 // Every remaining u32 index takes the same explicit constant-tail branch.
 r1::require(r1::expValue(655360,r1::ExpProfile::LegacyClampOne)==1 &&
  r1::expValue(0xffffffff,r1::ExpProfile::LegacyClampOne)==1 && r1::expValue(0xffffffff,r1::ExpProfile::Unclamped)==0,"Exp tail profile difference");
 std::cout<<"PASS generated candidate GKR/Range: negative and tied max, wrong/no/multiple/invalid selector, score binding, positive sqrt, x88 branch, original upper bounds\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
