from pathlib import Path
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');S=R/'reports/R7-MLE-FOLD-20261108-v1';P=R/'src/paper_r7'
assert 'CARRY_BASIS_2200' in (S/'carry-GREEN.log').read_text()
(P/'carry_terminal.hpp').write_bytes((S/'carry_terminal_green.hpp').read_bytes())
(P/'carry_terminal_tests.cpp').write_bytes((S/'carry_terminal_probe.cpp').read_bytes())
p=P/'CMakeLists.txt';s=p.read_text();assert 'r7_carry_terminal_tests' not in s;s+='\nadd_executable(r7_carry_terminal_tests carry_terminal_tests.cpp)\ntarget_link_libraries(r7_carry_terminal_tests PRIVATE paper_r7)\nadd_test(NAME r7_carry_terminal COMMAND r7_carry_terminal_tests)\n';p.write_text(s)
p=P/'category_auth.cpp';s=p.read_text().replace('#include "k_terminal.hpp"','#include "k_terminal.hpp"\n#include "carry_terminal.hpp"');start=s.index('std::optional<Fr>Commitments::categoryKernelEndpoint');end=s.index('void Commitments::authenticateCategoryInput',start)
method=r'''std::optional<Fr>Commitments::categoryKernelEndpoint(const std::vector<Fr>&q,const std::vector<Fr>&rho,const Authentication&auth,bool require_profitable)const{
 need(layout_.category_mode && q.size()<=32 && rho.size()==claims_.size() && auth.claims.size()==claims_.size(),"category endpoint dimensions");
 auto started=std::chrono::steady_clock::now();std::uint64_t total=0,eligible=0,fast=0,enumerated=0,blocks=0;
 auto power=[](std::size_t n){return n && !(n&(n-1));};
 auto base=[&](std::size_t k){const auto&r=roots_.at(k).spec;need(r.category<5,"category endpoint root category");const auto&c=layout_.categories[r.category];need(r.category_offset<=c.actual && r.actual<=c.actual-r.category_offset,"category endpoint actual bounds");auto z=c.logical_offset+r.category_offset;need(z<=(std::size_t(1)<<q.size()) && r.actual<=(std::size_t(1)<<q.size())-z,"category endpoint global bounds");return z;};
 auto each=[&](std::size_t i,const auto&consume){const auto&c=claims_[i];const auto&p=auth.claims[i];for(std::size_t root=0;root<roots_.size();++root){if(c.only_root>=0 && std::size_t(c.only_root)!=root)continue;if(!p.hasRoot(root))continue;auto offset=base(root);p.forRoot(root,auth.store,[&](const mapping::Run&r){
  const auto actual=roots_[root].spec.actual;need(r.count && r.logical_step && r.begin<c.padded && r.count<=1+(c.padded-1-r.begin)/r.logical_step && r.first.root==root && r.first.index<actual && r.stride>=-std::int64_t(actual) && r.stride<=std::int64_t(actual),"category endpoint frozen run bounds");
  auto last=std::int64_t(r.first.index)+std::int64_t(r.count-1)*r.stride;need(last>=0 && std::uint64_t(last)<actual,"category endpoint frozen run end");consume(r,offset);
 });}};
 // Split only the source's free-bit interval. Global alignment is unnecessary:
 // the public carry/borrow DP accounts for all global-coordinate carries.
 auto chunks=[&](const mapping::Run&r,std::size_t offset,const auto&consume){
  const auto magnitude=std::size_t(r.stride<0?-r.stride:r.stride);
  if(!power(r.logical_step) || (r.stride && !power(magnitude))){consume(r.begin,offset+r.first.index,r.count,0u,false);return;}
  unsigned shift=0;for(auto n=r.logical_step;n>1;n>>=1)++shift;
  std::size_t j=r.begin,remaining=r.count;std::int64_t index=r.first.index;
  while(remaining){std::size_t length=1;unsigned bits=0;while(length<=remaining/2 && !((j>>shift)&length)){length*=2;++bits;}
   consume(j,offset+std::size_t(index),length,bits,length>=64);j+=length*r.logical_step;remaining-=length;index+=std::int64_t(length)*r.stride;
  }
 };
 for(std::size_t i=0;i<claims_.size();++i){if(!auth.claims[i].frozen){total+=claims_[i].padded;continue;}each(i,[&](const mapping::Run&r,std::size_t offset){total+=r.count;chunks(r,offset,[&](std::size_t,std::size_t,std::size_t length,unsigned,bool use){if(use)eligible+=length;});});}
 if(require_profitable && (eligible<1024 || eligible*4<total*3)){std::cerr<<"CATEGORY_K_ENDPOINT {\"admitted\":false,\"basis\":\"POWER_STRIDE_CARRY_BORROW-v2\",\"eligible_coordinates\":"<<eligible<<",\"map_coordinates\":"<<total<<",\"wall_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<"}\n";return std::nullopt;}
 Weights global(q);std::size_t weight_peak=global.bytes();Fr out=0;
 for(std::size_t i=0;i<claims_.size();++i){const auto&c=claims_[i];const auto&p=auth.claims[i];std::unique_ptr<Weights>scratch;if(!p.weights)scratch=std::make_unique<Weights>(c.point);const auto&w=p.weights?*p.weights:*scratch;weight_peak=std::max(weight_peak,global.bytes()+(scratch?scratch->bytes():0));TerminalBasis aligned(c.point,q);CarryTerminalBasis carry(c.point,q);
  if(p.frozen)each(i,[&](const mapping::Run&r,std::size_t offset){chunks(r,offset,[&](std::size_t j,std::size_t z,std::size_t length,unsigned bits,bool use){
   if(use){bool simple=r.logical_step==1 && (r.stride==0 || (r.stride==1 && !(z&(length-1))));out+=rho[i]*(simple?aligned.block(j,z,bits,r.stride==0):carry.block(j,z,bits,r.logical_step,r.stride));fast+=length;++blocks;}
   else{std::int64_t index=std::int64_t(z);w.scaled(j,length,rho[i],[&](const Fr&value){out+=value*global.at(std::size_t(index));index+=r.stride;},r.logical_step);enumerated+=length;}
  });});
  else{ConsumedMapDigest digest;for(std::size_t j=0;j<c.padded;++j){auto l=c.map(j);digest.append(l);if(l.constant)continue;need(l.coordinate.root<roots_.size() && l.coordinate.index<roots_[l.coordinate.root].spec.actual,"category endpoint callback bounds");out+=rho[i]*w.at(j)*global.at(base(l.coordinate.root)+l.coordinate.index);++enumerated;}need(digest.finish()==c.map_digest,"category endpoint source map changed after defer");}
 }
 need(weight_peak<=8u<<20,"category endpoint public weight tables bound");auth_metrics_.endpoint_fast_coordinates+=fast;auth_metrics_.endpoint_fast_blocks+=blocks;std::cerr<<"CATEGORY_K_ENDPOINT {\"admitted\":true,\"basis\":\"POWER_STRIDE_CARRY_BORROW-v2\",\"eligible_coordinates\":"<<eligible<<",\"map_coordinates\":"<<total<<",\"fast_coordinates\":"<<fast<<",\"enumerated_coordinates\":"<<enumerated<<",\"blocks\":"<<blocks<<",\"additional_endpoint_weight_table_peak_bytes\":"<<weight_peak<<",\"K_dense_array_bytes\":0,\"wall_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<"}\n";return out;
}
'''
s=s[:start]+method+s[end:];p.write_text(s)
with (S/'progress.md').open('a') as f:f.write('2026-11-09: V1 Oneblock A2/B1/A3 completed, preserved as diagnostic. Actual endpoint eligible22.3%, so old75% gate selected dense fallback and V did not improve. Parent scheduler terminated only after current child finished; no source edits during active measurement. Independent carry basis2200-case numeric RED/GREEN plus262144-term dense equality passed before production edit. Extend K endpoint to power logical steps and positive/negative/zero power strides with public two-state carry/borrow DP; no global alignment or transcript change. New119th CTest, actual old proof V replay, focused review, final freeze and fresh ab-v2 required.\n')
print('carry basis integrated, old source/binary/evidence retained')
