#include "r6_model.hpp"
#include "r6_trust.hpp"
#include "paper_r1/pc.hpp"
#include <iostream>
using namespace zkgpt;
int main(){try{
 r1::init();
 // R1 implements both layouts already. Compare the same one-dimensional
 // vector and point, including rectangular shapes and zero padded rows.
 for(auto n:{8u,16u,64u,256u,1024u})for(auto actual:{1u,n/2+1,n}){
  std::vector<Fr>w(n,Fr(0)),q;for(unsigned i=0;i<actual;++i)w[i]=Fr(3*i*i+7*i-11);
  for(unsigned i=0;i<r1::log2exact(n);++i)q.push_back(Fr(2+3*i));
  auto expected=r1::mle(w,q);
  for(auto layout:{r1::Layout::ColumnMajor,r1::Layout::RowMajor}){
   auto s=r1::PCSpec::make("unit/same-W",actual,n,layout);auto c=r1::commit(s,w);
   const auto&g=r1::generators(s).g;
   for(std::size_t row=0;row<s.rows;++row){G1 hand;hand.clear();for(std::size_t col=0;col<s.columns;++col)hand+=g[col]*w[layout==r1::Layout::RowMajor?row*s.columns+col:row+s.rows*col];r1::require(hand==c.rows[row],"physical commitment row differs from independent scalar sum");}
   r1::Fs p("row-major-equivalence"),v("row-major-equivalence");auto proof=r1::open(s,c,w,q,p,"W");r1::require(proof.value==expected,"layout changed original MLE");r1::verify(s,c,q,proof,v,"W");r1::require(p.digest()==v.digest(),"independent transcript mismatch");
   auto wrong=s;wrong.layout=layout==r1::Layout::RowMajor?r1::Layout::ColumnMajor:r1::Layout::RowMajor;bool rejected=false;try{r1::Fs bad("row-major-equivalence");r1::verify(wrong,c,q,proof,bad,"W");}catch(...){rejected=true;}r1::require(rejected,"wrong verifier-owned layout accepted");
  }
 }
 LLM nn(1,12,64,768,3072);r6::configure(nn);auto c=nn.buildPublicCircuit();auto plan=r2::planModelQueries(c,nn.getWitnessRegistry(),nn.parameterRegions(),r1::ExpProfile::LegacyClampOne);r6::auditGraph(c,nn,plan);
 r1::require(plan.circuit_id=="1cf3bc83a9a9f289e540238255d467b7fa6e2801209aa00f07fe232c48635a9a","circuit fingerprint changed");
 r1::require(plan.master.layout==r1::Layout::RowMajor,"R6 Master layout changed");for(auto&s:plan.sources)r1::require(s.spec.layout==r1::Layout::RowMajor,"R6 source still ColumnMajor");
 for(auto&r:plan.ranges){r1::RangeSpec s{r.id,plan.sources.at(r.child).spec,r.bits};r1::require(r1::digitSpec(s,0).layout==r1::Layout::RowMajor && r1::readSpec(r1::digitMemory(s,0)).layout==r1::Layout::RowMajor,"R6 Range/Lasso helper still ColumnMajor");}
 for(auto&e:plan.exps){r1::ExpSpec s{e.id,plan.sources.at(e.index_child).spec,plan.sources.at(e.value_child).spec,e.profile};for(unsigned k=0;k<4;++k)r1::require(r1::expHelperSpec(s,k).layout==r1::Layout::RowMajor,"R6 Exp helper still ColumnMajor");r1::require(r1::finalSpec(r1::expMemorySpec(s)).layout==r1::Layout::RowMajor,"R6 Exp memory still ColumnMajor");}
 std::cout<<"PASS R1 both layouts preserve original MLE; all R6 sources RowMajor; frozen circuit unchanged\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
