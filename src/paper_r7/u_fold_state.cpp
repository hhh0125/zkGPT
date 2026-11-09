#include "u_fold_state.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
namespace zkgpt::r7::oneblock::unified {
namespace {using Clock=std::chrono::steady_clock;constexpr std::uint64_t maximum=16ull<<30;
bool same(InputSpan a,InputSpan b){return a.data==b.data && a.stored==b.stored && a.padded==b.padded && bool(a.read)==bool(b.read) && a.storage_identity==b.storage_identity && a.storage_offset==b.storage_offset;}
void shape(InputSpan s,std::size_t padded,bool deferred){need(s.padded==padded && s.stored<=s.padded && (!s.stored || s.data || (deferred && s.read && s.storage_identity)),"PERF4 U original storage shape");}
}
struct FoldedU::Impl {
 struct Root {InputSpan original;std::vector<Fr>values;bool seen=false;std::size_t captured=0;};
 bool deferred=false;unsigned admission=0;std::size_t n;unsigned bits=0;std::uint64_t cap,used=0;bool enabled=false,failed=false,consuming=false,ready=false;
 std::string session;std::vector<Segment>segments;std::vector<Root>roots;std::vector<Fr>point;UFoldMetrics m;
 Impl(std::size_t domain,const std::vector<Segment>&s,const Inputs&input,const std::string&id,std::uint64_t budget,bool callbacks):deferred(callbacks),n(domain),cap(callbacks?std::min<std::uint64_t>(budget,512ull<<20):budget),session(id){
  validate(n,s);need(input && !id.empty() && id.size()<=4096 && budget<=maximum,"PERF4 U constructor bounds");for(auto width=n;width>1;width/=2)++bits;
  std::uint64_t first=0;for(auto segment:s){auto count=std::max<std::size_t>(1,segment.padded/2);first+=count*sizeof(Fr);}
  const std::uint64_t meta=65536+s.size()*(2*sizeof(Root)+2*sizeof(Segment));
  if(deferred){
   // Public checkpoint selection only. Never retain the complete original U
   // or its first half: wait until actual tile contractions fit this budget.
   for(admission=0;admission<bits;++admission){first=0;for(auto segment:s)first+=std::max<std::size_t>(1,segment.padded>>std::min(admission,r1::log2exact(segment.padded)))*sizeof(Fr);if(first<=cap && meta<=cap-first)break;}
   if(admission==bits)return;
  }
  if(!bits || first>cap || meta>cap-first)return; // Public admission, before any retained private vector.
  segments=s;roots.resize(s.size());point.reserve(bits);
  used=sizeof(FoldedU)+sizeof(*this)+session.capacity()+segments.capacity()*sizeof(Segment)+roots.capacity()*sizeof(Root)+point.capacity()*sizeof(Fr);
  need(used<=meta && first<=cap-used,"PERF4 U metadata capacity budget");
  for(std::size_t k=0;k<s.size();++k){auto original=input(k);shape(original,s[k].padded,deferred);
   // Retain a nonallocating presence marker, never the owning reader closure.
   // The actual immutable reader is borrowed afresh for each round.
   if(deferred && original.read)original.read=[](std::size_t)->Fr{throw std::runtime_error("category U descriptor marker cannot read");};
   roots[k].original=std::move(original);}
  enabled=true;m.capacity_peak_bytes=used;
 }
 void check(const std::string&id,const std::vector<Fr>*prefix=nullptr)const{need(enabled && !failed && id==session && (!prefix || *prefix==point),"PERF4 U session/prefix/failed state");}
};
FoldedU::FoldedU(std::size_t n,const std::vector<Segment>&s,const Inputs&i,const std::string&id,std::uint64_t cap,bool deferred):p_(std::make_unique<Impl>(n,s,i,id,cap,deferred)){}
FoldedU::~FoldedU()=default;
bool FoldedU::requested(){auto p=std::getenv("ZKGPT_PERF4_U_FOLD");need(!p || std::string(p)=="0" || std::string(p)=="1","PERF4 U mode");return !p || std::string(p)=="1";}
std::uint64_t FoldedU::environmentBudget(){auto p=std::getenv("ZKGPT_PERF4_U_RAM_BYTES");if(!p)return maximum;std::string text(p);need(!text.empty() && text.find_first_not_of("0123456789")==std::string::npos,"PERF4 U budget syntax");auto cap=std::stoull(text);need(cap<=maximum,"PERF4 U budget cap");return cap;}
bool FoldedU::enabled()const{return p_->enabled;}
bool FoldedU::cached()const{return p_->enabled && !p_->failed && p_->point.size()>(p_->deferred?p_->admission:0);}std::size_t FoldedU::domain()const{return p_->n;}
void FoldedU::beginRound(const std::string&id,const std::vector<Fr>&prefix,const std::vector<Segment>&s){try{
 p_->check(id,&prefix);need(!p_->consuming && !p_->ready && prefix.size()<p_->bits && s.size()==p_->segments.size(),"PERF4 U round order");
 for(std::size_t k=0;k<s.size();++k)need(s[k].offset==p_->segments[k].offset && s[k].padded==p_->segments[k].padded,"PERF4 U segment drift");
 for(auto&r:p_->roots){r.seen=false;}
 p_->consuming=true;
}catch(...){poison();throw;}}
InputSpan FoldedU::span(std::size_t k,const std::string&id,const std::vector<Fr>&prefix,InputSpan original){try{
 p_->check(id,&prefix);need(p_->consuming && k<p_->roots.size(),"PERF4 U read phase/root");auto&r=p_->roots[k];need(!r.seen && same(original,r.original),"PERF4 U repeated read/storage drift");r.seen=true;
 if(p_->deferred && prefix.size()==p_->admission){
  auto count=std::max<std::size_t>(1,r.original.padded>>std::min<unsigned>(prefix.size(),r1::log2exact(r.original.padded)));
  need(r.values.empty() && count*sizeof(Fr)<=p_->cap-p_->used,"category U checkpoint preflight");r.values.resize(count);
  auto bytes=r.values.capacity()*sizeof(Fr);need(bytes<=p_->cap-p_->used,"category U checkpoint capacity");p_->used+=bytes;p_->m.capacity_peak_bytes=std::max(p_->m.capacity_peak_bytes,p_->used);r.captured=0;++p_->m.roots_materialized;
 }
 if(!cached()){return original;}
 need(r.values.size()==std::max<std::size_t>(1,r.original.padded>>std::min<unsigned>(prefix.size(),r1::log2exact(r.original.padded))),"PERF4 U folded local length");
 return {r.values.data(),r.values.size(),r.values.size()};
}catch(...){poison();throw;}}
void FoldedU::capture(std::size_t k,std::size_t first,const Fr*values,std::size_t count){try{
 if(!p_->deferred || p_->point.size()!=p_->admission)return;
 need(p_->enabled && !p_->failed && p_->consuming && k<p_->roots.size() && values,"category U capture phase");auto&r=p_->roots[k];
 need(first==r.captured && count<=r.values.size()-r.captured,"category U checkpoint coverage");std::copy_n(values,count,r.values.begin()+first);r.captured+=count;p_->m.fold_write_coordinates+=count;
}catch(...){poison();throw;}}
void FoldedU::completeRound(const std::string&id,const std::vector<Fr>&prefix){try{p_->check(id,&prefix);need(p_->consuming,"PERF4 U completion phase");for(const auto&r:p_->roots){need(r.seen,"PERF4 U incomplete root consumption");if(p_->deferred && prefix.size()==p_->admission)need(r.captured==r.values.size(),"category U incomplete checkpoint");}p_->consuming=false;p_->ready=true;}catch(...){poison();throw;}}
void FoldedU::advance(const std::string&id,const Fr&challenge,const Inputs&original){auto start=Clock::now();auto cpu=crypto::processCPU();
 auto timing=[&]{p_->m.fold_wall_seconds+=std::chrono::duration<double>(Clock::now()-start).count();p_->m.fold_cpu_seconds+=crypto::processCPU()-cpu;};
 try{
  p_->check(id);need(p_->ready && !p_->consuming && p_->point.size()<p_->bits && challenge.isValid() && original,"PERF4 U advancement order/challenge");
  // Validate ALL descriptors before modifying any root. No callback is retained.
  for(std::size_t k=0;k<p_->roots.size();++k)need(same(original(k),p_->roots[k].original),"PERF4 U original span changed before advance");
  for(auto&root:p_->roots){
   if(p_->deferred){
    if(p_->point.size()>=p_->admission && root.values.size()>1){auto n=root.values.size();for(std::size_t j=0;j<n/2;++j)root.values[j]=root.values[2*j]+challenge*(root.values[2*j+1]-root.values[2*j]);root.values.resize(n/2);p_->m.cache_fold_read_coordinates+=n;p_->m.fold_input_coordinates+=n;p_->m.fold_write_coordinates+=n/2;p_->m.multiplications+=n/2;p_->m.additions+=n;}
   }else if(p_->point.empty()){
    const auto n=root.original.padded,count=std::max<std::size_t>(1,n/2);need(count*sizeof(Fr)<=p_->cap-p_->used,"PERF4 U private capacity preflight");std::vector<Fr>next(count);
    auto bytes=next.capacity()*sizeof(Fr);need(bytes<=p_->cap-p_->used,"PERF4 U unexpected vector capacity");p_->used+=bytes;p_->m.capacity_peak_bytes=std::max(p_->m.capacity_peak_bytes,p_->used);
    auto read=[&](std::size_t j){return j<root.original.stored?root.original.data[j]:Fr(0);};
    if(n==1)next[0]=read(0);else{for(std::size_t j=0;j<count;++j){auto left=read(2*j),right=read(2*j+1);next[j]=left+challenge*(right-left);}p_->m.fold_input_coordinates+=n;p_->m.multiplications+=count;p_->m.additions+=n;}
    p_->m.original_read_coordinates+=root.original.stored;p_->m.fold_write_coordinates+=count;root.values.swap(next);++p_->m.roots_materialized;
   }else if(root.values.size()>1){auto n=root.values.size();for(std::size_t j=0;j<n/2;++j)root.values[j]=root.values[2*j]+challenge*(root.values[2*j+1]-root.values[2*j]);root.values.resize(n/2);p_->m.cache_fold_read_coordinates+=n;p_->m.fold_input_coordinates+=n;p_->m.fold_write_coordinates+=n/2;p_->m.multiplications+=n/2;p_->m.additions+=n;}
   // A scalar is kept unchanged; global selector scaling belongs to streaming.
  }
  p_->point.push_back(challenge);p_->ready=false;++p_->m.advances;p_->m.live_bytes=0;for(const auto&root:p_->roots)p_->m.live_bytes+=root.values.size()*sizeof(Fr);timing();
 }catch(...){poison();timing();throw;}
}
void FoldedU::poison()noexcept{p_->failed=true;}const UFoldMetrics&FoldedU::metrics()const{return p_->m;}
void FoldedU::report()const{const auto&m=p_->m;std::cerr<<"PERF4_U_STATE {\"enabled\":"<<(p_->enabled?"true":"false")<<",\"failed\":"<<(p_->failed?"true":"false")<<",\"budget_bytes\":"<<p_->cap<<",\"capacity_peak_bytes\":"<<m.capacity_peak_bytes<<",\"live_bytes\":"<<m.live_bytes<<",\"roots_materialized\":"<<m.roots_materialized<<",\"advances\":"<<m.advances<<",\"original_read_coordinates\":"<<m.original_read_coordinates<<",\"cache_fold_read_coordinates\":"<<m.cache_fold_read_coordinates<<",\"fold_input_coordinates\":"<<m.fold_input_coordinates<<",\"fold_write_coordinates\":"<<m.fold_write_coordinates<<",\"Fr_multiplications\":"<<m.multiplications<<",\"Fr_additions\":"<<m.additions<<",\"fold_wall_seconds\":"<<m.fold_wall_seconds<<",\"fold_cpu_seconds\":"<<m.fold_cpu_seconds<<",\"deferred_callback_mode\":"<<(p_->deferred?"true":"false")<<",\"checkpoint_round\":"<<p_->admission<<",\"private_files\":0}\n";}
}
