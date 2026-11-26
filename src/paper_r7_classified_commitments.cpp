#include "paper_r7_classified_commitments.hpp"
#include "paper_r7_stage2_opening.hpp"
#include "paper_r7_unified_input.hpp"
#include "paper_r7_public_k.hpp"
#include "paper_r7_k_terminal.hpp"
#include "paper_r7_static_source_map.hpp"
#include "paper_r7_native_workers.hpp"
#include <algorithm>
#include <atomic>
#include <openssl/evp.h>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <numeric>
#include <cstring>
#include <sys/resource.h>
namespace zkgpt::r7::oneblock::classified {
namespace {
using Clock=std::chrono::steady_clock;
void rootTiming(const char*phase,const std::string&id,Clock::time_point started){struct rusage usage{};getrusage(RUSAGE_SELF,&usage);std::cerr<<"OPT1_ROOT {\"phase\":\""<<phase<<"\",\"id\":\""<<id<<"\",\"seconds\":"<<std::chrono::duration<double>(Clock::now()-started).count()<<",\"cumulative_peak_rss_kib\":"<<usage.ru_maxrss<<"}\n";}
constexpr std::size_t map_record_bytes=41;
constexpr std::size_t reference_buffer_bytes=32u<<10;
constexpr std::size_t mapping_buffer_bytes=64u<<10;
bool serialMapReference(){
 const auto*mode=std::getenv("ZKGPT_PERF3_MAP_REFERENCE");
 return mode && std::strcmp(mode,"1")==0;
}
void putMapIndex(unsigned char*out,std::uint32_t n){
 for(unsigned byte=0;byte<4;++byte)out[byte]=std::uint8_t(n>>(8*byte));
}
class MapHash {
 std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)>ctx_{EVP_MD_CTX_new(),EVP_MD_CTX_free};
 std::vector<unsigned char>buffer_;std::size_t used_=0;
 AuthMetrics*metrics_;
 std::array<unsigned char,32>bias_bytes_{};Fr bias_;bool have_bias_=false,reuse_bias_;
 void flush(){if(used_){const auto started=metrics_?Clock::now():Clock::time_point{};need(EVP_DigestUpdate(ctx_.get(),buffer_.data(),used_)==1,"classified map digest update");if(metrics_){++metrics_->maphash_updates;metrics_->maphash_bytes+=used_;metrics_->maphash_update_seconds+=std::chrono::duration<double>(Clock::now()-started).count();}used_=0;}}
 public:explicit MapHash(AuthMetrics*m=nullptr,std::size_t buffer_bytes=reference_buffer_bytes):
  buffer_(buffer_bytes),metrics_(m),reuse_bias_(!serialMapReference()){
  need(buffer_bytes>=map_record_bytes && ctx_ && EVP_DigestInit_ex(ctx_.get(),EVP_sha256(),nullptr)==1,"classified map digest init");
 }
 void append(const Located&l){
  if(used_+41>buffer_.size()){flush();}
  buffer_[used_++]=l.constant?1:0;for(auto n:{l.coordinate.root,l.coordinate.index})for(unsigned j=0;j<4;++j)buffer_[used_++]=std::uint8_t(n>>(8*j));
  // Only canonical bytes of an exactly equal, previously validated public Fr
  // are reused. Coordinates and the complete hash stream are always rebuilt.
  if(reuse_bias_ && have_bias_ && l.bias==bias_){std::memcpy(buffer_.data()+used_,bias_bytes_.data(),32);if(metrics_)++metrics_->map_bias_reuses;}
  else{
   need(l.bias.isValid() && l.bias.serialize(buffer_.data()+used_,32)==32,"classified canonical map bias");if(metrics_)++metrics_->map_bias_serializations;
   if(reuse_bias_){bias_=l.bias;std::memcpy(bias_bytes_.data(),buffer_.data()+used_,32);have_bias_=true;}
  }used_+=32;
 }
 // Each run has one fixed record prefix and one canonical bias encoding.
 // For physical runs only the four little-endian index bytes vary.
 void appendContiguous(Located l,std::size_t count){
  if(!count)return;
  if(!reuse_bias_){for(std::size_t j=0;j<count;++j){append(l);if(!l.constant)++l.coordinate.index;}return;}
  append(l);
  if(--count==0)return;
  std::array<unsigned char,map_record_bytes>record;
  std::memcpy(record.data(),buffer_.data()+used_-map_record_bytes,record.size());
  auto next_index=static_cast<std::uint32_t>(l.coordinate.index+1);
  for(;count;){
   if(used_+map_record_bytes>buffer_.size())flush();
   const auto n=std::min(count,(buffer_.size()-used_)/map_record_bytes);
   auto*out=buffer_.data()+used_;
   if(l.constant){
    for(std::size_t j=0;j<n;++j,out+=map_record_bytes)std::memcpy(out,record.data(),record.size());
   }else{
    for(std::size_t j=0;j<n;++j,out+=map_record_bytes){
     std::memcpy(out,record.data(),9);
     putMapIndex(out+5,next_index++);
     std::memcpy(out+9,bias_bytes_.data(),bias_bytes_.size());
    }
   }
   used_+=map_record_bytes*n;
   count-=n;
   if(metrics_)metrics_->map_bias_reuses+=n;
  }
 }
 Digest finish(){flush();Digest d{};unsigned n=0;need(EVP_DigestFinal_ex(ctx_.get(),d.data(),&n)==1 && n==32,"classified map digest end");return d;}
};

// A descriptor is borrowed from the trusted layout/circuit for this session.
// The streaming cursor avoids storing terminal runs alongside the frozen plan.
struct MappingDescriptor {
 const void*identity;
 const StaticSourceMap*source;
 const std::vector<std::uint32_t>*terminal;
 std::size_t padded;
 int only_root;
 std::size_t bias_runs=0;
 const std::string*view_id=nullptr;
};

class MappingSpans {
 const MappingDescriptor&descriptor_;
 const StaticBaseMap&base_;
 std::size_t next_=0,source_run_=0;
 public:
 MappingSpans(const MappingDescriptor&descriptor,const StaticBaseMap&base):descriptor_(descriptor),base_(base){}
 bool next(StaticSourceMap::Run&run){
  if(next_==descriptor_.padded)return false;
  if(descriptor_.source){
   run=descriptor_.source->runs().at(source_run_++);
  }else{
   const auto&indices=*descriptor_.terminal;
   if(next_>=indices.size()){
    run={next_,descriptor_.padded-next_,{{},Fr(0),true}};
   }else{
    const auto first=indices[next_];
    const auto&pieces=base_.runs();
    auto it=std::upper_bound(pieces.begin(),pieces.end(),first,
      [](std::size_t at,const Piece&piece){return at<piece.begin;});
    need(it!=pieces.begin(),"classified direct terminal prefix");
    const auto&piece=*--it;
    const auto offset=first-piece.begin;
    need(offset<piece.count,"classified direct terminal coordinate");
    const auto limit=std::min(indices.size()-next_,piece.count-offset);
    std::size_t count=1;
    while(count<limit && indices[next_+count]==std::size_t(first)+count)++count;
    run={next_,count,{{piece.start.root,std::uint32_t(piece.start.index+offset)},Fr(0),false}};
   }
  }
  need(run.begin==next_ && run.count && run.count<=descriptor_.padded-next_,"classified direct map coverage");
  next_+=run.count;
  return true;
 }
};

using CandidateKey=std::array<std::uint64_t,13>;
CandidateKey candidateKey(const MappingDescriptor&d,const StaticBaseMap&base){
 CandidateKey key{d.padded,std::uint64_t(d.only_root)};
 for(std::size_t i=2;i<key.size();++i){
  const auto j=(d.padded-1)*(i-2)/(key.size()-3);
  auto loc=d.source?d.source->at(j):
   j<d.terminal->size()?Located{base.at((*d.terminal)[j]),Fr(0),false}:Located{{},Fr(0),true};
  key[i]=loc.constant?std::uint64_t(-1):(std::uint64_t(loc.coordinate.root)<<32)|loc.coordinate.index;
 }
 return key;
}

bool sameMapping(const MappingDescriptor&a,const MappingDescriptor&b,const StaticBaseMap&base){
 if(a.padded!=b.padded || a.only_root!=b.only_root)return false;
 if(a.terminal && b.terminal && *a.terminal==*b.terminal)return true;
 MappingSpans left(a,base),right(b,base);
 StaticSourceMap::Run x,y;
 left.next(x);right.next(y);
 std::size_t j=0;
 while(j<a.padded){
  const auto&xl=x.first;
  const auto&yl=y.first;
  if(xl.constant!=yl.constant || xl.bias!=yl.bias || xl.coordinate.root!=yl.coordinate.root)return false;
  if(xl.constant ? xl.coordinate.index!=yl.coordinate.index :
     std::uint64_t(xl.coordinate.index)+j-x.begin!=std::uint64_t(yl.coordinate.index)+j-y.begin)return false;
  j=std::min(x.begin+x.count,y.begin+y.count);
  if(j==x.begin+x.count)left.next(x);
  if(j==y.begin+y.count)right.next(y);
 }
 return true;
}

Fr fold(std::vector<Fr>&v,const std::vector<Fr>&q){std::size_t n=v.size();need(r1::log2exact(n)==q.size(),"classified public kernel query");for(auto&x:q){for(std::size_t j=0;j<n/2;++j)v[j]=v[2*j]+x*(v[2*j+1]-v[2*j]);n/=2;}return v[0];}
std::vector<unified::Block>unifiedBlocks(const Layout&p){std::vector<unified::Block>b;for(auto&r:p.roots){unsigned category=r.model?1:(r.id.rfind("C_aux",0)==0?2:3);need(r.model || r.id.rfind("C_aux",0)==0 || r.id.rfind("C_lookup/",0)==0,"unified unknown classified root category");b.push_back({category,r.padded});}return b;}
std::string legacyUnifiedInputIdentity(const Layout&p,const std::vector<Fr>&x,const unified::Packing&packing,const Bytes&descriptor){
 const auto&segments=packing.segments();const auto&categories=packing.categories();
 Writer w;w.raw(descriptor);w.text(vectorSuite);w.text(vectorLayout);w.text(identity(p));w.number(x.size(),8);for(auto&v:x){need(v.isValid(),"unified public X identity scalar");w.scalar(v);}w.number(p.roots.size());for(std::size_t k=0;k<p.roots.size();++k){auto&r=p.roots[k];w.text(r.id);w.number(categories[k+1]);w.number(r.actual,8);w.number(r.padded,8);w.number(segments[k+1].offset,8);}return hex(r1::hash(w.b));}
}
std::string unifiedInputIdentity(const Layout&p,const std::vector<Fr>&x){if(p.category_mode){Writer w;w.text(single::suite);w.raw(single::descriptor(singlePlan(p,x)));w.number(x.size(),8);for(const auto&v:x)w.scalar(v);return hex(r1::hash(w.b));}
 need(!x.empty() && x.size()<=(1u<<24),"unified public X identity bound");
 const auto packing=unified::pack(unifiedBlocks(p),power2(x.size()));
 return legacyUnifiedInputIdentity(p,x,packing,unified::descriptor(packing));
}
// All cached state is owned by one stack-scoped authentication session.
// Neither a caller callback nor Layout storage is retained in a frozen map.
Commitments::Weights::Weights(const std::vector<Fr>&q,AuthMetrics*m):split_(q.size()/2){auto t=processCPU();a_=r1::weights({q.begin(),q.begin()+split_});b_=r1::weights({q.begin()+split_,q.end()});if(m){++m->weights;m->weight_terms+=a_.size()+b_.size();m->weights_cpu+=processCPU()-t;}}
std::size_t Commitments::Weights::bytes()const{return (a_.capacity()+b_.capacity())*sizeof(Fr);}
Fr Commitments::Weights::at(std::size_t i)const{return a_[i&(a_.size()-1)]*b_[i>>split_];}

