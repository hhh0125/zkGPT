#include "classified_commitments.hpp"
#include "public_k.hpp"
#include "category_tiles.hpp"
#include "k_terminal.hpp"
#include "carry_terminal.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <openssl/evp.h>
namespace zkgpt::r7::oneblock::classified {
namespace {
class ConsumedMapDigest {
 std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)>ctx{EVP_MD_CTX_new(),EVP_MD_CTX_free};
 std::array<unsigned char,32768>buffer{};std::size_t used=0;
 void flush(){if(used){need(EVP_DigestUpdate(ctx.get(),buffer.data(),used)==1,"category map digest update");used=0;}}
 public:
 ConsumedMapDigest(){need(ctx && EVP_DigestInit_ex(ctx.get(),EVP_sha256(),nullptr)==1,"category map digest init");}
 void append(const Located&l){if(used+41>buffer.size())flush();buffer[used++]=l.constant?1:0;for(auto n:{l.coordinate.root,l.coordinate.index})for(unsigned j=0;j<4;++j)buffer[used++]=std::uint8_t(n>>(8*j));need(l.bias.isValid() && l.bias.serialize(buffer.data()+used,32)==32,"category map bias");used+=32;}
 Digest finish(){flush();Digest d{};unsigned n=0;need(EVP_DigestFinal_ex(ctx.get(),d.data(),&n)==1 && n==32,"category map digest final");return d;}
};
}
std::vector<CategoryTile>categoryTiles(const Layout&l){
 std::vector<CategoryTile>out;
 for(std::size_t k=0;k<l.roots.size();++k){const auto&r=l.roots[k];need(r.category<5 && r.category_offset<=l.categories[r.category].actual && r.actual<=l.categories[r.category].actual-r.category_offset,"category root bounds");
  auto logical=l.categories[r.category].logical_offset+r.category_offset;std::size_t begin=0;
  while(begin<r.actual){std::size_t n=1;while(n<(1u<<20) && n<=(r.actual-begin)/2 && (logical+begin)%(2*n)==0)n*=2;out.push_back({{logical+begin,n},k,begin});begin+=n;}
 }
 std::sort(out.begin(),out.end(),[](const CategoryTile&a,const CategoryTile&b){return a.segment.offset<b.segment.offset;});return out;
}
std::vector<Fr>Commitments::categoryKernel(std::size_t root,std::size_t begin,std::size_t count,const std::vector<Fr>&rho,const Authentication&auth)const{
 need(root<roots_.size() && begin<=roots_[root].spec.actual && count<=roots_[root].spec.actual-begin && count<=(1u<<20),"category bounded K tile");
 ++auth_metrics_.kernel_calls;auth_metrics_.kernel_peak_bytes=std::max<std::uint64_t>(auth_metrics_.kernel_peak_bytes,count*sizeof(Fr));std::vector<Fr>out(count,Fr(0));
 for(std::size_t i=0;i<claims_.size();++i){const auto&c=claims_[i];const auto&p=auth.claims[i];if(c.only_root>=0 && std::size_t(c.only_root)!=root)continue;if(p.frozen && !p.hasRoot(root))continue;
  std::unique_ptr<Weights>scratch;if(!p.weights)scratch=std::make_unique<Weights>(c.point,&auth_metrics_);const auto&w=p.weights?*p.weights:*scratch;
  if(p.frozen)p.forRoot(root,auth.store,[&](const mapping::Run&r){
   const std::int64_t first=r.first.index,lo=begin,hi=begin+count;std::size_t a=0,b=r.count;
   if(r.stride>0){auto d=r.stride;if(lo>first)a=std::min(r.count,std::size_t((lo-first+d-1)/d));b=hi>first?std::min(r.count,std::size_t((hi-1-first)/d+1)):0;}
   else if(r.stride<0){auto d=-r.stride;if(first>=hi)a=std::min(r.count,std::size_t((first-hi)/d+1));b=first>=lo?std::min(r.count,std::size_t((first-lo)/d+1)):0;}
   else if(first<lo || first>=hi)return;
   if(a>=b)return;
   auto at=first+std::int64_t(a)*r.stride;w.scaled(r.begin+a*r.logical_step,b-a,rho[i],[&](const Fr&value){need(at>=lo && at<hi,"category K clipped map");out[std::size_t(at-lo)]+=value;at+=r.stride;},r.logical_step);auth_metrics_.kernel_coordinates+=b-a;
  });
  else{
   // The canonical map digest was rebuilt by prepareClaim immediately before
   // rho. A callback fallback retains the same bounds and map obligations.
   ConsumedMapDigest digest;
   for(std::size_t j=0;j<c.padded;++j){auto at=c.map(j);digest.append(at);if(!at.constant && at.coordinate.root==root && at.coordinate.index>=begin && at.coordinate.index-begin<count)out[at.coordinate.index-begin]+=rho[i]*w.at(j);}
   need(digest.finish()==c.map_digest,"category callback source map changed after defer");
  }
 }
 return out;
}
std::optional<Fr>Commitments::categoryKernelEndpoint(const std::vector<Fr>&q,const std::vector<Fr>&rho,const Authentication&auth,bool require_profitable)const{
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
void Commitments::authenticateCategoryInput(const std::vector<Fr>&x){
 namespace ui=unified;auto started=std::chrono::steady_clock::now();
 need(layout_.category_mode && single_pc_ && sealed_ && tails_constrained_ && !authenticated_ && io_.session()==session_ && identity(layout_)==identity_ && x==single_x_,"category authentication lifecycle/statement");
 need(single::descriptor(singlePlan(layout_,x))==single::descriptor(single_pc_->plan),"category sealed plan");
 // C_U and the verifier-owned statement/map are fixed before r_X is sampled.
 Writer xb;xb.text(single::suite);xb.text(single::identity(*single_pc_));xb.text(identity_);xb.number(x.size(),8);for(const auto&v:x){need(v.isValid(),"category public X scalar");xb.scalar(v);}bindPublic(io_,"CategoryInput/X-statement-and-seal",xb.b);
 std::vector<Fr>rx;const auto padded=layout_.categories[0].padded;for(unsigned j=0;j<single::bits(padded);++j)rx.push_back(io_.coin("CategoryInput/X-query/"+std::to_string(j)));
 Weights wx(rx);Fr expected=0;for(std::size_t j=0;j<x.size();++j)expected+=wx.at(j)*x[j];
 auto spans=layout_.x_spans;auto xindex=[spans](std::size_t j){for(const auto&s:spans){if(j<s.count)return s.offset+j;j-=s.count;}throw std::runtime_error("category X trusted map omission");};
 deferBase("public/X-committed-consistency",x.size(),padded,xindex,publicHidden(io_,"CategoryInput/X-expected",expected),rx);
 need(names_.count("public/X-committed-consistency")==1 && !claims_.empty() && claims_.size()<=8192,"category mandatory X claim");
 auto tile_started=std::chrono::steady_clock::now();auto plan=categoryTiles(layout_);
 std::cerr<<"CATEGORY_TILE_DETAIL {\"tile_enumeration_count\":1,\"tiles\":"<<plan.size()<<",\"tile_mapping_time\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-tile_started).count()<<"}\n";std::vector<ui::Segment>segments;for(const auto&t:plan)segments.push_back(t.segment);ui::validate(single_pc_->plan.domain,segments);
 Writer declaration;declaration.text(single::suite);declaration.text(single::identity(*single_pc_));declaration.text(identity_);declaration.number(claims_.size());for(const auto&c:claims_){declaration.text(c.id);declaration.raw(Bytes(c.map_digest.begin(),c.map_digest.end()));}
 // Tiles are derived local computation views; no tile commitment or independent
 // claim is ever sent. Their descriptor is bound as an execution consistency aid.
 declaration.number(segments.size());for(auto s:segments){declaration.number(s.offset,8);declaration.number(s.padded,8);}bindPublic(io_,"CategoryInput/all-claims-fixed",declaration.b);
 Authentication auth(claims_.size(),layout_.master_actual!=0);std::vector<Fr>biases;
 for(std::size_t i=0;i<claims_.size();++i)biases.push_back(prepareClaim(claims_[i],auth,i));
 std::vector<Fr>rho;std::vector<Hidden>values;Fr bias=0;
 for(std::size_t i=0;i<claims_.size();++i){rho.push_back(io_.coin("CategoryInput/rho/"+std::to_string(i)));need(!rho.back().isZero(),"category zero aggregation");values.push_back(claims_[i].value);bias+=rho.back()*biases[i];}
 auto initial=combine(session_,"CategoryInput/initial",values,rho);initial.value-=bias;initial.handle.point-=scalarCommit(bias,Fr(0));bindPublic(io_,"CategoryInput/initial",encode(initial.handle));
 auto generate=[&](std::size_t k){const auto&t=plan.at(k);return categoryKernel(t.root,t.begin,t.segment.padded,rho,auth);};
 std::unique_ptr<ui::PublicK>state;if(io_.prover())state=std::make_unique<ui::PublicK>(segments,generate,ui::KBudget::environment(),session_,single_pc_->plan.domain);
 ui::Factors factors;ui::Inputs inputs;if(io_.prover()){
  factors=[&](std::size_t k,const ui::Consume&consume){const auto&t=plan.at(k);auto a=[&,root=t.root,begin=t.begin](std::size_t j){return roots_[root].value(begin+j);};state->withRoot(k,session_,[&](const Value&b){consume(a,b);});};
  inputs=[&](std::size_t k){const auto&t=plan.at(k);ui::InputSpan s;s.stored=s.padded=t.segment.padded;s.storage_identity=&roots_[t.root];s.storage_offset=t.begin;s.read=[&,root=t.root,begin=t.begin](std::size_t j){return roots_[root].value(begin+j);};return s;};
 }
 bool fast_endpoint=true;if(const auto*v=std::getenv("ZKGPT_CATEGORY_K_ENDPOINT")){std::string text(v);need(text=="0" || text=="1","category K endpoint switch");fast_endpoint=text=="1";}
 auto endpoint=[&](const std::vector<Fr>&q){if(fast_endpoint){auto out=categoryKernelEndpoint(q,rho,auth);if(out)return *out;}Fr out=0;for(std::size_t k=0;k<plan.size();++k){auto v=generate(k);auto width=v.size();unsigned j=0;for(;width>1;width/=2,++j)for(std::size_t a=0;a<width/2;++a)v[a]=v[2*a]+q[j]*(v[2*a+1]-v[2*a]);out+=ui::selector(segments[k],q)*v[0];}return out;};
 auto reduced=ui::reduce(io_,single_pc_->plan.domain,segments,initial,factors,endpoint,rng_,"UnifiedInput/sumcheck",state.get(),inputs);if(state)state->report();state.reset();
 single::open(io_,*single_pc_,{reduced.point,reduced.endpoints[0].handle},x,io_.prover()?&*single_secret_:nullptr,io_.prover()?&reduced.endpoints[0]:nullptr,rng_,"UnifiedInput/opening/C_U");
 Writer done;done.text(single::suite);done.text(single::identity(*single_pc_));done.text("public/X-committed-consistency");done.number(claims_.size());bindPublic(io_,"CategoryInput/all-claims-authenticated",done.b);io_.coin("CategoryInput/all-claims-authenticated/ack");authenticated_=true;
 std::cerr<<"CATEGORY_INPUT {\"claims\":"<<claims_.size()<<",\"category_count\":"<<single_pc_->plan.segments.size()<<",\"computation_tiles\":"<<plan.size()<<",\"storage_chunks\":"<<roots_.size()<<",\"K_tile_peak_bound_bytes\":33554432,\"full_witness_copy_bytes\":0,\"X_claim_authenticated\":true,\"seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<",\"production_gate\":\"BLOCKED_CRYPTOGRAPHIC_DESIGN\",\"may_enter_stage2\":false}\n";
}
}
