#include "paper_r7_public_k.hpp"
#include "paper_r7_k_mmap_buffer.hpp"
#include "paper_r7_opt2_metrics.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <sys/resource.h>

namespace zkgpt::r7::oneblock::unified {
namespace {
using Clock=std::chrono::steady_clock;
double seconds(Clock::time_point t){return std::chrono::duration<double>(Clock::now()-t).count();}
std::uint64_t rssBytes(){
 std::ifstream f("/proc/self/statm");std::uint64_t total=0,resident=0;
 auto page=::sysconf(_SC_PAGESIZE);
 return page>0 && (f>>total>>resident)?resident*std::uint64_t(page):0;
}
std::uint64_t peakRssBytes(){
 struct rusage r{};return ::getrusage(RUSAGE_SELF,&r)==0?std::uint64_t(r.ru_maxrss)*1024:0;
}
void add(std::uint64_t&value,std::uint64_t increment){
 need(increment<=std::numeric_limits<std::uint64_t>::max()-value,"K mmap metric byte count overflow");
 value+=increment;
}
}
// Kept only for callers compiled against the old API. Budgets no longer select
// K storage; in particular environment values cannot restore eviction or I/O.
KBudget KBudget::environment(){return {};}
struct PublicK::Impl {
 struct Root {detail::KFrBuffer values;bool built=false,touched=false;};
 std::vector<Segment>segments;Build build;std::string session;
 std::vector<Fr>point;std::vector<Root>roots;unsigned bits=0;
 bool ended=false,failed=false;KMetrics metrics;
 Impl(const std::vector<Segment>&s,Build b,KBudget,const std::string&id,std::size_t domain)
 :segments(s),build(std::move(b)),session(id),roots(s.size()){
  need(build && !session.empty() && !segments.empty() && segments.size()<=8193,"public K constructor bound");
  std::size_t end=0;
  for(auto segment:segments){
   need(segment.padded && !(segment.padded&(segment.padded-1)) &&
        segment.padded<=r1::kMaxVector && segment.offset<=(std::size_t(1)<<32)-segment.padded,
        "public K physical root bound");
   end=std::max(end,segment.offset+segment.padded);
  }
  if(!domain){domain=1;while(domain<end)domain*=2;}
  validate(domain,segments);while(domain>1){++bits;domain/=2;}point.reserve(bits);
 }
 void check(const std::string&id)const{need(id==session,"public K foreign authentication session");}
 void poison()noexcept{
  failed=true;for(auto&root:roots)root.values.clear();
  metrics.mmap_live_bytes=metrics.mapped_bytes=metrics.resident_bytes=0;
 }
 void peak(){
  metrics.mmap_peak_bytes=std::max(metrics.mmap_peak_bytes,metrics.mmap_live_bytes);
  metrics.mapped_peak_bytes=std::max(metrics.mapped_peak_bytes,metrics.mapped_bytes);
 }
 void resident(){
  auto page=::sysconf(_SC_PAGESIZE);
  if(page<=0){metrics.resident_measurement_available=false;metrics.resident_bytes=0;return;}
  std::uint64_t bytes=0;std::vector<unsigned char>pages;
  for(const auto&root:roots){
   if(!root.values.mappedBytes())continue;
   pages.resize(root.values.mappedBytes()/std::size_t(page));
   if(::mincore(const_cast<Fr*>(root.values.data()),root.values.mappedBytes(),pages.data())!=0){
    metrics.resident_measurement_available=false;metrics.resident_bytes=0;return;
   }
   for(auto value:pages)if(value&1)add(bytes,std::uint64_t(page));
  }
  metrics.resident_bytes=bytes;
  metrics.resident_peak_bytes=std::max(metrics.resident_peak_bytes,bytes);
 }
 Fr at(std::size_t k,std::size_t j){
  auto&root=roots.at(k);need(j<root.values.size(),"public K read index");
  ++metrics.state_read_coordinates;return root.values[j];
 }
 void with(std::size_t k,const std::function<void(const Value&)>&consume){
  auto&root=roots.at(k);root.touched=true;
  if(!root.built){
   need(point.empty(),"public K unbuilt root after challenge");
   auto start=Clock::now();auto original=build(k);
   need(original.size()==segments[k].padded,"public K builder shape");
   metrics.initial_build_seconds+=seconds(start);
   metrics.builder_peak_bytes=std::max<std::uint64_t>(metrics.builder_peak_bytes,original.capacity()*sizeof(Fr));
   start=Clock::now();detail::KFrBuffer next(original.size());
   std::copy(original.begin(),original.end(),next.data());
   metrics.mmap_allocation_seconds+=seconds(start);
   add(metrics.mmap_live_bytes,next.size()*sizeof(Fr));
   add(metrics.mapped_bytes,next.mappedBytes());peak();
   root.values=std::move(next);root.built=true;
   ++metrics.unique_root_builds;++metrics.initial_builds;
   // The vector is released before the consumer, avoiding a borrowed temporary.
  }
  consume([this,k](std::size_t j){return at(k,j);});
 }
 void advance(const Fr&challenge){
  // Validate every root before changing any value or reclaiming a page.
  for(const auto&root:roots)need(root.built && root.touched,"public K incomplete round state");
  resident();auto start=Clock::now();auto cpu=crypto::processCPU();
  auto released_before=metrics.unmapped_tail_bytes;
  for(auto&root:roots){
   root.touched=false;auto n=root.values.size();if(n<=1)continue;
   for(std::size_t j=0;j<n/2;++j){
    auto left=root.values[2*j],right=root.values[2*j+1];
    root.values[j]=left+challenge*(right-left);
   }
   add(metrics.fold_coordinates,n);
   auto before=root.values.mappedBytes();auto unmap_start=Clock::now();
   root.values.shrinkTo(n/2);
   metrics.unmap_wall_seconds+=seconds(unmap_start);
   auto released=before-root.values.mappedBytes();
   if(released){++metrics.unmap_calls;add(metrics.unmapped_tail_bytes,released);metrics.mapped_bytes-=released;}
   metrics.mmap_live_bytes-=(n/2)*sizeof(Fr);
  }
  point.push_back(challenge);
  metrics.fold_cpu_seconds+=crypto::processCPU()-cpu;metrics.fold_wall_seconds+=seconds(start);
  resident();
  if(crypto::opt2MetricsEnabled())
   std::cerr<<"K_MMAP_ROUND {\"round\":"<<point.size()-1
    <<",\"K_effective_elements\":"<<metrics.mmap_live_bytes/sizeof(Fr)
    <<",\"K_mmap_live_bytes\":"<<metrics.mmap_live_bytes<<",\"K_mapped_bytes\":"<<metrics.mapped_bytes
    <<",\"K_resident_bytes\":"<<metrics.resident_bytes
    <<",\"K_resident_measurement_available\":"<<(metrics.resident_measurement_available?"true":"false")
    <<",\"released_tail_bytes\":"<<metrics.unmapped_tail_bytes-released_before
    <<",\"K_unmapped_tail_bytes\":"<<metrics.unmapped_tail_bytes
    <<",\"process_rss_bytes\":"<<rssBytes()<<",\"process_peak_rss_bytes\":"<<peakRssBytes()<<"}\n";
 }
};
PublicK::PublicK(const std::vector<Segment>&s,Build b,KBudget cap,const std::string&id,std::size_t domain)
 :p_(std::make_unique<Impl>(s,std::move(b),cap,id,domain)){}
PublicK::~PublicK()=default;
void PublicK::withRoot(std::size_t k,const std::string&id,const std::function<void(const Value&)>&consume){
 try{p_->check(id);need(!p_->ended && !p_->failed && bool(consume),"public K closed/failed state or missing consumer");p_->with(k,consume);}
 catch(...){p_->poison();throw;}
}
void PublicK::advance(const std::string&id,const Fr&challenge){
 try{p_->check(id);need(!p_->ended && !p_->failed && challenge.isValid() && p_->point.size()<p_->bits,
                      "public K challenge/phase bound");p_->advance(challenge);}
 catch(...){p_->poison();throw;}
}
Fr PublicK::endpoint(const std::string&id,const std::vector<Fr>&point){
 try{
  p_->check(id);need(!p_->ended && !p_->failed && point==p_->point && point.size()==p_->bits,
                    "public K endpoint state order/terminal dimension");
  p_->ended=true;Fr out=0;
  for(std::size_t k=0;k<p_->segments.size();++k)
   p_->with(k,[&](const Value&v){out+=selector(p_->segments[k],point)*v(0);});
  return out;
 }catch(...){p_->poison();throw;}
}
const KMetrics&PublicK::metrics()const{p_->resident();return p_->metrics;}
void PublicK::report()const{
 p_->resident();const auto&m=p_->metrics;
 std::cerr<<"OPT3C_K {\"storage\":\"anonymous-mmap\",\"legacy_cache_metrics_deprecated\":true,";
#define K_METRIC(name,field) std::cerr<<"\"" name "\":"<<m.field<<",";
 K_METRIC("K_unique_root_builds",unique_root_builds) K_METRIC("K_rebuild_calls",rebuild_calls)
 K_METRIC("K_mmap_live_bytes",mmap_live_bytes) K_METRIC("K_mmap_peak_bytes",mmap_peak_bytes)
 K_METRIC("K_mapped_bytes",mapped_bytes) K_METRIC("K_mapped_peak_bytes",mapped_peak_bytes)
 K_METRIC("K_resident_bytes",resident_bytes) K_METRIC("K_resident_peak_bytes",resident_peak_bytes)
 K_METRIC("K_unmapped_tail_bytes",unmapped_tail_bytes) K_METRIC("K_unmap_calls",unmap_calls)
 K_METRIC("K_fold_coordinates",fold_coordinates) K_METRIC("K_state_read_coordinates",state_read_coordinates)
 K_METRIC("K_initial_build_seconds",initial_build_seconds)
 K_METRIC("K_fold_wall_seconds",fold_wall_seconds) K_METRIC("K_fold_cpu_seconds",fold_cpu_seconds)
 K_METRIC("K_mmap_allocation_seconds",mmap_allocation_seconds) K_METRIC("K_unmap_wall_seconds",unmap_wall_seconds)
 K_METRIC("K_file_read_bytes",file_read_bytes) K_METRIC("K_file_write_bytes",file_write_bytes)
 K_METRIC("K_builder_peak_bytes",builder_peak_bytes)
 K_METRIC("K_initial_builds",initial_builds) K_METRIC("fallback_rebuild_calls",fallback_rebuild_calls)
#undef K_METRIC
 std::cerr<<"\"K_resident_measurement_available\":"<<(m.resident_measurement_available?"true":"false")
  <<",\"failed\":"<<(p_->failed?"true":"false")<<",\"process_rss_bytes\":"<<rssBytes()
  <<",\"process_peak_rss_bytes\":"<<peakRssBytes()<<"}\n";
}
}