bool Commitments::usesBaseCoordinates(const Claim&c)const{
 if(c.static_locator)return false;
 // Built-in native-terminal and mapped-view callbacks perform exactly one
 // baseCoordinate lookup for each nonconstant location. Custom test maps do not.
 auto it=layout_.views.find(c.map_id);return c.map_id.rfind("native-base/",0)==0 || (it!=layout_.views.end() && it->second.mapped);
}
void Commitments::prepareStaticMappings(const std::vector<TerminalMapping>&terminals){
 if(!layout_.category_mode || !static_source_enabled_ || serialMapReference() || !single_public_coordinates_.empty())return;
 const auto start=Clock::now();
 need(!category_failed_ && sealed_ && tails_constrained_ && !authenticated_ &&
      io_.session()==session_ && identity(layout_)==identity_,"category static mapping session lifecycle");
 if(!category_auth_){
  category_auth_=std::make_unique<Authentication>(claims_.size(),layout_.master_actual!=0);
  category_auth_->session=session_;
  category_auth_->transcript=io_.transcriptOwner();
 }
 auto&auth=*category_auth_;
 need(!auth.static_prepared && auth.session==session_ && auth.transcript.lock()==io_.transcriptOwner(),
      "category static mapping already prepared or cross-session");
 try{
  const auto base=staticBase();
  std::vector<MappingDescriptor>descriptors;
  descriptors.reserve(layout_.views.size()+terminals.size());
  std::size_t bias_allowed=Authentication::bias_budget-auth.bias_bytes;
  for(const auto&[id,view]:layout_.views){
   if(!view.mapped && view.pieces.empty())continue;
   if(auth.templates.count(&view))continue;
   auto&source=static_sources_[id];
   if(!source){
    source=std::make_shared<const StaticSourceMap>(layout_,view,*base);
    ++auth_metrics_.static_source_builds;
    auth_metrics_.static_locator_bytes+=source->bytes();
   }
   const auto nonzero=std::count_if(source->runs().begin(),source->runs().end(),
      [](const auto&run){return !run.first.bias.isZero();});
   const auto bytes=std::size_t(nonzero)*sizeof(Authentication::MappingTemplate::BiasRun);
   // The existing query-local path handles unusually fragmented bias plans.
   if(bytes>bias_allowed)continue;
   bias_allowed-=bytes;
   const int hint=!view.mapped && view.pieces.size()<=1?int(view.start.root):-1;
   descriptors.push_back({&view,source.get(),nullptr,view.padded,hint,std::size_t(nonzero),&id});
  }
  std::map<const void*,std::size_t>seen_terminals;
  for(const auto&[indices,padded]:terminals){
   need(indices && !indices->empty() && indices->size()<=padded && padded<=r1::kMaxVector,
        "category static terminal shape");
   const auto [seen,inserted]=seen_terminals.emplace(indices,padded);
   need(seen->second==padded,"category static terminal descriptor shape");
   if(!inserted || auth.templates.count(indices))continue;
   descriptors.push_back({indices,nullptr,indices,padded,-1});
  }
  const auto describe_end=Clock::now();
  std::map<CandidateKey,std::vector<std::size_t>>candidates;
  std::vector<std::size_t>canonical(descriptors.size()),jobs;
  std::uint64_t dedup_bytes=0,dedup_updates=0,comparisons=0;
  double key_seconds=0,compare_seconds=0;
  constexpr auto records_per_update=mapping_buffer_bytes/map_record_bytes;
  for(std::size_t j=0;j<descriptors.size();++j){
   const auto&descriptor=descriptors[j];
   const auto key_start=Clock::now();
   auto&group=candidates[candidateKey(descriptor,*base)];
   key_seconds+=std::chrono::duration<double>(Clock::now()-key_start).count();
   const auto compare_start=Clock::now();
   const auto known=std::find_if(group.begin(),group.end(),[&](auto i){
    ++comparisons;
    return sameMapping(descriptors[i],descriptor,*base);
   });
   compare_seconds+=std::chrono::duration<double>(Clock::now()-compare_start).count();
   if(known==group.end()){
    canonical[j]=j;
    group.push_back(j);
    jobs.push_back(j);
   }else{
    canonical[j]=*known;
    dedup_bytes+=descriptor.padded*map_record_bytes;
    dedup_updates+=(descriptor.padded+records_per_update-1)/records_per_update;
   }
  }
  std::stable_sort(jobs.begin(),jobs.end(),[&](auto a,auto b){
   return descriptors[a].padded>descriptors[b].padded;
  });
  struct alignas(64) Result {
   std::shared_ptr<Authentication::MappingTemplate> value;
   AuthMetrics metrics;
   double seconds=0,encoding_seconds=0;
   unsigned worker=0;
  };
  std::vector<Result>results(jobs.size());
  const auto index_bytes=auth.store.bytes;
  const auto reserve_per_builder=mapping::block_runs*sizeof(mapping::Run)+
      roots_.size()*(sizeof(mapping::RootSpan)+sizeof(std::size_t));
  double hash_wall=0,publish_wall=0,critical=0,encoding=0,total_job_seconds=0;
  std::array<double,detail::NativeWorkers::capacity>worker_active{};
  std::array<unsigned,detail::NativeWorkers::capacity>worker_jobs{};
  unsigned max_workers=0;
  const auto run_job=[&](std::size_t slot,std::size_t budget,unsigned worker){
   const auto job_start=Clock::now();
   const auto&d=descriptors[jobs[slot]];
   auto&out=results[slot];
   out.worker=worker;
   auto value=std::make_shared<Authentication::MappingTemplate>();
   value->padded=d.padded;
   value->only_root=d.only_root;
   value->bias_runs.reserve(d.bias_runs);
   mapping::Builder builder(*value,auth.store,budget,roots_.size());
   MapHash digest(&out.metrics,mapping_buffer_bytes);
   MappingSpans spans(d,*base);
   StaticSourceMap::Run run;
   const auto fail=[&]{
    return std::runtime_error(std::string("category mapping freeze failed: ")+value->reason);
   };
   auto&metrics=out.metrics;
   while(spans.next(run)){
    const auto&loc=run.first;
    if(!loc.constant){
     const auto actual=roots_.at(loc.coordinate.root).spec.actual;
     need(loc.coordinate.index<actual && run.count<=actual-loc.coordinate.index,"classified direct map span bounds");
    }
    if(d.only_root>=0)need(!loc.constant && loc.bias.isZero() && loc.coordinate.root==std::size_t(d.only_root),"classified invalid root-only map hint");
    const auto encode_start=Clock::now();
    digest.appendContiguous(loc,run.count);
    out.encoding_seconds+=std::chrono::duration<double>(Clock::now()-encode_start).count();
    if(!loc.bias.isZero()){
     auto&bias=value->bias_runs;
     if(!bias.empty() && bias.back().begin+bias.back().count==run.begin && bias.back().value==loc.bias)
      bias.back().count+=run.count;
     else bias.push_back({run.begin,run.count,loc.bias});
    }
    if(!loc.constant){
     builder.appendContiguous(run.begin,run.count,loc.coordinate);
     if(builder.failed())throw fail();
    }
    metrics.mapping_ram_peak=std::max<std::uint64_t>(metrics.mapping_ram_peak,builder.workingBytes());
    ++metrics.direct_map_runs;
    metrics.direct_map_coordinates+=run.count;
   }
   const auto finish_start=Clock::now();
   value->digest=digest.finish();
   out.encoding_seconds+=std::chrono::duration<double>(Clock::now()-finish_start).count();
   out.encoding_seconds-=metrics.maphash_update_seconds;
   builder.finish(true);
   if(!value->frozen)throw fail();
   out.value=std::move(value);
   out.seconds=std::chrono::duration<double>(Clock::now()-job_start).count();
  };
  for(std::size_t first=0;first<jobs.size();){
   const auto available=auth.map_budget-auth.map_bytes;
   const auto batch=std::min(jobs.size()-first,std::max<std::size_t>(1,available/reserve_per_builder));
   const auto budget=std::min(reserve_per_builder,available/batch);
   const auto begin=Clock::now();
   unsigned participants=1;
   if(batch>1 && descriptors[jobs[first]].padded*map_record_bytes>=4096){
    auto&pool=detail::nativeWorkers();
    participants=unsigned(std::min<std::size_t>(batch,pool.count()));
    std::atomic<std::size_t>next{0};
    pool.run(participants,[&](std::size_t worker){
     for(auto j=next.fetch_add(1,std::memory_order_relaxed);j<batch;
         j=next.fetch_add(1,std::memory_order_relaxed))run_job(first+j,budget,unsigned(worker));
    },participants>1);
   }else{
    for(std::size_t j=0;j<batch;++j)run_job(first+j,budget,0);
   }
   const auto joined=Clock::now();
   hash_wall+=std::chrono::duration<double>(joined-begin).count();
   max_workers=std::max(max_workers,participants);
   for(std::size_t j=first;j<first+batch;++j){
    auto&out=results[j];
    const auto&m=out.metrics;
    auto&total=auth_metrics_;
    const auto&value=*out.value;
    auth.map_bytes+=value.bytes();
    auth.bias_bytes+=value.bias_runs.capacity()*sizeof(Authentication::MappingTemplate::BiasRun);
    total.mapping_ram_peak=std::max<std::uint64_t>(total.mapping_ram_peak,auth.map_bytes);
    total.map_calls++;
    total.map_coordinates+=value.padded;
    total.template_runs+=value.run_count;
    total.template_cache_bytes+=sizeof(value)+value.bytes()+value.bias_runs.capacity()*sizeof(Authentication::MappingTemplate::BiasRun);
    total.direct_map_runs+=m.direct_map_runs;
    total.direct_map_coordinates+=m.direct_map_coordinates;
    total.maphash_bytes+=m.maphash_bytes;
    total.maphash_updates+=m.maphash_updates;
    total.maphash_update_seconds+=m.maphash_update_seconds;
    total.map_bias_serializations+=m.map_bias_serializations;
    total.map_bias_reuses+=m.map_bias_reuses;
    critical=std::max(critical,out.seconds);
    encoding+=out.encoding_seconds;
    total_job_seconds+=out.seconds;
    worker_active[out.worker]+=out.seconds;
    ++worker_jobs[out.worker];
   }
   publish_wall+=std::chrono::duration<double>(Clock::now()-joined).count();
   first+=batch;
  }
  {
   const auto begin=Clock::now();
   std::vector<std::size_t>result_index(descriptors.size());
   for(std::size_t j=0;j<jobs.size();++j)result_index[jobs[j]]=j;
   // Workers are joined. Publish aliases in the original descriptor order.
   for(std::size_t j=0;j<descriptors.size();++j)
    auth.templates.emplace(descriptors[j].identity,results[result_index[canonical[j]]].value);
   publish_wall+=std::chrono::duration<double>(Clock::now()-begin).count();
  }
  auth.static_prepared=true;
  auth_metrics_.bias_descriptor_bytes=auth.bias_bytes;
  auth_metrics_.mapping_file_peak=std::max(auth_metrics_.mapping_file_peak,auth.store.bytes);
  const auto elapsed=std::chrono::duration<double>(Clock::now()-start).count();
  auth_metrics_.map_wall+=elapsed;
  auth_metrics_.map_cpu+=total_job_seconds;
  const auto bytes=auth.store.bytes-index_bytes;
  std::cerr<<std::setprecision(12)
   <<"MAPPING_PARALLEL_BATCH {\"descriptors\":"<<descriptors.size()
   <<",\"unique\":"<<jobs.size()<<",\"reused_descriptors\":"<<(descriptors.size()-jobs.size())
   <<",\"avoided_bytes\":"<<dedup_bytes<<",\"avoided_updates\":"<<dedup_updates
   <<",\"key_seconds\":"<<key_seconds<<",\"comparisons\":"<<comparisons
   <<",\"compare_seconds\":"<<compare_seconds<<",\"workers\":"<<max_workers
   <<",\"buffer_bytes\":"<<mapping_buffer_bytes<<",\"encoding_thread_seconds\":"<<encoding
   <<",\"task_thread_seconds\":"<<total_job_seconds<<",\"hash_and_freeze_wall_seconds\":"<<hash_wall
   <<",\"describe_wall_seconds\":"<<std::chrono::duration<double>(describe_end-start).count()
   <<",\"publish_wall_seconds\":"<<publish_wall<<",\"critical_task_seconds\":"<<critical
   <<",\"file_bytes\":"<<bytes<<",\"bias_bytes\":"<<auth.bias_bytes
   <<",\"buffer_bytes_upper\":"<<max_workers*mapping_buffer_bytes
   <<",\"temporary_task_bytes\":"<<results.capacity()*sizeof(Result)+descriptors.capacity()*sizeof(MappingDescriptor)
   <<",\"wall_seconds\":"<<elapsed<<"}\n";
  for(std::size_t j=0;j<jobs.size();++j){
   const auto&d=descriptors[jobs[j]];
   const auto&out=results[j];
   std::cerr<<"MAPPING_PARALLEL_TEMPLATE {\"ordinal\":"<<jobs[j]
    <<",\"family\":"<<std::quoted(d.source?"view":"terminal")
    <<",\"view_id\":"<<std::quoted(d.view_id?*d.view_id:"")
    <<",\"padded\":"<<d.padded<<",\"bytes\":"<<out.metrics.maphash_bytes
    <<",\"updates\":"<<out.metrics.maphash_updates
    <<",\"SHA_update_seconds\":"<<out.metrics.maphash_update_seconds
    <<",\"wall_seconds\":"<<out.seconds<<",\"worker\":"<<out.worker<<"}\n";
  }
  for(unsigned j=0;j<max_workers;++j)
   std::cerr<<"MAPPING_PARALLEL_WORKER {\"id\":"<<j<<",\"jobs\":"<<worker_jobs[j]
    <<",\"active_seconds\":"<<worker_active[j]
    <<",\"idle_upper_seconds\":"<<std::max(0.0,hash_wall-worker_active[j])<<"}\n";
 }catch(...){
  category_failed_=true;
  category_auth_.reset();
  throw;
 }
}
void Commitments::prepareCategoryClaim(Claim&c,Authentication&auth,std::size_t i,const void*descriptor,const StaticSourceMap*source,const std::vector<std::uint32_t>*indices){
 need(!category_failed_ && auth.session==session_ && auth.transcript.lock()==io_.transcriptOwner() && io_.session()==session_,"category prepared mapping cross-session/failed lifecycle");
 const auto started=processCPU();
 const auto wall=Clock::now();
 auto&p=auth.claims.at(i);need(!p.ready,"category mapping already prepared");
 auto weights=std::make_unique<Weights>(c.point,&auth_metrics_);
 auto existing=descriptor?auth.templates.find(descriptor):auth.templates.end();
 bool hit=false;
 Fr bias=0;
 auto accumulateBias=[&](std::size_t j,std::size_t count,const Fr&value){
  while(count){
   if(count<64){
    for(std::size_t k=0;k<count;++k){bias+=weights->at(j+k)*value;++auth_metrics_.weight_products;}
    break;
   }
   unsigned free=0;
   for(std::size_t n=1;n<=count/2 && !(j&n);n*=2)++free;
   auto term=value;
   for(unsigned bit=free;bit<c.point.size();++bit)
    term*=((j>>bit)&1)?c.point[bit]:Fr(1)-c.point[bit];
   bias+=term;auth_metrics_.weight_products+=c.point.size()-free;
   const auto block=std::size_t(1)<<free;j+=block;count-=block;
  }
 };
 if(existing!=auth.templates.end()){
  p.mapping=existing->second;
  hit=!auth.claimed_templates.insert(p.mapping.get()).second;
  need(p.mapping->padded==c.padded && p.mapping->only_root==c.only_root,"category template descriptor shape");
  for(const auto&run:p.mapping->bias_runs)accumulateBias(run.begin,run.count,run.value);
 }else{
  need(auth.map_bytes<=auth.map_budget,"category shared mapping budget");
  auto mapping=std::make_shared<Authentication::MappingTemplate>();
  mapping->padded=c.padded;mapping->only_root=c.only_root;
  mapping::Builder builder(*mapping,auth.store,auth.map_budget-auth.map_bytes,roots_.size());
  MapHash digest(&auth_metrics_);
  auto failure=[&]{return std::runtime_error("category mapping freeze failed: claim="+c.id+" reason="+mapping->reason+" RAM_used="+std::to_string(auth.map_bytes+mapping->bytes())+" RAM_limit="+std::to_string(auth.map_budget)+" file_used="+std::to_string(auth.store.bytes)+" file_limit="+std::to_string(auth.store.limit));};
  const bool baseMapped=usesBaseCoordinates(c);
  bool cache=descriptor!=nullptr;
  const auto bias_limit=(Authentication::bias_budget-auth.bias_bytes)/sizeof(Authentication::MappingTemplate::BiasRun);
  auto&runs=mapping->bias_runs;
  auto addBias=[&](std::size_t j,std::size_t count,const Fr&value){
   if(value.isZero())return;
   accumulateBias(j,count,value);
   if(!cache)return;
   if(!runs.empty() && runs.back().begin+runs.back().count==j && runs.back().value==value)
    runs.back().count+=count;
   else if(runs.size()==bias_limit){decltype(mapping->bias_runs){}.swap(runs);cache=false;}
   else{
    if(runs.size()==runs.capacity())runs.reserve(std::min(bias_limit,std::max<std::size_t>(4,2*runs.capacity())));
    runs.push_back({j,count,value});
   }
  };
  auto updateStorage=[&]{
   auth_metrics_.mapping_ram_peak=std::max<std::uint64_t>(auth_metrics_.mapping_ram_peak,auth.map_bytes+builder.workingBytes());
   auth_metrics_.mapping_file_peak=std::max<std::uint64_t>(auth_metrics_.mapping_file_peak,auth.store.bytes);
   if(builder.failed())throw failure();
  };
  auto appendCoordinate=[&](std::size_t j,const Located&l){
   need(l.constant || (l.coordinate.root<roots_.size() && l.coordinate.index<roots_[l.coordinate.root].spec.actual),"classified map root/coordinate bounds");
   if(c.only_root>=0)need(!l.constant && l.bias.isZero() && l.coordinate.root==std::size_t(c.only_root),"classified invalid root-only map hint");
   digest.append(l);
   addBias(j,1,l.bias);
   if(!l.constant){builder.append(j,l.coordinate);updateStorage();}
  };
  std::size_t covered=0;
  auto appendSpan=[&](const StaticSourceMap::Run&run){
   const auto&l=run.first;
   need(run.begin==covered && run.count<=c.padded-covered,"classified direct map span shape");
   covered+=run.count;
   if(!l.constant && !single_public_roots_.empty() && single_public_roots_.at(l.coordinate.root)){
    for(auto j=run.begin;j<covered;++j){appendCoordinate(j,c.map(j));++p.callback_calls;}
    return;
   }
   if(!l.constant){
    const auto actual=roots_.at(l.coordinate.root).spec.actual;
    need(l.coordinate.index<actual && run.count<=actual-l.coordinate.index,"classified direct map span bounds");
   }
   if(c.only_root>=0)
    need(!l.constant && l.bias.isZero() && l.coordinate.root==std::size_t(c.only_root),"classified invalid root-only map hint");
   digest.appendContiguous(l,run.count);
   addBias(run.begin,run.count,l.bias);
   if(!l.constant){builder.appendContiguous(run.begin,run.count,l.coordinate);updateStorage();}
   ++auth_metrics_.direct_map_runs;
   auth_metrics_.direct_map_coordinates+=run.count;
  };
  if(source){
   for(const auto&run:source->runs())appendSpan(run);
   need(covered==c.padded,"classified direct map coverage");
  }else if(indices){
   const MappingDescriptor terminal{descriptor,nullptr,indices,c.padded,c.only_root};
   MappingSpans spans(terminal,*staticBase());
   StaticSourceMap::Run run;
   while(spans.next(run))appendSpan(run);
  }else for(std::size_t j=0;j<c.padded;++j){
   const auto l=c.map(j);++p.callback_calls;
   auth_metrics_.rowmajor_coordinate_lookups+=baseMapped && !l.constant;
   appendCoordinate(j,l);
  }
  mapping->digest=digest.finish();builder.finish(true);if(!mapping->frozen)throw failure();
  auth.map_bytes+=mapping->bytes();
  if(cache){
   auth.bias_bytes+=runs.capacity()*sizeof(runs.front());
   auth.templates.emplace(descriptor,mapping);
   auth.claimed_templates.insert(mapping.get());
  }
  p.mapping=std::move(mapping);p.full_scans=1;
  ++auth_metrics_.map_calls;auth_metrics_.map_coordinates+=c.padded;
  auth_metrics_.template_runs+=p.mapping->run_count;
  auth_metrics_.bias_descriptor_bytes=auth.bias_bytes;
  // Storage owned by cached templates. Container implementation overhead is separate.
  if(cache)auth_metrics_.template_cache_bytes+=sizeof(*p.mapping)+p.mapping->bytes()+runs.capacity()*sizeof(runs.front());
 }
 p.bias=bias;p.ready=true;c.map_digest=p.mapping->digest;
 if(weights->bytes()<=auth.weight_budget-auth.weight_bytes){auth.weight_bytes+=weights->bytes();p.weights=std::move(weights);}
 const auto elapsed=std::chrono::duration<double>(Clock::now()-wall).count();
 ++auth_metrics_.mapping_claims;auth_metrics_.template_hits+=hit;
 auth_metrics_.map_cpu+=processCPU()-started;auth_metrics_.map_wall+=elapsed;
 const auto phase=c.map_id.rfind("native-base/gkr/",0)==0?0:c.id.rfind("Classified/source/",0)==0?1:c.id.rfind("r7/",0)==0?2:3;
 auto&phase_stats=auth_metrics_.mapping_phases[phase];
 ++phase_stats.claims;phase_stats.hits+=hit;phase_stats.scanned+=p.callback_calls;phase_stats.avoided+=hit?c.padded:0;phase_stats.wall+=elapsed;
 const auto&plan=p.plan();
 auth_metrics_.frozen_ram_claims+=std::string(plan.reason)=="FROZEN_ROOT_RAM";auth_metrics_.frozen_file_claims+=std::string(plan.reason)=="FROZEN_ROOT_FILE";
 auth_metrics_.mapping_ram_peak=std::max<std::uint64_t>(auth_metrics_.mapping_ram_peak,auth.map_bytes);auth_metrics_.mapping_file_peak=std::max<std::uint64_t>(auth_metrics_.mapping_file_peak,auth.store.bytes);
 if(opt2MetricsEnabled())std::cerr<<"OPT3B_PLAN {\"claim\":"<<i<<",\"id\":"<<std::quoted(c.id)<<",\"status\":"<<std::quoted(plan.reason)<<",\"frozen\":true,\"template_hit\":"<<(hit?"true":"false")<<",\"runs\":"<<plan.run_count<<",\"RAM_bytes\":"<<plan.bytes()<<",\"shared_RAM_bytes\":"<<auth.map_bytes<<",\"file_bytes\":"<<auth.store.bytes<<",\"file_limit\":"<<auth.store.limit<<",\"mapping_callback_calls\":"<<p.callback_calls<<",\"complete_mapping_scans\":"<<p.full_scans<<",\"prepare_wall_seconds\":"<<elapsed<<"}\n";
 // The authenticated frozen stream is the only remaining mapping source.
 c.map={};
}
Fr Commitments::prepareClaim(const Claim&c,Authentication&auth,std::size_t i)const{
 if(layout_.category_mode){need(!category_failed_ && auth.session==session_ && auth.transcript.lock()==io_.transcriptOwner() && io_.session()==session_,"category prepared mapping cross-session/failed lifecycle");const auto&p=auth.claims.at(i);need(p.ready && p.mapping && p.mapping->frozen && p.mapping->digest==c.map_digest && !c.map,"category missing prepared frozen mapping");return p.bias;}
 auto started=processCPU();auto wall=Clock::now();++auth_metrics_.map_calls;auth_metrics_.map_coordinates+=c.padded;
 auto&p=auth.claims[i];mapping::Builder builder(p,auth.store,auth.map_budget-auth.map_bytes,roots_.size());
 auto weights=std::make_unique<Weights>(c.point,&auth_metrics_);Fr bias=0;
 // A static view owns an immutable map. Native base claims still call a supplied index.
 std::optional<MapHash> digest;
 if(!c.static_locator || c.map_id.rfind("native-base/",0)==0)digest.emplace(&auth_metrics_);
 const bool baseMapped=usesBaseCoordinates(c);
 for(std::size_t j=0;j<c.padded;++j){auto l=c.map(j);auth_metrics_.rowmajor_coordinate_lookups+=baseMapped && !l.constant;
  need(l.constant || (l.coordinate.root<roots_.size() && l.coordinate.index<roots_[l.coordinate.root].spec.actual),"classified map root/coordinate bounds");
  if(c.only_root>=0)need(!l.constant && l.bias.isZero() && l.coordinate.root==std::size_t(c.only_root),"classified invalid root-only map hint");
  if(digest)digest->append(l);
  if(c.only_root<0 && !l.bias.isZero()){bias+=weights->at(j)*l.bias;++auth_metrics_.weight_products;}
  if(!l.constant)builder.append(j,l.coordinate);
 }
 if(digest)need(digest->finish()==c.map_digest,"classified source map changed after defer");
 builder.finish();auth.map_bytes+=p.bytes();
 if(weights->bytes()<=auth.weight_budget-auth.weight_bytes){auth.weight_bytes+=weights->bytes();p.weights=std::move(weights);}
 if(opt2MetricsEnabled())std::cerr<<"OPT3B_PLAN {\"claim\":"<<i<<",\"id\":\""<<c.id<<"\",\"status\":\""<<p.reason<<"\",\"frozen\":"<<(p.frozen?"true":"false")<<",\"runs\":"<<p.run_count<<",\"RAM_bytes\":"<<p.bytes()<<",\"shared_RAM_bytes\":"<<auth.map_bytes<<",\"file_bytes\":"<<auth.store.bytes<<",\"file_limit\":"<<auth.store.limit<<"}\n";
 auth_metrics_.map_cpu+=processCPU()-started;auth_metrics_.map_wall+=std::chrono::duration<double>(Clock::now()-wall).count();return bias;
}
std::vector<Fr>Commitments::kernel(std::size_t k,const std::vector<Fr>&lambda,const Authentication&auth)const{
 auto started=processCPU();auto wall=Clock::now();crypto::AllocationTrace allocation;++auth_metrics_.kernel_calls;auto n=roots_.at(k).spec.padded;auth_metrics_.kernel_peak_bytes=std::max<std::uint64_t>(auth_metrics_.kernel_peak_bytes,n*sizeof(Fr));std::vector<Fr>result(n,Fr(0));std::uint64_t visited=0,matched=0,multiplies=0,fallback=0,frozen=0;
 for(std::size_t i=0;i<claims_.size();++i){auto&c=claims_[i];if(c.only_root>=0 && std::size_t(c.only_root)!=k)continue;auto&p=auth.claims[i];
  if(p.frozen && !p.hasRoot(k))continue;
  std::unique_ptr<Weights>scratch;if(!p.weights)scratch=std::make_unique<Weights>(c.point,&auth_metrics_);auto&w=p.weights?*p.weights:*scratch;
  if(p.frozen){p.forRoot(k,auth.store,[&](const auto&run){std::int64_t at=run.first.index;
    auto groups=w.scaled(run.begin,run.count,lambda[i],[&](const Fr&weight){result[std::size_t(at)]+=weight;at+=run.stride;},run.logical_step);
    visited+=run.count;matched+=run.count;frozen+=run.count;multiplies+=run.count+groups;
   });}
  else{std::optional<MapHash> digest;if(!c.static_locator || c.map_id.rfind("native-base/",0)==0)digest.emplace();const bool baseMapped=usesBaseCoordinates(c);for(std::size_t j=0;j<c.padded;++j){auto l=c.map(j);auth_metrics_.rowmajor_coordinate_lookups+=baseMapped && !l.constant;if(digest)digest->append(l);++visited;++fallback;if(!l.constant && l.coordinate.root==k){need(l.coordinate.index<roots_[k].spec.actual,"classified mapped coordinate outside actual root");result[l.coordinate.index]+=lambda[i]*w.at(j);++matched;multiplies+=2;}}if(digest)need(digest->finish()==c.map_digest,"classified source map changed after defer");}
 }
 auth_metrics_.cpp_allocations+=allocation.count();auth_metrics_.cpp_allocated_bytes+=allocation.bytes();
 auto cpu=processCPU()-started;auto seconds=std::chrono::duration<double>(Clock::now()-wall).count();
 auth_metrics_.kernel_coordinates+=visited;auth_metrics_.kernel_matched+=matched;auth_metrics_.frozen_coordinates+=frozen;auth_metrics_.fallback_map_queries+=fallback;auth_metrics_.fr_multiplications+=multiplies;auth_metrics_.fr_additions+=matched;auth_metrics_.weight_products+=matched;auth_metrics_.kernel_cpu+=cpu;auth_metrics_.kernel_wall+=seconds;
 if(opt2MetricsEnabled())std::cerr<<"OPT2_KERNEL {\"root\":"<<k<<",\"coordinates\":"<<visited<<",\"matched_coordinates\":"<<matched<<",\"fallback_map_queries\":"<<fallback<<",\"Fr_multiplications\":"<<multiplies<<",\"Fr_additions\":"<<matched<<",\"wall_seconds\":"<<seconds<<",\"cpu_seconds\":"<<cpu<<"}\n";
 return result;
}
Fr Commitments::kernelEndpoint(std::size_t k,const std::vector<Fr>&rho,const Authentication&auth,const std::vector<Fr>&point)const{
 const auto n=roots_.at(k).spec.padded;need(point.size()==r1::log2exact(n) && rho.size()==claims_.size() && auth.claims.size()==claims_.size(),"K terminal shape");
 const bool timing=opt2MetricsEnabled();auto start=timing?Clock::now():Clock::time_point{};auto&m=auth_metrics_;++m.endpoint_calls;
 auto elapsed=[&](Clock::time_point t){return timing?std::chrono::duration<double>(Clock::now()-t).count():0.;};AllocationTrace allocations;
 auto finish=[&]{m.endpoint_allocations+=allocations.count();m.endpoint_allocated_bytes+=allocations.bytes();};
 const auto*reference=std::getenv("ZKGPT_PERF2_K_REFERENCE");
 // PERF2 did not meet its predeclared performance gate. Explicit opt-in only.
 if(!reference || std::string(reference)!="0"){
  auto matched=m.kernel_matched;auto v=kernel(k,rho,auth);m.endpoint_dense_coordinates+=m.kernel_matched-matched;m.endpoint_dense_bytes+=n*sizeof(Fr);m.endpoint_build_seconds+=elapsed(start);auto begin=timing?Clock::now():Clock::time_point{};
  auto out=fold(v,point);m.endpoint_fold_coordinates+=2*n-2;m.endpoint_fold_seconds+=elapsed(begin);m.endpoint_seconds+=elapsed(start);finish();return out;
 }
 std::vector<Fr>dense;std::size_t active=0;Fr total=0;std::uint64_t fast=0,blocks=0,visited=0;
 for(std::size_t i=0;i<claims_.size();++i){const auto&c=claims_[i];const auto&p=auth.claims[i];if(c.only_root>=0 && std::size_t(c.only_root)!=k)continue;if(p.frozen && !p.hasRoot(k))continue;
  std::unique_ptr<Weights>scratch;if(!p.weights)scratch=std::make_unique<Weights>(c.point,&m);const auto&w=p.weights?*p.weights:*scratch;std::optional<TerminalBasis>basis;
  auto add=[&](std::size_t begin,std::size_t count,std::int64_t at,std::int64_t stride,std::size_t step){
   if(dense.empty()){dense.assign(n,Fr(0));m.endpoint_dense_bytes+=n*sizeof(Fr);}
   auto last=at+std::int64_t(count-1)*stride;active=std::max(active,std::size_t(std::max(at,last))+1);
   w.scaled(begin,count,rho[i],[&](const Fr&weight){dense[std::size_t(at)]+=weight;at+=stride;},step);visited+=count;
  };
  if(p.frozen)p.forRoot(k,auth.store,[&](const mapping::Run&run){
   need(run.count && run.logical_step && run.begin<c.padded && run.count<=1+(c.padded-1-run.begin)/run.logical_step,"K terminal verified logical run bounds");
   const auto actual=roots_[k].spec.actual;need(run.stride>=-std::int64_t(actual) && run.stride<=std::int64_t(actual),"K terminal verified stride bound");
   const auto last=std::int64_t(run.first.index)+std::int64_t(run.count-1)*run.stride;need(run.first.root==k && run.first.index<actual && last>=0 && std::uint64_t(last)<actual,"K terminal verified source run bounds");
   if(run.logical_step!=1 || (run.stride!=0 && run.stride!=1) || run.count<64 || (run.stride==1 && (run.begin%64!=run.first.index%64))){add(run.begin,run.count,run.first.index,run.stride,run.logical_step);return;}
   if(!basis)basis.emplace(c.point,point);
   auto j=run.begin,z=std::size_t(run.first.index),remaining=run.count;
   while(remaining){std::size_t length=1;unsigned bits=0;while(length<=remaining/2 && !(j&length) && (!run.stride || !(z&length))){length*=2;++bits;}
    if(length>=64){total+=rho[i]*basis->block(j,z,bits,run.stride==0);fast+=length;++blocks;}
    else add(j,length,z,run.stride,1);
    j+=length;if(run.stride)z+=length;remaining-=length;
   }
  });
  else{
   // Keep canonical callback revalidation for the existing budget fallback.
   std::optional<MapHash> digest;if(!c.static_locator || c.map_id.rfind("native-base/",0)==0)digest.emplace();for(std::size_t j=0;j<c.padded;++j){auto l=c.map(j);if(digest)digest->append(l);if(!l.constant && l.coordinate.root==k){need(l.coordinate.index<roots_[k].spec.actual,"K terminal fallback source bounds");add(j,1,l.coordinate.index,0,1);}}
   if(digest)need(digest->finish()==c.map_digest,"classified source map changed after defer");
  }
 }
 m.endpoint_build_seconds+=elapsed(start);auto begin=timing?Clock::now():Clock::time_point{};std::size_t width=1;while(width<active)width*=2;
 if(active){unsigned t=0;for(auto count=width;count>1;count/=2,++t){m.endpoint_fold_coordinates+=count;for(std::size_t j=0;j<count/2;++j)dense[j]=dense[2*j]+point[t]*(dense[2*j+1]-dense[2*j]);}
  auto residual=dense[0];for(;t<point.size();++t)residual*=Fr(1)-point[t];total+=residual;
 }
 m.endpoint_fast_coordinates+=fast;m.endpoint_fast_blocks+=blocks;m.endpoint_dense_coordinates+=visited;m.endpoint_fold_seconds+=elapsed(begin);m.endpoint_seconds+=elapsed(start);
 if(timing)std::cerr<<"PERF2_K {\"root\":"<<k<<",\"fast_coordinates\":"<<fast<<",\"fast_blocks\":"<<blocks<<",\"dense_coordinates\":"<<visited<<",\"dense_initialized_bytes\":"<<(dense.empty()?0:n*sizeof(Fr))<<",\"dense_fold_extent\":"<<(active?width:0)<<",\"copy_bytes\":0}\n";
 finish();return total;
}
Commitments::Commitments(IO&io,const Layout&layout,Value base,Prepared*prepared,Random*rng,const model::Trust*trust,model::Secret*modelSecret,const std::vector<model::Trust>*chunks,std::vector<model::Secret>*chunkSecrets,const std::vector<Fr>*single_x,const std::vector<Fr>*category_master):io_(io),layout_(layout),identity_(identity(layout)),session_(io.session()),rng_(rng){
 if(layout.master_actual){auto text=std::getenv("ZKGPT_PERF4_SOURCE_REFERENCE");need(!text || std::string(text)=="0" || std::string(text)=="1","PERF4 static source mode");static_source_enabled_=!text || std::string(text)!="1";}
 need(!category_master || (single_x && layout.category_mode && io.prover()),"category native storage prover boundary");
 if(single_x){need(!trust && !modelSecret && !chunks && !chunkSecrets,"single prover-selected model cannot import independent root authority");initializeSingle(std::move(base),prepared,*single_x,category_master);return;}
 auto modelCount=std::count_if(layout.roots.begin(),layout.roots.end(),[](auto&r){return r.model;});need(layout.master_actual?(chunks && chunks->size()==std::size_t(modelCount) && !trust && !modelSecret && (io.prover()?(chunkSecrets && chunkSecrets->size()==chunks->size()):!chunkSecrets)):(!chunks && !chunkSecrets),"classified Full12 independently loaded chunk trust boundary");
 need(io.prover()?bool(base)&&prepared&&rng:!base&&!prepared&&!rng&&!modelSecret,"classified P/V boundary");Writer binding;binding.text(suite);binding.text(identity_);binding.text(vectorSuite);binding.text(vectorLayout);binding.text(session_);bindPublic(io,"Classified/layout",binding.b);roots_.resize(layout.roots.size());std::set<std::string>usedPrepared;
 for(std::size_t k=0;k<roots_.size();++k){auto rootStarted=Clock::now();auto&root=roots_[k];root.spec=layout.roots[k];if(root.spec.model)continue;std::shared_ptr<std::vector<Fr>>packed;
  if(io.prover()){
   packed=std::make_shared<std::vector<Fr>>(root.spec.padded,Fr(0));
   if(root.spec.registered){if(layout.master_actual){for(auto&run:layout.base_runs)if(run.start.root==k)for(std::size_t j=0;j<run.count;++j)packed->at(run.start.index+j)=base(run.begin+j);}else for(std::size_t i=0;i<layout.base.size();++i)if(layout.base[i].root==k)packed->at(layout.base[i].index)=base(i);}
   else if(layout.master_actual){for(auto&[id,view]:layout.views)if(!view.mapped)for(auto&piece:view.pieces)if(piece.start.root==k){auto it=prepared->find(id);need(it!=prepared->end() && it->second->size()==view.padded && piece.begin<=view.padded && piece.count<=view.padded-piece.begin && piece.start.index<=packed->size() && piece.count<=packed->size()-piece.start.index,"classified prepared partition shape/bounds");std::copy_n(it->second->begin()+piece.begin,piece.count,packed->begin()+piece.start.index);usedPrepared.insert(id);}}
   else for(auto&[id,view]:layout.views)if(!view.mapped && view.start.root==k){auto it=prepared->find(id);need(it!=prepared->end() && it->second->size()==view.padded,"classified prepared object/shape");std::copy(it->second->begin(),it->second->end(),packed->begin()+view.start.index);prepared->erase(it);}
   root.secret=commitVectorOwned(root.spec.id,root.spec.actual,root.spec.padded,std::move(packed),*rng);root.pc=root.secret->pc;
  }
  auto bytes=io.message("Classified/root/"+std::to_string(k)+"/PC",io.prover()?encodeVector(root.pc):Bytes{});root.pc=decodeVector(bytes);need(root.pc.id==root.spec.id && root.pc.actual==root.spec.actual && root.pc.padded==root.spec.padded && root.pc.layout==r1::Layout::RowMajor,"classified wrong commitment identity/shape");root.basis=hyrax_a3::generators(root.pc.columns);
  if(io.prover()){auto values=root.secret->values;root.value=[values](std::size_t i){return values->at(i);};root.blinds=root.secret->blinds;root.sealed_data=values->data();root.sealed_stored=values->size();root.sealed_storage_valid=[values,data=values->data(),size=values->size()]{return values->data()==data && values->size()==size;};}
  rootTiming("pack-and-commit",root.spec.id,rootStarted);
 }
 if(io.prover() && layout.master_actual){need(usedPrepared.size()==prepared->size(),"classified unused/missing partitioned helper");prepared->clear();}
 std::size_t chunkIndex=0;
 for(std::size_t k=0;k<roots_.size();++k)if(roots_[k].spec.model){
  const model::Trust*activeTrust=layout.master_actual?&chunks->at(chunkIndex):trust;model::Secret*activeSecret=layout.master_actual?(io.prover()?&chunkSecrets->at(chunkIndex):nullptr):modelSecret;need(activeTrust,"classified missing model authority");
  std::string scope=layout.master_actual?"Full12/model/chunk/"+std::to_string(chunkIndex):"";std::optional<stage2::ScopedIO>scoped;if(!scope.empty())scoped.emplace(io,scope);IO&activeIO=scoped?static_cast<IO&>(*scoped):io;
  auto pub=model::registerModel(activeIO,*activeTrust,activeSecret,rng);auto&root=roots_[k];need(root.spec.actual==pub.spec.actual && root.spec.padded==pub.spec.padded,"classified W trusted dimensions");root.pc={root.spec.id,pub.spec.actual,pub.spec.padded,pub.spec.rows,pub.spec.columns,pub.rows,r1::Layout::RowMajor};root.basis=r1::generators(activeTrust->anchor.w.spec).g;
  if(io.prover()){root.value=[activeSecret](std::size_t i){return activeSecret->value(i);};root.blinds=activeSecret->blinds;root.sealed_data=activeSecret->values.data();root.sealed_stored=activeSecret->values.size();root.sealed_storage_valid=[activeSecret,data=activeSecret->values.data(),size=activeSecret->values.size()]{return activeSecret->values.data()==data && activeSecret->values.size()==size;};}
  root.open=[activeTrust,pub,activeSecret,scope](IO&live,const Query&q,const Hidden*h,Random*r,const std::string&id){if(scope.empty())model::opening(live,*activeTrust,pub,q,activeSecret,h,r,id);else{stage2::ScopedIO scoped(live,scope);model::opening(scoped,*activeTrust,pub,q,activeSecret,h,r,id);}};
  ++chunkIndex;
 }
 if(io.prover())need(prepared->empty(),"classified unused prepared object");
 sealed_=true;
}
void Commitments::constrainBaseTails(){
 if(single_pc_){need(sealed_ && !tails_constrained_ && !authenticated_ && claims_.empty() && io_.session()==session_ && identity(layout_)==identity_,"single compact lifecycle");tails_constrained_=true;return;}
 need(sealed_ && !tails_constrained_ && !authenticated_ && claims_.empty() && io_.session()==session_ && identity(layout_)==identity_,"classified tail constraint lifecycle");for(auto&r:roots_)need(r.pc.layout==r1::Layout::RowMajor,"classified root must be RowMajor");std::set<std::string>seen;
 for(std::size_t k=0;k<roots_.size();++k){auto&r=roots_[k];if(!r.spec.registered || r.spec.model)continue;auto chunk=std::min<std::size_t>(64,65536/r.pc.columns);
  // Full valid rows need no registration. Model tails are fixed by the trusted
  // R6 anchor and registerModel() rerandomization binding in the constructor.
  for(std::size_t start=r.spec.actual/r.pc.columns;start<r.pc.rows;start+=chunk){auto count=std::min(chunk,r.pc.rows-start);auto name="Classified/zero-tail/"+std::to_string(k)+"/"+std::to_string(start);Writer header;header.text(suite);header.text(identity_);header.text(session_);header.text(r.spec.id);header.number(start);header.number(count);header.number(r.pc.columns);for(std::size_t i=0;i<count;++i)header.point(r.pc.points[start+i]);bindPublic(io_,name+"/header",header.b);std::vector<std::vector<Fr>>masks;std::vector<Fr>blinds;Writer first;
   auto active=[&](std::size_t row){const auto begin=row*r.pc.columns;return begin>=r.spec.actual?std::size_t(0):std::min(r.pc.columns,r.spec.actual-begin);};
   if(io_.prover())for(std::size_t i=0;i<count;++i){std::vector<Fr>d(active(start+i));for(auto&x:d)x=rng_->scalar();auto rho=rng_->scalar();G1 mask;mask.clear();if(!d.empty())mask=master3::masterMSM(r.basis.data(),d.data(),d.size());first.point(mask+parameters(1).hiding*rho);masks.push_back(std::move(d));blinds.push_back(rho);}
   auto b=io_.message(name+"/masks",first.b);Reader mr{b};std::vector<G1>D;for(std::size_t i=0;i<count;++i){auto d=mr.point();need(seen.insert(hex(r1::hash(r1::encode(d)))).second,"classified reused tail mask");D.push_back(d);}mr.end();auto c=io_.coin(name+"/challenge");need(!c.isZero(),"classified zero tail challenge");Writer response;
   if(io_.prover())for(std::size_t i=0;i<count;++i){for(std::size_t j=0;j<masks[i].size();++j)response.scalar(masks[i][j]+c*r.value((start+i)*r.pc.columns+j));response.scalar(blinds[i]+c*r.blinds[start+i]);}
   b=io_.message(name+"/responses",response.b);tail_response_bytes+=b.size();Reader rr{b};for(std::size_t i=0;i<count;++i){auto n=active(start+i);std::vector<Fr>z(n);for(auto&x:z)x=rr.scalar();auto rho=rr.scalar();if(!io_.prover()){G1 nonblind;nonblind.clear();if(n)nonblind=master3::masterMSM(r.basis.data(),z.data(),n);need(nonblind+parameters(1).hiding*rho==D[i]+r.pc.points[start+i]*c,"classified zero-tail equation failed");}++tail_rows;}rr.end();
  }
 }
 Writer end;end.text(identity_);end.number(tail_rows,8);end.number(tail_response_bytes,8);bindPublic(io_,"Classified/base-tails-constrained",end.b);tails_constrained_=true;
}
void Commitments::rootOpening(std::size_t k,const Query&q,const Hidden*h,const std::string&id){auto started=Clock::now();auto&r=roots_.at(k);if(r.spec.model)r.open(io_,q,h,rng_,id);else openVector(io_,r.pc,q,io_.prover()?&*r.secret:nullptr,h,rng_,id);rootTiming("opening",r.spec.id,started);}
std::shared_ptr<const StaticBaseMap>Commitments::staticBase(){
 need(static_source_enabled_,"PERF4 static base disabled");if(!static_base_){static_base_=std::make_shared<const StaticBaseMap>(layout_);++auth_metrics_.static_base_builds;auth_metrics_.static_locator_bytes+=static_base_->bytes();}return static_base_;
}
VectorOracle Commitments::view(const std::string&id,bool source_mle){
 need(sealed_ && !authenticated_,"classified view lifecycle");const auto&v=layout_.views.at(id);VectorOracle out;out.id=id;out.actual=v.actual;out.padded=v.padded;
 if(io_.prover() && !v.mapped && v.pieces.size()==1 && v.pieces[0].begin==0 && v.pieces[0].count==v.padded){
  const auto start=v.pieces[0].start;
  const auto&root=roots_.at(start.root);
  if(root.sealed_span && (single_public_roots_.empty() || !single_public_roots_.at(start.root))){
   const auto span=root.sealed_span(start.index,v.padded);
   if(span.count==v.padded)out.contiguous=span.data;
  }
 }
 if(static_source_enabled_ && (v.mapped || !v.pieces.empty())){
  auto&plan=static_sources_[id];if(!plan){plan=std::make_shared<const StaticSourceMap>(layout_,v,*staticBase());++auth_metrics_.static_source_builds;auth_metrics_.static_locator_bytes+=plan->bytes();}else ++auth_metrics_.static_source_hits;
  if(io_.prover() && source_mle && !out.contiguous){
   for(const auto&run:plan->runs()){
    const auto&l=run.first;
    const auto end=run.begin+run.count;
    if(l.constant){out.source_spans.push_back({end,nullptr,l.bias});continue;}
    const auto&root=roots_.at(l.coordinate.root);
    if(!root.sealed_span || (!single_public_roots_.empty() && single_public_roots_.at(l.coordinate.root))){
     out.source_spans.clear();break;
    }
    std::size_t consumed=0;
    while(consumed<run.count){
     const auto span=root.sealed_span(l.coordinate.index+consumed,run.count-consumed);
     if(!span.data || !span.count)break;
     consumed+=span.count;
     out.source_spans.push_back({run.begin+consumed,span.data,l.bias});
    }
    if(consumed!=run.count){out.source_spans.clear();break;}
   }
  }
  if(io_.prover())out.value=[this,plan](std::size_t j){auto l=publicLocation(plan->at(j));return l.constant?l.bias:roots_.at(l.coordinate.root).value(l.coordinate.index)+l.bias;};
  const auto map_id=v.id;const auto padded=v.padded;const int hint=!v.mapped && v.pieces.size()<=1?int(v.start.root):-1;
  out.open=[this,plan,map_id,padded,hint,descriptor=&v](IO&live,const Query&q,const Hidden*h,Random*,std::string name){need(&live==&io_,"classified foreign IO session");Hidden value;if(live.prover()){need(h,"classified opening missing private evaluation");value=*h;}else value.handle=q.evaluation;Claim c{name,map_id,value,q.point,padded,[plan](std::size_t j){return plan->at(j);}};c.only_root=hint;c.static_locator=true;deferTrusted(std::move(c),descriptor,plan.get());};return out;
 }
 if(io_.prover())out.value=[this,&v](std::size_t j){auto l=publicLocation(locate(layout_,v,j));return l.constant?l.bias:roots_.at(l.coordinate.root).value(l.coordinate.index)+l.bias;};
 out.open=[this,&v](IO&live,const Query&q,const Hidden*h,Random*,std::string name){need(&live==&io_,"classified foreign IO session");Hidden value;if(live.prover()){need(h,"classified opening missing private evaluation");value=*h;}else value.handle=q.evaluation;Claim c{name,v.id,value,q.point,v.padded,[this,&v](std::size_t j){return locate(layout_,v,j);}};if(!v.mapped && v.pieces.size()<=1)c.only_root=int(v.start.root);deferTrusted(std::move(c),&v);};return out;
}
void Commitments::defer(Claim c){
 need(!layout_.category_mode,"category untrusted raw Claim source: use verifier-owned view or deferBase rule");deferTrusted(std::move(c));
}
void Commitments::deferTrusted(Claim c,const void*descriptor,const StaticSourceMap*source,const std::vector<std::uint32_t>*indices){
 if(layout_.category_mode){
  need(!category_failed_ && sealed_ && tails_constrained_ && !authenticated_ && io_.session()==session_,"category failed/closed claim lifecycle");
  try{
   need(c.map && c.padded && c.padded<=r1::kMaxVector && c.point.size()==r1::log2exact(c.padded) && names_.count(c.id)==0,"classified missing/duplicate/invalid claim");
   need(c.value.handle.session==session_,"R7 HiddenScalar session/role identity mismatch");
   if(!category_auth_){category_auth_=std::make_unique<Authentication>(0,layout_.master_actual!=0);category_auth_->session=session_;category_auth_->transcript=io_.transcriptOwner();}
   auto&auth=*category_auth_;need(auth.claims.size()==claims_.size(),"category prepared registration size");auth.claims.emplace_back();
   auto original=std::move(c.map);c.map=[this,original=std::move(original)](std::size_t j){return publicLocation(original(j));};
   prepareCategoryClaim(c,auth,claims_.size(),descriptor,source,indices);
   Writer w;w.text(suite);w.text(identity_);w.text(c.id);w.text(c.map_id);w.raw(Bytes(c.map_digest.begin(),c.map_digest.end()));w.raw(encode(c.value.handle));w.number(c.padded,8);for(auto&x:c.point)w.scalar(x);
   bindPublic(io_,"Classified/defer/"+std::to_string(claims_.size()),w.b);names_.insert(c.id);claims_.push_back(std::move(c));
  }catch(...){category_failed_=true;category_auth_.reset();throw;}return;
 }
 if(single_pc_){auto original=std::move(c.map);need(bool(original),"single missing source map");c.map=[this,original=std::move(original)](std::size_t j){return publicLocation(original(j));};
  if(c.only_root>=0)for(const auto&entry:single_public_coordinates_)if((entry.first>>32)==std::size_t(c.only_root)){c.only_root=-1;break;}}
 need(sealed_ && tails_constrained_ && !authenticated_ && io_.session()==session_,"classified unconstrained tails/closed session");need(c.map && c.padded && c.padded<=r1::kMaxVector && c.point.size()==r1::log2exact(c.padded) && names_.insert(c.id).second,"classified missing/duplicate/invalid claim");need(c.value.handle.session==session_,"R7 HiddenScalar session/role identity mismatch");c.map_digest=mapDigest(c);Writer w;w.text(suite);w.text(identity_);w.text(c.id);w.text(c.map_id);w.raw(Bytes(c.map_digest.begin(),c.map_digest.end()));w.raw(encode(c.value.handle));w.number(c.padded,8);for(auto&x:c.point)w.scalar(x);bindPublic(io_,"Classified/defer/"+std::to_string(claims_.size()),w.b);claims_.push_back(std::move(c));
}
void Commitments::deferBase(const std::string&id,std::size_t actual,std::size_t padded,std::function<std::size_t(std::size_t)>index,const Hidden&h,const std::vector<Fr>&q){
 deferBaseMapped(id,actual,padded,std::move(index),h,q,nullptr);
}
void Commitments::deferNativeTerminal(const std::string&id,const std::vector<std::uint32_t>&indices,std::size_t padded,const Hidden&h,const std::vector<Fr>&q){
 if(layout_.category_mode && static_source_enabled_){
  need(!indices.empty() && indices.size()<=padded,"classified base claim shape");
  auto base=staticBase();
  Claim c{id,"native-base/"+id,h,q,padded,[base,&indices](std::size_t j)->Located{
   return j>=indices.size()?Located{{},Fr(0),true}:Located{base->at(indices[j]),Fr(0),false};
  }};
  c.static_locator=true;
  deferTrusted(std::move(c),&indices,nullptr,&indices);
  return;
 }
 deferBaseMapped(id,indices.size(),padded,[&indices](std::size_t j){return indices.at(j);},h,q,&indices);
}
void Commitments::deferBaseMapped(const std::string&id,std::size_t actual,std::size_t padded,std::function<std::size_t(std::size_t)>index,const Hidden&h,const std::vector<Fr>&q,const void*descriptor){need(actual>0 && actual<=padded,"classified base claim shape");
 if(static_source_enabled_){auto base=staticBase();Claim c{id,"native-base/"+id,h,q,padded,[base,index=std::move(index),actual](std::size_t j)->Located{if(j>=actual)return {{},Fr(0),true};return {base->at(index(j)),Fr(0),false};}};c.static_locator=true;deferTrusted(std::move(c),descriptor);return;}
 deferTrusted({id,"native-base/"+id,h,q,padded,[this,index=std::move(index),actual](std::size_t j)->Located{if(j>=actual)return {{},Fr(0),true};auto at=index(j);need(at<baseSize(layout_),"classified base map out of bounds");return {baseCoordinate(layout_,at),Fr(0),false};}},descriptor);}
