#include "paper_r7_validated_layout.hpp"
#include "paper_r7_u_fold_state.hpp"
#include "paper_r7_parallel_fold.hpp"
#include <algorithm>
#include <chrono>
#include <limits>
#include <new>
#include <memory>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include "paper_r7_opt2_metrics.hpp"
namespace zkgpt::r7::oneblock::unified {
namespace {
using Clock=std::chrono::steady_clock;
double seconds(Clock::time_point t){return std::chrono::duration<double>(Clock::now()-t).count();}
class FrBuffer {
 Fr* data_=nullptr;std::size_t size_=0,mapped_bytes_=0,page_size_=0;
 std::size_t aligned(std::size_t count) const {
  if(count>std::numeric_limits<std::size_t>::max()/sizeof(Fr))throw std::bad_alloc();
  auto bytes=count*sizeof(Fr);
  if(bytes>std::numeric_limits<std::size_t>::max()-(page_size_-1))throw std::bad_alloc();
  return ((bytes+page_size_-1)/page_size_)*page_size_;
 }
 public:
 FrBuffer()=default;
 explicit FrBuffer(std::size_t count){
  auto page=::sysconf(_SC_PAGESIZE);if(page<=0)throw std::runtime_error("U page size unavailable");page_size_=static_cast<std::size_t>(page);
  mapped_bytes_=aligned(count);if(!mapped_bytes_)return;
  auto ptr=::mmap(nullptr,mapped_bytes_,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
  if(ptr==MAP_FAILED){mapped_bytes_=0;throw std::bad_alloc();}data_=static_cast<Fr*>(ptr);
  try{for(;size_<count;++size_)::new(static_cast<void*>(data_+size_))Fr();}catch(...){clear();throw;}
 }
 ~FrBuffer(){clear();}
 FrBuffer(const FrBuffer&)=delete;FrBuffer&operator=(const FrBuffer&)=delete;
 FrBuffer(FrBuffer&&other)noexcept{swap(other);}
 FrBuffer&operator=(FrBuffer&&other)noexcept{if(this!=&other){clear();swap(other);}return *this;}
 void swap(FrBuffer&other)noexcept{std::swap(data_,other.data_);std::swap(size_,other.size_);std::swap(mapped_bytes_,other.mapped_bytes_);std::swap(page_size_,other.page_size_);}
 Fr*data()noexcept{return data_;}const Fr*data()const noexcept{return data_;}
 std::size_t size()const noexcept{return size_;}std::size_t mappedBytes()const noexcept{return mapped_bytes_;}
 Fr&operator[](std::size_t i){return data_[i];}const Fr&operator[](std::size_t i)const{return data_[i];}
 void shrinkTo(std::size_t count){
  if(count>size_)throw std::logic_error("U FrBuffer cannot grow");
  if(count==size_)return;
  std::destroy_n(data_+count,size_-count);size_=count;
  auto bytes=aligned(count);
  if(bytes<mapped_bytes_){if(::munmap(reinterpret_cast<char*>(data_)+bytes,mapped_bytes_-bytes)!=0)throw std::runtime_error("U tail munmap failed");mapped_bytes_=bytes;if(!bytes)data_=nullptr;}
 }
 void clear()noexcept{
  if(data_){std::destroy_n(data_,size_);size_=0;
   // A transient failure (also exercised by fault injection) is retried once.
   // A persistent OS failure cannot be hidden by a noexcept destructor.
   if(::munmap(data_,mapped_bytes_)!=0 && ::munmap(data_,mapped_bytes_)!=0)std::terminate();
  }
  data_=nullptr;size_=mapped_bytes_=page_size_=0;
 }
};
std::uint64_t residentBytes(){std::ifstream f("/proc/self/statm");std::uint64_t total=0,resident=0;if(!(f>>total>>resident))return 0;auto page=::sysconf(_SC_PAGESIZE);return page>0?resident*static_cast<std::uint64_t>(page):0;}
std::uint64_t peakResidentBytes(){struct rusage r{};return ::getrusage(RUSAGE_SELF,&r)==0?static_cast<std::uint64_t>(r.ru_maxrss)*1024:0;}
bool same(InputSpan a,InputSpan b){return a.data==b.data && a.stored==b.stored && a.padded==b.padded && bool(a.read)==bool(b.read) && bool(a.window)==bool(b.window) && a.storage_identity==b.storage_identity && a.storage_offset==b.storage_offset && a.stable_windows==b.stable_windows;}
void shape(InputSpan s,std::size_t padded,bool callbacks){need(s.padded==padded && s.stored<=s.padded && (!s.stored || s.data || (callbacks && s.read && s.storage_identity)),"PERF4 U original storage shape");}
}
struct FoldedU::Impl {
 struct Root {InputSpan original;FrBuffer values;bool seen=false;};
 std::size_t n;unsigned bits=0;bool enabled=true,failed=false,consuming=false,ready=false;
 std::string session;detail::ValidatedLayout layout;std::vector<Root>roots;std::vector<Fr>point;UFoldMetrics m;
 Impl(const detail::ValidatedLayout&fixed,const Inputs&input,const std::string&id):n(fixed.domain()),session(id),layout(fixed){
  const auto&s=layout.segments();need(input && !id.empty() && id.size()<=4096,"U constructor bounds");for(auto width=n;width>1;width/=2)++bits;
  roots.resize(s.size());point.reserve(bits);
  for(std::size_t k=0;k<s.size();++k){auto original=input(k);shape(original,s[k].padded,true);
   if(original.read)original.read=[](std::size_t)->Fr{throw std::runtime_error("U descriptor marker cannot read");};
   if(original.window)original.window=[](std::size_t,std::size_t)->InputWindow{throw std::runtime_error("U descriptor marker cannot borrow");};
   roots[k].original=std::move(original);
  }
 }
 void check(const std::string&id,const std::vector<Fr>*prefix=nullptr)const{need(enabled && !failed && id==session && (!prefix || *prefix==point),"PERF4 U session/prefix/failed state");}
};
FoldedU::FoldedU(std::size_t n,const std::vector<Segment>&s,const Inputs&i,const std::string&id):FoldedU(detail::ValidatedLayout(n,s),i,id){}
FoldedU::FoldedU(const detail::ValidatedLayout&fixed,const Inputs&i,const std::string&id):p_(std::make_unique<Impl>(fixed,i,id)){}
FoldedU::~FoldedU()=default;
bool FoldedU::enabled()const{return p_->enabled && !p_->failed;}
bool FoldedU::cached()const{return enabled() && !p_->point.empty();}
std::size_t FoldedU::domain()const{return p_->n;}
void FoldedU::beginRound(const std::string&id,const std::vector<Fr>&prefix,const std::vector<Segment>&s){try{
 const auto&fixed=p_->layout.segments();
 p_->check(id,&prefix);need(!p_->consuming && !p_->ready && prefix.size()<p_->bits && s.size()==fixed.size(),"PERF4 U round order");
 if(&s!=&fixed)for(std::size_t k=0;k<s.size();++k)need(s[k].offset==fixed[k].offset && s[k].padded==fixed[k].padded,"PERF4 U segment drift");
 for(auto&r:p_->roots){r.seen=false;}
 p_->consuming=true;
}catch(...){poison();throw;}}
InputSpan FoldedU::span(std::size_t k,const std::string&id,const std::vector<Fr>&prefix,InputSpan original){try{
 p_->check(id,&prefix);need(p_->consuming && k<p_->roots.size(),"PERF4 U read phase/root");auto&r=p_->roots[k];need(!r.seen && same(original,r.original),"PERF4 U repeated read/storage drift");r.seen=true;
 if(!cached()){return original;}
 need(r.values.size()==std::max<std::size_t>(1,r.original.padded>>std::min<unsigned>(prefix.size(),r1::log2exact(r.original.padded))),"PERF4 U folded local length");
 return {r.values.data(),r.values.size(),r.values.size()};
}catch(...){poison();throw;}}
void FoldedU::capture(std::size_t,std::size_t,const Fr*,std::size_t){}
void FoldedU::completeRound(const std::string&id,const std::vector<Fr>&prefix){try{p_->check(id,&prefix);need(p_->consuming,"U completion phase");for(const auto&r:p_->roots)need(r.seen,"U incomplete root consumption");p_->consuming=false;p_->ready=true;}catch(...){poison();throw;}}
void FoldedU::advance(const std::string&id,const Fr&challenge,const Inputs&original,detail::Workers*workers){
 auto start=Clock::now();auto cpu=crypto::processCPU();
 auto timing=[&]{p_->m.fold_wall_seconds+=seconds(start);p_->m.fold_cpu_seconds+=crypto::processCPU()-cpu;};
 try{
  p_->check(id);need(p_->ready && !p_->consuming && p_->point.size()<p_->bits && challenge.isValid() && original,"PERF4 U advancement order/challenge");
  // Validate all descriptors before any allocation or mutation.
  for(std::size_t k=0;k<p_->roots.size();++k)need(same(original(k),p_->roots[k].original),"PERF4 U original span changed before advance");
  const bool materializing=p_->point.empty(),parallel=workers && workers->count()>1;
  const auto released_before=p_->m.unmapped_tail_bytes;
  const auto original_before=p_->m.original_read_coordinates;
  std::vector<std::size_t> counts;
  if(parallel)counts.resize(p_->roots.size());
  if(materializing){
   struct WindowTask {const Fr*source;Fr*output;std::size_t pairs;};
   std::vector<WindowTask>windows;
   constexpr std::size_t maximum_pending_windows=4096;
   auto foldWindow=[&](const Fr*source,Fr*output,std::size_t first,std::size_t end){
    for(auto j=first;j<end;++j){
     const auto&left=source[2*j];const auto&right=source[2*j+1];
     output[j]=left+challenge*(right-left);
    }
   };
   auto flushWindows=[&]{
    std::vector<std::size_t>lengths;
    lengths.reserve(windows.size());
    for(const auto&window:windows)lengths.push_back(window.pairs);
    p_->m.parallel_materialization_jobs+=detail::foldRanges(lengths,0,std::numeric_limits<std::size_t>::max(),*workers,false,
       [&](std::size_t k,std::size_t first,std::size_t end){
        const auto&window=windows[k];
        foldWindow(window.source,window.output,first,end);
       });
    windows.clear();
   };
   // For direct spans, fold complete pairs without a padding test or callback
   // in the inner loop. The partial pair and padded suffix still use zeros.
   auto direct=[&](std::size_t k,std::size_t first,std::size_t end){
    auto&root=p_->roots[k];const auto&source=root.original;
    auto*out=root.values.data();
    if(source.padded==1){out[0]=source.stored?source.data[0]:Fr(0);return;}
    const auto full_end=std::min(end,source.stored/2);
    if(first<full_end){foldWindow(source.data,out,first,full_end);first=full_end;}
    if(first<end && 2*first<source.stored){const auto&left=source.data[2*first];out[first++]=left+challenge*(Fr(0)-left);}
    const Fr zero(0);
    for(;first<end;++first)out[first]=zero+challenge*(zero-zero);
   };
   for(std::size_t k=0;k<p_->roots.size();++k){
    auto&root=p_->roots[k];
    const auto source=original(k);
    need(same(source,root.original),"R8 U materialization storage drift");
    const auto n=source.padded,count=std::max<std::size_t>(1,n/2);
    auto map_start=Clock::now();
    root.values=FrBuffer(count);
    p_->m.mmap_allocation_seconds+=seconds(map_start);
    p_->m.mapped_bytes+=root.values.mappedBytes();
    p_->m.mapped_capacity_peak_bytes=std::max(p_->m.mapped_capacity_peak_bytes,p_->m.mapped_bytes);
    if(parallel && (source.data || !source.stored)){
     counts[k]=count;
    }else{
     auto arithmetic_start=Clock::now();
     if(workers && (source.data || !source.stored)){
      direct(k,0,count);
     }else{
      // Callback readers may have thread-affine owners or mutate counters.
      std::uint64_t callback_coordinates=0;
      auto read=[&](std::size_t j)->Fr{
       if(j>=source.stored)return Fr(0);
       if(source.data)return source.data[j];
       ++callback_coordinates;
       return source.read(j);
      };
      if(n==1)root.values[0]=read(0);
      else for(std::size_t first=0;first<count;){
       auto end=first+std::min<std::size_t>(4096,count-first);
       if(workers && source.window && 2*first<source.stored){
        const bool defer=parallel && source.stable_windows;
        const auto maximum=defer?source.stored-2*first:std::min(2*(end-first),source.stored-2*first);
        const auto window=source.window(2*first,maximum);
        need(window.count && window.count<=maximum,"U materialization source window bounds");
        const auto pairs=window.count/2;
        if(window.data && pairs){
         if(defer){
          windows.push_back({window.data,root.values.data()+first,pairs});
          p_->m.parallel_window_coordinates+=2*pairs;
          if(windows.size()==maximum_pending_windows)flushWindows();
         }else foldWindow(window.data,root.values.data()+first,0,pairs);
         first+=pairs;
         continue;
        }
        end=first+std::min(end-first,std::max<std::size_t>(1,pairs));
       }
       for(std::size_t j=first;j<end;++j){
        auto left=read(2*j),right=read(2*j+1);
        root.values[j]=left+challenge*(right-left);
       }
       first=end;
      }
      p_->m.original_callback_coordinates+=callback_coordinates;
     }
     p_->m.fold_arithmetic_wall_seconds+=seconds(arithmetic_start);
    }
    if(n>1){p_->m.fold_input_coordinates+=n;p_->m.multiplications+=count;p_->m.additions+=n;}
    
    p_->m.original_read_coordinates+=source.stored;
    p_->m.fold_write_coordinates+=count;
   }
   if(parallel){
    auto arithmetic_start=Clock::now();
    for(const auto&window:windows)counts.push_back(window.pairs);
    auto run=[&](std::size_t k,std::size_t first,std::size_t end){
     if(k<p_->roots.size()){direct(k,first,end);return;}
     const auto&window=windows[k-p_->roots.size()];
     foldWindow(window.source,window.output,first,end);
    };
    const auto jobs=detail::foldRanges(counts,0,std::numeric_limits<std::size_t>::max(),*workers,false,run);
    p_->m.fold_arithmetic_wall_seconds+=seconds(arithmetic_start);
    p_->m.parallel_materialization_jobs+=jobs;
   }
   p_->m.roots_materialized+=p_->roots.size();
   p_->m.materialization_wall_seconds=seconds(start);
   p_->m.materialization_cpu_seconds=crypto::processCPU()-cpu;
  }else{
   auto fold=[&](std::size_t k,std::size_t first,std::size_t end){
    auto*values=p_->roots[k].values.data();
    for(std::size_t j=first;j<end;++j){
     auto left=values[2*j],right=values[2*j+1];values[j]=left+challenge*(right-left);
    }
   };
   if(parallel){
    for(std::size_t k=0;k<counts.size();++k)counts[k]=p_->roots[k].values.size()/2;
    auto arithmetic_start=Clock::now();
    p_->m.parallel_fold_jobs+=detail::foldInPlace(counts,*workers,fold);
    p_->m.fold_arithmetic_wall_seconds+=seconds(arithmetic_start);
   }
   // Workers have joined. No thread can read a page while it is reclaimed.
   for(std::size_t k=0;k<p_->roots.size();++k){
    auto&root=p_->roots[k];const auto n=root.values.size();
    if(n<=1)continue;
    if(!parallel){
     auto arithmetic_start=Clock::now();fold(k,0,n/2);
     p_->m.fold_arithmetic_wall_seconds+=seconds(arithmetic_start);
    }
    auto before=root.values.mappedBytes();auto unmap_start=Clock::now();auto unmap_cpu=crypto::processCPU();
    root.values.shrinkTo(n/2);
    auto released=before-root.values.mappedBytes();
    if(released){++p_->m.unmap_calls;p_->m.unmapped_tail_bytes+=released;p_->m.mapped_bytes-=released;}
    p_->m.unmap_wall_seconds+=seconds(unmap_start);p_->m.unmap_cpu_seconds+=crypto::processCPU()-unmap_cpu;
    p_->m.cache_fold_read_coordinates+=n;p_->m.fold_input_coordinates+=n;p_->m.fold_write_coordinates+=n/2;
    p_->m.multiplications+=n/2;p_->m.additions+=n;
   }
  }
  p_->point.push_back(challenge);p_->ready=false;++p_->m.advances;p_->m.live_bytes=0;
  for(const auto&root:p_->roots)p_->m.live_bytes+=root.values.size()*sizeof(Fr);
  timing();
  if(crypto::opt2MetricsEnabled()){
   auto sample_start=Clock::now();const auto rss=residentBytes(),peak_rss=peakResidentBytes();
   p_->m.resident_sampling_seconds+=seconds(sample_start);
   std::cerr<<"R8_U_FOLD_ROUND {\"round\":"<<p_->point.size()-1<<",\"effective_elements\":"<<p_->m.live_bytes/sizeof(Fr)<<",\"live_bytes\":"<<p_->m.live_bytes<<",\"mapped_bytes\":"<<p_->m.mapped_bytes<<",\"released_tail_bytes\":"<<p_->m.unmapped_tail_bytes-released_before<<",\"unmapped_tail_bytes\":"<<p_->m.unmapped_tail_bytes<<",\"rss_bytes\":"<<rss<<",\"peak_rss_bytes\":"<<peak_rss<<",\"fold_wall_seconds\":"<<seconds(start)<<",\"fold_cpu_seconds\":"<<crypto::processCPU()-cpu<<",\"original_read_elements\":"<<p_->m.original_read_coordinates-original_before<<",\"materialization\":"<<(materializing?"true":"false")<<"}\n";
  }
 }catch(...){poison();timing();throw;}
}
void FoldedU::poison()noexcept{p_->failed=true;for(auto&root:p_->roots)root.values.clear();p_->m.live_bytes=0;p_->m.mapped_bytes=0;}
const UFoldMetrics&FoldedU::metrics()const{return p_->m;}
void FoldedU::report()const{const auto&m=p_->m;std::cerr<<"PERF4_U_STATE {";
#define U_METRIC(field) std::cerr<<"\"" #field "\":"<<m.field<<",";
 U_METRIC(live_bytes) U_METRIC(mapped_bytes) U_METRIC(mapped_capacity_peak_bytes) U_METRIC(unmapped_tail_bytes) U_METRIC(unmap_calls)
 U_METRIC(unmap_wall_seconds) U_METRIC(unmap_cpu_seconds) U_METRIC(roots_materialized) U_METRIC(advances)
 U_METRIC(original_read_coordinates) U_METRIC(cache_fold_read_coordinates) U_METRIC(fold_input_coordinates) U_METRIC(fold_write_coordinates)
 U_METRIC(multiplications) U_METRIC(additions) U_METRIC(fold_wall_seconds) U_METRIC(fold_cpu_seconds)
 U_METRIC(materialization_wall_seconds) U_METRIC(materialization_cpu_seconds)
 U_METRIC(original_callback_coordinates) U_METRIC(parallel_materialization_jobs) U_METRIC(parallel_fold_jobs) U_METRIC(parallel_window_coordinates)
 U_METRIC(mmap_allocation_seconds) U_METRIC(fold_arithmetic_wall_seconds) U_METRIC(resident_sampling_seconds)
#undef U_METRIC
 std::cerr<<"\"enabled\":"<<(enabled()?"true":"false")<<",\"failed\":"<<(p_->failed?"true":"false")<<",\"private_files\":0}\n";
}
}
