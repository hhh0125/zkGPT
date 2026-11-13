from pathlib import Path
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');P=R/'src/paper_r7';S=R/'reports/R7-MLE-FOLD-20261108-v1'
assert 'categoryKernelEndpoint' in (S/'endpoint-RED.log').read_text()
p=P/'classified_commitments.hpp';s=p.read_text();anchor=' Fr kernelEndpoint(';at=s.index(anchor);s=s[:at]+' std::optional<Fr>categoryKernelEndpoint(const std::vector<Fr>&,const std::vector<Fr>&,const Authentication&,bool require_profitable=true)const;\n'+s[at:];p.write_text(s)
p=P/'category_auth.cpp';s=p.read_text().replace('#include "category_tiles.hpp"','#include "category_tiles.hpp"\n#include "k_terminal.hpp"');at=s.index('void Commitments::authenticateCategoryInput')
method=r'''std::optional<Fr>Commitments::categoryKernelEndpoint(const std::vector<Fr>&q,const std::vector<Fr>&rho,const Authentication&auth,bool require_profitable)const{
 need(layout_.category_mode && q.size()<=32 && rho.size()==claims_.size() && auth.claims.size()==claims_.size(),"category endpoint dimensions");
 auto start=std::chrono::steady_clock::now();std::uint64_t total=0,eligible=0,fast=0,enumerated=0,blocks=0;
 auto base=[&](std::size_t k){const auto&r=roots_.at(k).spec;need(r.category<5,"category endpoint root category");const auto&c=layout_.categories[r.category];need(r.category_offset<=c.actual && r.actual<=c.actual-r.category_offset,"category endpoint actual bounds");auto z=c.logical_offset+r.category_offset;need(z<=(std::size_t(1)<<q.size()) && r.actual<=(std::size_t(1)<<q.size())-z,"category endpoint global bounds");return z;};
 auto each=[&](std::size_t i,const auto&consume){const auto&c=claims_[i];const auto&p=auth.claims[i];for(std::size_t root=0;root<roots_.size();++root){if(c.only_root>=0 && std::size_t(c.only_root)!=root)continue;if(!p.hasRoot(root))continue;auto offset=base(root);p.forRoot(root,auth.store,[&](const mapping::Run&r){
  const auto actual=roots_[root].spec.actual;need(r.count && r.logical_step && r.begin<c.padded && r.count<=1+(c.padded-1-r.begin)/r.logical_step && r.first.root==root && r.first.index<actual && r.stride>=-std::int64_t(actual) && r.stride<=std::int64_t(actual),"category endpoint frozen run bounds");
  auto last=std::int64_t(r.first.index)+std::int64_t(r.count-1)*r.stride;need(last>=0 && std::uint64_t(last)<actual,"category endpoint frozen run end");consume(r,offset);
 });}};
 // Admit only from public affine geometry, not from witness values or point
 // values. Otherwise the exact previous dense endpoint remains available.
 for(std::size_t i=0;i<claims_.size();++i){if(!auth.claims[i].frozen){total+=claims_[i].padded;continue;}each(i,[&](const mapping::Run&r,std::size_t offset){total+=r.count;if(r.logical_step==1 && (r.stride==0 || (r.stride==1 && (r.begin%64)==(offset+r.first.index)%64))){auto edge=(64-r.begin%64)%64;if(r.count>edge)eligible+=(r.count-edge)/64*64;}});}
 if(require_profitable && (eligible<1024 || eligible*4<total*3)){std::cerr<<"CATEGORY_K_ENDPOINT {\"admitted\":false,\"eligible_coordinates\":"<<eligible<<",\"map_coordinates\":"<<total<<",\"wall_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<"}\n";return std::nullopt;}
 Weights global(q);std::size_t weight_peak=global.bytes();Fr out=0;
 for(std::size_t i=0;i<claims_.size();++i){const auto&c=claims_[i];const auto&p=auth.claims[i];std::unique_ptr<Weights>scratch;if(!p.weights)scratch=std::make_unique<Weights>(c.point);const auto&w=p.weights?*p.weights:*scratch;weight_peak=std::max(weight_peak,global.bytes()+(scratch?scratch->bytes():0));TerminalBasis basis(c.point,q);
  auto add=[&](const mapping::Run&r,std::size_t offset){std::size_t j=r.begin,remaining=r.count;std::int64_t index=r.first.index;
   while(remaining){auto z=offset+std::size_t(index);std::size_t length=1;unsigned bits=0;
    if(r.logical_step==1 && (r.stride==0 || r.stride==1))while(length<=remaining/2 && !(j&length) && (r.stride==0 || !(z&length))){length*=2;++bits;}
    if(length>=64){out+=rho[i]*basis.block(j,z,bits,r.stride==0);fast+=length;++blocks;}
    else{w.scaled(j,length,rho[i],[&](const Fr&value){out+=value*global.at(offset+std::size_t(index));index+=r.stride;},r.logical_step);enumerated+=length;j+=length*r.logical_step;remaining-=length;continue;}
    index+=std::int64_t(length)*r.stride;j+=length;remaining-=length;
   }
  };
  if(p.frozen)each(i,add);else{ConsumedMapDigest digest;for(std::size_t j=0;j<c.padded;++j){auto l=c.map(j);digest.append(l);if(l.constant)continue;need(l.coordinate.root<roots_.size() && l.coordinate.index<roots_[l.coordinate.root].spec.actual,"category endpoint callback bounds");out+=rho[i]*w.at(j)*global.at(base(l.coordinate.root)+l.coordinate.index);++enumerated;}need(digest.finish()==c.map_digest,"category endpoint source map changed after defer");}
 }
 need(weight_peak<=8u<<20,"category endpoint public weight tables bound");auth_metrics_.endpoint_fast_coordinates+=fast;auth_metrics_.endpoint_fast_blocks+=blocks;std::cerr<<"CATEGORY_K_ENDPOINT {\"admitted\":true,\"eligible_coordinates\":"<<eligible<<",\"map_coordinates\":"<<total<<",\"fast_coordinates\":"<<fast<<",\"enumerated_coordinates\":"<<enumerated<<",\"blocks\":"<<blocks<<",\"weight_table_peak_bytes\":"<<weight_peak<<",\"K_dense_array_bytes\":0,\"wall_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<"}\n";return out;
}
'''
s=s[:at]+method+s[at:]
old='auto endpoint=[&](const std::vector<Fr>&q){Fr out=0;';new='''bool fast_endpoint=true;if(const auto*v=std::getenv("ZKGPT_CATEGORY_K_ENDPOINT")){std::string text(v);need(text=="0" || text=="1","category K endpoint switch");fast_endpoint=text=="1";}
 auto endpoint=[&](const std::vector<Fr>&q){if(fast_endpoint){auto out=categoryKernelEndpoint(q,rho,auth);if(out)return *out;}Fr out=0;''';assert old in s;s=s.replace(old,new);p.write_text(s)
with (S/'progress.md').open('a') as f:f.write('Ruling: initial D capacity optimization affects P only. User requires Full12 P/V improvement, so reopen D before any strict runs: reuse existing TerminalBasis for the identical public global K endpoint, with geometry-only >=75% profitable admission and exact old dense fallback. No FS rule/message/claim/source map changes. New 220-case independent bit-product/dense-map/fallback-mutation RED -> GREEN and focused supplemental review required because this is new mathematical implementation scope, not rereview of the prior measurement fix.\n')
print('public global K endpoint direct basis implemented with dense fallback')