Digest Commitments::mapDigest(const Claim&c)const{auto started=processCPU();++auth_metrics_.map_calls;auth_metrics_.map_coordinates+=c.padded;MapHash h;const bool baseMapped=usesBaseCoordinates(c);for(std::size_t j=0;j<c.padded;++j){auto l=c.map(j);auth_metrics_.rowmajor_coordinate_lookups+=baseMapped && !l.constant;need(l.constant || (l.coordinate.root<roots_.size() && l.coordinate.index<roots_[l.coordinate.root].spec.actual),"classified map root/coordinate bounds");if(c.only_root>=0)need(!l.constant && l.bias.isZero() && l.coordinate.root==std::size_t(c.only_root),"classified invalid root-only map hint");h.append(l);}auto d=h.finish();auth_metrics_.map_cpu+=processCPU()-started;return d;}
std::vector<Fr>Commitments::kernel(std::size_t k,const std::vector<Fr>&lambda)const{
 auto started=processCPU();++auth_metrics_.kernel_calls;auth_metrics_.kernel_peak_bytes=std::max<std::uint64_t>(auth_metrics_.kernel_peak_bytes,roots_.at(k).spec.padded*sizeof(Fr));std::vector<Fr>result(roots_.at(k).spec.padded,Fr(0));for(std::size_t i=0;i<claims_.size();++i){auto&c=claims_[i];if(c.only_root>=0 && std::size_t(c.only_root)!=k)continue;auth_metrics_.kernel_coordinates+=c.padded;auth_metrics_.weight_products+=c.padded;Weights w(c.point,&auth_metrics_);std::optional<MapHash> digest;if(!c.static_locator || c.map_id.rfind("native-base/",0)==0)digest.emplace();for(std::size_t j=0;j<c.padded;++j){auto l=c.map(j);if(digest)digest->append(l);if(!l.constant && l.coordinate.root==k){need(l.coordinate.index<roots_[k].spec.actual,"classified mapped coordinate outside actual root");result[l.coordinate.index]+=lambda[i]*w.at(j);}}if(digest)need(digest->finish()==c.map_digest,"classified source map changed after defer");}auth_metrics_.kernel_cpu+=processCPU()-started;if(opt2MetricsEnabled())std::cerr<<"OPT2_KERNEL {\"root\":"<<k<<",\"cpu_seconds\":"<<processCPU()-started<<"}\n";
 return result;
}
Commitments::~Commitments(){
 const auto&m=auth_metrics_;
 if(layout_.category_mode)std::cerr<<"MAPHASH_V3 {\"direct_runs\":"<<m.direct_map_runs
  <<",\"direct_coordinates\":"<<m.direct_map_coordinates<<",\"updates\":"<<m.maphash_updates
  <<",\"bytes\":"<<m.maphash_bytes<<",\"hash_update_seconds\":"<<m.maphash_update_seconds<<"}\n";
 if(layout_.category_mode){
  const char*names[]={"GKR terminals","Source Original Handles","Range/Exp/Lasso/Memory/Product","public and other"};
  for(unsigned i=0;i<m.mapping_phases.size();++i){
   const auto&phase=m.mapping_phases[i];
   std::cerr<<std::setprecision(12)<<"MAPPING_TEMPLATE_PHASE {\"phase\":"<<std::quoted(names[i])<<",\"claims\":"<<phase.claims
    <<",\"hits\":"<<phase.hits<<",\"scanned_coordinates\":"<<phase.scanned<<",\"avoided_callbacks\":"<<phase.avoided
    <<",\"prepare_wall_seconds\":"<<phase.wall<<"}\n";
  }
  const auto avoided=std::accumulate(m.mapping_phases.begin(),m.mapping_phases.end(),std::uint64_t(0),[](auto sum,const auto&phase){return sum+phase.avoided;});
  std::cerr<<"MAPPING_TEMPLATE_TOTAL {\"claims\":"<<m.mapping_claims<<",\"unique_templates\":"<<m.map_calls
   <<",\"reused_claims\":"<<m.template_hits<<",\"hit_rate\":"<<(m.mapping_claims?double(m.template_hits)/m.mapping_claims:0)
   <<",\"scanned_coordinates\":"<<m.map_coordinates<<",\"avoided_callbacks\":"<<avoided<<",\"avoided_maphash_bytes\":"<<41*avoided
   <<",\"frozen_runs\":"<<m.template_runs<<",\"frozen_run_ram_bytes\":"<<(category_auth_?category_auth_->map_bytes:0)
   <<",\"frozen_file_storage_bytes\":"<<(category_auth_?category_auth_->store.bytes:0)
   <<",\"cache_payload_ram_bytes\":"<<m.template_cache_bytes<<",\"bias_descriptor_bytes\":"<<m.bias_descriptor_bytes
   <<",\"prepare_wall_seconds\":"<<m.map_wall<<"}\n";
 }
 if(opt2MetricsEnabled()){
 if(layout_.category_mode)std::cerr<<"CATEGORY_MAPPING_TOTAL {\"complete_mapping_scans\":"<<m.map_calls<<",\"mapping_callback_calls\":"<<(m.map_coordinates-m.direct_map_coordinates)<<",\"frozen_RAM_claims\":"<<m.frozen_ram_claims<<",\"frozen_FILE_claims\":"<<m.frozen_file_claims<<",\"mapping_RAM_peak_bytes\":"<<m.mapping_ram_peak<<",\"mapping_file_peak_bytes\":"<<m.mapping_file_peak<<",\"prepare_wall_seconds\":"<<m.map_wall<<",\"K_wall_seconds\":"<<m.kernel_wall<<",\"endpoint_wall_seconds\":"<<m.endpoint_seconds<<",\"fallback_map_queries\":0}\n";
 std::cerr<<"PERF4_STATIC_SOURCE {\"base_builds\":"<<m.static_base_builds<<",\"view_builds\":"<<m.static_source_builds<<",\"view_hits\":"<<m.static_source_hits<<",\"owned_plan_bytes\":"<<m.static_locator_bytes<<",\"claim_digest_and_builder_reused\":"<<(layout_.category_mode?"true":"false")<<"}\n";
 std::cerr<<"PERF3_MAP {\"prepare_bias_serializations\":"<<m.map_bias_serializations<<",\"prepare_bias_encoding_reuses\":"<<m.map_bias_reuses<<"}\n";
 std::cerr<<"PERF2_K_TOTAL {\"endpoint_calls\":"<<m.endpoint_calls<<",\"fast_coordinates\":"<<m.endpoint_fast_coordinates<<",\"fast_blocks\":"<<m.endpoint_fast_blocks<<",\"dense_coordinates\":"<<m.endpoint_dense_coordinates<<",\"dense_initialized_bytes\":"<<m.endpoint_dense_bytes<<",\"fold_coordinates\":"<<m.endpoint_fold_coordinates<<",\"build_seconds\":"<<m.endpoint_build_seconds<<",\"fold_seconds\":"<<m.endpoint_fold_seconds<<",\"endpoint_seconds\":"<<m.endpoint_seconds<<",\"allocations\":"<<m.endpoint_allocations<<",\"allocated_bytes\":"<<m.endpoint_allocated_bytes<<",\"copy_bytes\":0}\n";
 std::cerr<<"OPT3A_AUTH {\"builtin_RowMajor_base_coordinate_lookups\":"<<m.rowmajor_coordinate_lookups<<",\"kernel_matched_coordinates\":"<<m.kernel_matched<<",\"partial_visited_coordinates\":"<<m.partial_coordinates<<",\"frozen_kernel_coordinates\":"<<m.frozen_coordinates<<",\"fallback_map_queries\":"<<m.fallback_map_queries<<",\"kernel_and_partial_Fr_multiplications\":"<<m.fr_multiplications<<",\"kernel_and_partial_Fr_additions\":"<<m.fr_additions<<",\"kernel_wall_seconds\":"<<m.kernel_wall<<",\"partial_wall_seconds\":"<<m.partial_wall<<",\"prepare_wall_seconds\":"<<m.map_wall<<",\"kernel_and_partial_cpp_allocations\":"<<m.cpp_allocations<<",\"kernel_and_partial_cpp_allocated_bytes\":"<<m.cpp_allocated_bytes<<"}\n";std::cerr<<"OPT2_AUTH {\"map_calls\":"<<m.map_calls<<",\"map_coordinates\":"<<m.map_coordinates<<",\"map_cpu_seconds\":"<<m.map_cpu<<",\"kernel_calls\":"<<m.kernel_calls<<",\"kernel_coordinates\":"<<m.kernel_coordinates<<",\"kernel_cpu_seconds\":"<<m.kernel_cpu<<",\"weights_precomputations\":"<<m.weights<<",\"weights_terms\":"<<m.weight_terms<<",\"weights_cpu_seconds\":"<<m.weights_cpu<<",\"weight_products\":"<<m.weight_products<<",\"partial_cpu_seconds\":"<<m.partial_cpu<<",\"kernel_peak_bytes\":"<<m.kernel_peak_bytes<<"}\n";}}
