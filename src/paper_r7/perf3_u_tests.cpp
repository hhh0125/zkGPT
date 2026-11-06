#include "unified_input.hpp"
#include <iostream>
using namespace zkgpt::r7;using namespace oneblock;
namespace ui=zkgpt::r7::oneblock::unified;
int main(){try{
 r1::init();bool observed=false;
 for(unsigned threads:{1u,2u,4u})for(unsigned variant=0;variant<3;++variant){
  auto p=ui::pack({{1,1u<<18},{2,16},{3,1}},4);std::vector<std::vector<Fr>>a(p.segments.size()),b(a.size());
  for(std::size_t k=0;k<a.size();++k){auto n=p.segments[k].padded;a[k].resize(n);b[k].resize(n);for(std::size_t j=0;j<n;++j){a[k][j]=j+3<n?Fr(int(j%71)-29):Fr(0);b[k][j]=Fr(int((7*j+k)%79)-39);}}
  std::vector<Fr>point;
  for(unsigned t=0;t<r1::log2exact(p.domain);++t){
   ui::Factors factors=[&](std::size_t k,const ui::Consume&consume){consume([&,k](std::size_t j){return a[k].at(j);},[&,k](std::size_t j){return b[k].at(j);});};
   ui::Inputs inputs=[&](std::size_t k){return ui::InputSpan{a[k].data(),a[k].size()>3?a[k].size()-3:0,a[k].size()};};
   std::vector<Fr>before,after;ui::UOptions options;options.threads=threads;options.tile_pairs=3;
   options.observe=[&](std::size_t,std::size_t,const Fr*v,std::size_t n){before.insert(before.end(),v,v+n);};
   ui::UMetrics rm,cm;auto ref=ui::quadraticRoundStreaming(p.domain,p.segments,point,factors,inputs,options,&rm);
   options.striped_dot=true;options.observe=[&](std::size_t,std::size_t,const Fr*v,std::size_t n){after.insert(after.end(),v,v+n);};
   auto got=ui::quadraticRoundStreaming(p.domain,p.segments,point,factors,inputs,options,&cm);auto independent=ui::quadraticRoundFolded(p.domain,p.segments,point,factors);
   need(before==after && got.coefficients==ref.coefficients && got.coefficients==independent.coefficients && got.input_pair==ref.input_pair && got.kernel_pair==ref.kernel_pair,"PERF3 U contraction/polynomial/endpoint mismatch");
   need(cm.contracted_coordinates==rm.contracted_coordinates && cm.multiplications==rm.multiplications && !options.elide_zero_suffix,"PERF3 dot changed scan or arithmetic coverage");
   observed=observed || cm.striped_dot_groups>0;
   Fr r=variant==0?Fr(0):variant==1?Fr(1):Fr(-3-int(t));for(auto&v:b)if(v.size()>1){for(std::size_t j=0;j<v.size()/2;++j)v[j]=v[2*j]+r*(v[2*j+1]-v[2*j]);v.resize(v.size()/2);}point.push_back(r);
  }
 }
 need(observed,"PERF3 striped accumulator candidate not executed");std::cout<<"PERF3_U_ALL_ROUNDS_CONTRACTIONS_POLYNOMIALS_K_FOLDS_ENDPOINTS_PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