void Commitments::authenticateUnifiedInput(const std::vector<Fr>&public_x){
 namespace ui=unified;
 if(layout_.category_mode){authenticateCategoryInput(public_x);return;}
 need(sealed_ && tails_constrained_ && !authenticated_ && !claims_.empty() && claims_.size()<=8192 && io_.session()==session_ && identity(layout_)==identity_,"unified missing/closed/mutated authentication");
 need(!public_x.empty() && public_x.size()<=(1u<<24),"unified public X bound");
 if(single_pc_)need(public_x==single_x_,"single public X changed after commitment seal");
 auto blocks=unifiedBlocks(layout_);
 auto packing=ui::pack(blocks,power2(public_x.size()));
 const auto packing_descriptor=ui::descriptor(packing);
 Writer layout;layout.raw(packing_descriptor);layout.text(legacyUnifiedInputIdentity(layout_,public_x,packing,packing_descriptor));layout.text(identity_);layout.text(session_);layout.number(public_x.size(),8);for(auto&x:public_x){need(x.isValid(),"unified public X scalar");layout.scalar(x);}layout.number(roots_.size());
 if(single_pc_){layout.text(single::suite);layout.raw(single::descriptor(single_pc_->plan()));}
 else for(std::size_t k=0;k<roots_.size();++k){auto&r=roots_[k];auto s=packing.segments()[k+1];layout.text(r.spec.id);layout.number(blocks[k].category);layout.number(s.offset,8);layout.number(r.spec.actual,8);layout.number(s.padded,8);Writer rows;rows.number(r.pc.rows,8);rows.number(r.pc.columns,8);for(auto&p:r.pc.points)rows.point(p);auto digest=r1::hash(rows.b);layout.raw(Bytes(digest.begin(),digest.end()));}
 bindPublic(io_,"UnifiedInput/layout",layout.b);
 Writer declarations;declarations.text(ui::suite);declarations.number(claims_.size());for(auto&c:claims_){declarations.text(c.id);declarations.raw(Bytes(c.map_digest.begin(),c.map_digest.end()));}bindPublic(io_,"UnifiedInput/all-claims-fixed",declarations.b);
 Authentication auth(claims_.size(),layout_.master_actual!=0);std::vector<Fr>biases;for(std::size_t i=0;i<claims_.size();++i)biases.push_back(prepareClaim(claims_[i],auth,i));
 // Every mapping/bias is fully revalidated BEFORE any aggregation draw.
 std::vector<Fr>rho;std::vector<Hidden>values;Fr bias=0;
 for(std::size_t i=0;i<claims_.size();++i){rho.push_back(io_.coin("UnifiedInput/rho/"+std::to_string(i)));need(!rho.back().isZero(),"unified zero aggregation challenge");values.push_back(claims_[i].value);bias+=rho.back()*biases[i];}
 auto initial=combine(session_,"UnifiedInput/initial",values,rho);initial.value-=bias;initial.handle.point-=scalarCommit(bias,Fr(0));bindPublic(io_,"UnifiedInput/initial",encode(initial.handle));
 Value x=[&](std::size_t j){return j<public_x.size()?public_x[j]:Fr(0);};
 std::unique_ptr<ui::PublicK>public_k;
 const auto*reference=std::getenv("ZKGPT_K_REFERENCE");
 if(io_.prover() && !(reference && std::string(reference)=="1"))public_k=std::make_unique<ui::PublicK>(packing.segments(),[&](std::size_t k){return k?kernel(k-1,rho,auth):std::vector<Fr>(packing.segments()[0].padded,Fr(0));},ui::KBudget::environment(),session_,packing.domain());
 ui::Factors factors;if(io_.prover())factors=[&](std::size_t k,const ui::Consume&consume){
  if(public_k){public_k->withRoot(k,session_,[&](const Value&b){consume(k?roots_[k-1].value:x,b);});return;}
  if(!k){consume(x,[](std::size_t){return Fr(0);});return;}auto K=kernel(k-1,rho,auth);consume(roots_[k-1].value,[&K](std::size_t j){return K.at(j);});};
 auto endpoint=[&](const std::vector<Fr>&point){Fr expected=0;for(std::size_t k=0;k<roots_.size();++k){auto d=r1::log2exact(roots_[k].spec.padded);std::vector<Fr>q(point.begin(),point.begin()+d);expected+=ui::selector(packing.segments()[k+1],point)*kernelEndpoint(k,rho,auth,q);}return expected;};
 ui::Inputs inputs;const auto*u_reference=std::getenv("ZKGPT_U_REFERENCE");
 if(io_.prover() && public_k && !(u_reference && std::string(u_reference)=="1")){
  for(auto&r:roots_){need(r.sealed_storage_valid && r.sealed_storage_valid(),"OPT3D sealed storage changed");need(r.sealed_data && r.sealed_stored==((single_pc_ || r.spec.model)?r.spec.actual:r.spec.padded),"OPT3D sealed root storage identity/length");}
  inputs=[&](std::size_t k){need(k<packing.segments().size(),"OPT3D sealed root plan bounds");if(!k)return ui::InputSpan{public_x.data(),public_x.size(),packing.segments()[0].padded};auto&r=roots_[k-1];need(r.sealed_storage_valid && r.sealed_storage_valid(),"OPT3D sealed storage changed");return ui::InputSpan{r.sealed_data,r.sealed_stored,r.spec.padded};};
 }
 auto reduced=ui::reduce(io_,packing.domain(),packing.segments(),initial,factors,endpoint,rng_,"UnifiedInput/sumcheck",public_k.get(),inputs);if(public_k)public_k->report();public_k.reset();auto opening_started=Clock::now();auto opening_cpu=processCPU();
 if(single_pc_){
  single::open(io_,*single_pc_,{reduced.point,reduced.endpoints.at(0).handle},public_x,io_.prover()?&*single_secret_:nullptr,io_.prover()?&reduced.endpoints.at(0):nullptr,rng_,"UnifiedInput/opening/C_U");
  Writer completed;completed.text(single::suite);completed.number(claims_.size());completed.number(1);completed.number(reduced.point.size());bindPublic(io_,"UnifiedInput/all-claims-authenticated",completed.b);io_.coin("UnifiedInput/all-claims-authenticated/ack");authenticated_=true;
  std::cerr<<"UNIFIED_OPENING {\"roots\":1,\"source_segments\":"<<roots_.size()<<",\"wall_seconds\":"<<std::chrono::duration<double>(Clock::now()-opening_started).count()<<",\"cpu_seconds\":"<<processCPU()-opening_cpu<<",\"includes_single_matrix_fold_and_hidden_equality\":true}\n";
  std::cerr<<"UNIFIED_INPUT {\"claims\":"<<claims_.size()<<",\"roots_opened\":1,\"source_segments\":"<<roots_.size()<<",\"domain\":"<<packing.domain()<<",\"sumcheck_rounds\":"<<reduced.point.size()<<",\"full_input_copy_bytes\":0,\"private_folded_input_copy_bytes\":"<<single_pc_->plan().columns*sizeof(Fr)<<",\"old_root_sumchecks\":0,\"production_gate\":\"BLOCKED_CRYPTOGRAPHIC_DESIGN\",\"may_enter_stage2\":false}\n";return;
 }
 std::vector<Hidden>opened;std::vector<Fr>selectors;
 for(std::size_t k=0;k<roots_.size();++k){auto d=r1::log2exact(roots_[k].spec.padded);std::vector<Fr>q(reduced.point.begin(),reduced.point.begin()+d);Fr value=0;if(io_.prover()){Weights w(q);for(std::size_t j=0;j<roots_[k].spec.padded;++j)value+=w.at(j)*roots_[k].value(j);}auto name="UnifiedInput/opening/root/"+std::to_string(k);auto h=exchangeHidden(io_,name+"/evaluation",value,rng_);rootOpening(k,{q,h.handle},io_.prover()?&h:nullptr,name+"/hyrax");opened.push_back(h);selectors.push_back(ui::selector(packing.segments()[k+1],reduced.point));}
 auto dx=r1::log2exact(packing.segments()[0].padded);std::vector<Fr>qx(reduced.point.begin(),reduced.point.begin()+dx);Weights wx(qx);Fr public_value=0;for(std::size_t j=0;j<public_x.size();++j)public_value+=wx.at(j)*public_x[j];public_value*=ui::selector(packing.segments()[0],reduced.point);
 auto stitched=combine(session_,"UnifiedInput/opening/stitched",opened,selectors);bindPublic(io_,"UnifiedInput/opening/stitched",encode(stitched.handle));auto public_part=publicHidden(io_,"UnifiedInput/opening/public-X",public_value);
 std::vector<Hidden>w{stitched,public_part,reduced.endpoints.at(0)};Relations relation;relation.role="UnifiedInput/opening/input-consistency";for(auto&h:w)relation.handles.push_back(h.handle);relation.linear={{Fr(1),Fr(1),Fr(-1)}};sigma(io_,relation,io_.prover()?&w:nullptr,rng_);
 Writer completed;completed.text(ui::suite);completed.number(claims_.size());completed.number(opened.size());completed.number(reduced.point.size());bindPublic(io_,"UnifiedInput/all-claims-authenticated",completed.b);io_.coin("UnifiedInput/all-claims-authenticated/ack");authenticated_=true;
 std::cerr<<"UNIFIED_OPENING {\"roots\":"<<opened.size()<<",\"wall_seconds\":"<<std::chrono::duration<double>(Clock::now()-opening_started).count()<<",\"cpu_seconds\":"<<processCPU()-opening_cpu<<",\"includes_root_MLE_and_hidden_stitch\":true}\n";
 std::cerr<<"UNIFIED_INPUT {\"claims\":"<<claims_.size()<<",\"roots_opened\":"<<opened.size()<<",\"domain\":"<<packing.domain()<<",\"sumcheck_rounds\":"<<reduced.point.size()<<",\"full_input_copy_bytes\":0,\"private_folded_input_copy_bytes\":0,\"old_root_sumchecks\":0,\"production_gate\":\"BLOCKED_CRYPTOGRAPHIC_DESIGN\",\"may_enter_stage2\":false}\n";
}
}
