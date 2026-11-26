#include "paper_r7_validated_layout.hpp"
#include "paper_r7_unified_input.hpp"
#include "paper_r7_u_fold_state.hpp"
#include "paper_r7_opt2_metrics.hpp"
#include "paper_r7_public_k.hpp"
#include "paper_r7_unified_workers.hpp"
#include "paper_r7_native_workers.hpp"
#include <thread_joiner.hpp>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <time.h>
#include <set>
#include <sstream>
namespace zkgpt::r7::oneblock::unified {
namespace {
using Clock=std::chrono::steady_clock;
double threadCPU(){timespec t{};need(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t)==0,"thread CPU clock");return t.tv_sec+t.tv_nsec*1e-9;}
double elapsed(Clock::time_point t){return std::chrono::duration<double>(Clock::now()-t).count();}
struct Weights {
 const std::vector<Fr>&low;std::vector<Fr>high;unsigned split;std::size_t block;
 Weights(const std::vector<Fr>&p,unsigned d,const std::vector<Fr>&shared):low(shared),split(std::min(d,16u)),block(std::size_t(1)<<d){high=r1::weights({p.begin()+split,p.begin()+d});}
 Fr at(std::size_t i)const{return low[i&(low.size()-1)]*high[i>>split];}
 // Contract contiguous low groups before scaling by their common high weight.
 // No division: exactly valid when challenges are0 or1 as well.
 template<class Read>Fr dot(std::size_t begin,std::size_t start,std::size_t stop,const Read&read,bool factored,bool striped)const{
  Fr result=0;if(block==1)return read(begin);
  if(!factored){for(auto i=start;i<stop;++i)result+=at(i)*read(begin+i);return result;}
  for(auto i=start;i<stop;){auto h=i>>split;auto end=std::min(stop,(h+1)*low.size());Fr partial=0;
   if(striped && end-i>=4){
    Fr p0=0,p1=0,p2=0,p3=0;
    for(;end-i>=4;i+=4){p0+=low[i&(low.size()-1)]*read(begin+i);p1+=low[(i+1)&(low.size()-1)]*read(begin+i+1);p2+=low[(i+2)&(low.size()-1)]*read(begin+i+2);p3+=low[(i+3)&(low.size()-1)]*read(begin+i+3);}
    partial=(p0+p1)+(p2+p3);
   }
   for(;i<end;++i)partial+=low[i&(low.size()-1)]*read(begin+i);
   if(high.size()==1)result+=partial;else result+=high[h]*partial;
  }
  return result;
 }
 std::size_t bytes()const{return high.capacity()*sizeof(Fr);}
};
Fr fixed(const Segment&s,const std::vector<Fr>&p,unsigned begin){Fr w=1;for(unsigned j=begin;j<p.size();++j)w*=((s.offset>>j)&1)?p[j]:Fr(1)-p[j];return w;}
}
unsigned unifiedOptimizationMask(){
 const char* raw=std::getenv("ZKGPT_UNIFIED_OPT");
 if(!raw)return U1|U2|U3|K1|K2|K3|K4|F1;
 const std::string text(raw);
 need(!text.empty() && text.size()<=3 && text.find_first_not_of("0123456789")==std::string::npos,"UnifiedInput optimization mask syntax");
 const auto mask=std::stoul(text);need(mask<=255,"UnifiedInput optimization mask bounds");
 return unsigned(mask);
}
UOptions UOptions::environment(){UOptions out;out.optimization=unifiedOptimizationMask();if(out.optimization&(U2|U3))out.tile_pairs=16384;auto budget=::zkgpt::r7::oneblock::detail::nativeWorkerBudget();out.threads=budget>=16?16:budget>=8?8:budget>=4?4:budget>=2?2:1;if(const auto*v=std::getenv("ZKGPT_U_THREADS")){std::string s(v);need(s=="1" || s=="2" || s=="4" || s=="8" || s=="16" || s=="32","OPT3D thread budget");out.threads=unsigned(std::stoul(s));need(out.threads<=zkgpt::workerThreadCount(),"OPT3D cannot exceed existing worker budget");}if(const auto*v=std::getenv("ZKGPT_PERF3_U_DOT")){std::string s(v);need(s=="0" || s=="1","PERF3 dot mode");out.striped_dot=s=="1";}if(const auto*v=std::getenv("ZKGPT_U_PROFILE_READ")){std::string text(v);need(text=="0" || text=="1","U read profiler switch");out.profile_reads=text=="1";}return out;}
static Round quadraticRoundStreamingImpl(std::size_t n,const std::vector<Segment>&segments,const std::vector<Fr>&prefix,const Factors&provider,const Inputs&inputs,UOptions options,UMetrics*metrics,FoldedU*folded,const std::string&session,bool validated,PublicK*public_k,detail::Workers*shared_workers){
 try{
 if(folded){need(folded->domain()==n && bool(inputs),"PERF4 U domain/storage boundary");folded->beginRound(session,prefix,segments);}
 const bool cached=folded && folded->cached();
 need(!folded || (!options.elide_zero_suffix && !options.striped_dot),"PERF4 U cannot combine experimental scan/dot modes");
 if(!validated){validate(n,segments);}
 need(prefix.size()<32 && (n>>prefix.size())>=2 && provider,"OPT3D prefix/provider");need((options.threads==1 || options.threads==2 || options.threads==4 || options.threads==8 || options.threads==16 || options.threads==32) && options.tile_pairs && options.tile_pairs<=16384,"OPT3D bounded tile/thread options");
 const bool diagnostic=crypto::opt2MetricsEnabled();
 crypto::AllocationTrace allocation;UMetrics m;m.read_profile_enabled=options.profile_reads;std::set<const void*>source_chunks;Round out;const auto t=unsigned(prefix.size());
 // Check public allocation sizes BEFORE allocating any new private tile/pool.
 std::array<bool,33>seen{};std::array<bool,17>splits{};std::size_t planned=options.threads*1024*sizeof(Fr)+segments.size()*192+32768+(2*options.tile_pairs+4)*sizeof(Fr)+(inputs?options.threads-1:0)*((1u<<20)+4096);
 if(!cached)for(auto s:segments){auto d=std::min(t,r1::log2exact(s.padded)),split=std::min(d,16u);if(!seen[d]){planned+=(std::size_t(1)<<(d-split))*sizeof(Fr);seen[d]=true;}if(!splits[split]){planned+=(std::size_t(1)<<split)*sizeof(Fr);splits[split]=true;}}
 const std::size_t budget=(options.threads>4 || options.tile_pairs>4096)?(24ull+(options.threads>16?options.threads-16:0))<<20:8ull<<20;
 need(planned<=budget,"OPT3D extra RAM budget preflight");
 const bool borrow=public_k && inputs && (options.optimization&(U2|U3));
 const bool parallel_products=borrow && (options.optimization&U2);
 const auto worker_count=inputs && (!cached || parallel_products)?options.threads:1;
 need(!shared_workers || shared_workers->count()==options.threads,"UnifiedInput worker budget mismatch");
 detail::Workers local_workers(shared_workers?1:worker_count);
 auto&workers=shared_workers?*shared_workers:local_workers;
 struct alignas(64) WorkerReads { UMetrics metrics; };std::array<WorkerReads,detail::Workers::capacity>reads{};std::vector<Fr>tile(2*options.tile_pairs+4,Fr(0));m.tile_bytes=tile.capacity()*sizeof(Fr);m.thread_bytes=(worker_count-1)*((1u<<20)+4096)+sizeof(detail::Workers)+sizeof(reads)+workers.count()*sizeof(Fr)*8;
 std::map<unsigned,std::vector<Fr>>shared_low;std::map<unsigned,std::unique_ptr<Weights>>weights;std::size_t weight_bytes=0;
 struct Pair{Fr a=0,b=0;};std::map<std::size_t,Pair>collapsed;
 const bool three=options.optimization & U1, terminal=(n>>t)==2;
 struct alignas(64) Product {
  Round sums;
  std::uint64_t pairs=0;
  void add(const Fr&a0,const Fr&a1,const Fr&b0,const Fr&b1,bool three,bool terminal){
   const Fr da=a1-a0,db=b1-b0;
   auto&c=sums.coefficients;
   c[0]+=a0*b0;
   if(three)c[1]+=a1*b1;
   else c[1]+=a0*db+da*b0;
   c[2]+=da*db;
   if(terminal){sums.input_pair[0]+=a0;sums.input_pair[1]+=a1;}
   ++pairs;
  }
 };
 std::array<Product,detail::Workers::capacity> products_by_worker{};
 auto&serial=products_by_worker[0];
 auto product=[&](const Fr&a0,const Fr&a1,const Fr&b0,const Fr&b1){serial.add(a0,a1,b0,b1,three,terminal);};
 for(std::size_t k=0;k<segments.size();++k){const auto s=segments[k];const auto d=r1::log2exact(s.padded),low=std::min(t,d);Weights*wp=nullptr;if(!cached){auto&owned=weights[low];if(!owned){auto weight_start=Clock::now();auto split=std::min(low,16u);auto&shared=shared_low[split];if(shared.empty()){shared=r1::weights({prefix.begin(),prefix.begin()+split});weight_bytes+=shared.capacity()*sizeof(Fr);}owned=std::make_unique<Weights>(prefix,low,shared);weight_bytes+=owned->bytes();m.weights_wall+=elapsed(weight_start);}wp=owned.get();}const auto block=std::size_t(1)<<low;
  InputSpan span;if(inputs){auto original=inputs(k);need(original.padded==s.padded && original.stored<=original.padded && (!original.stored || original.data || original.read),"OPT3D invalid sealed RowMajor span bounds");span=folded?folded->span(k,session,prefix,original):original;need(span.padded==(cached?(s.padded>>low):s.padded),"PERF4 U local span dimension");}
  if(inputs){if(cached)++m.state_hits;else{++m.state_misses;if(span.stored){if(span.storage_identity)source_chunks.insert(span.storage_identity);else m.storage_identity_complete=false;}}}
  const auto length=s.padded>>low;
  auto root_start=Clock::now();auto root_cpu=diagnostic?crypto::processCPU():0;auto before=m;unsigned calls=0;
  auto consume=[&](const Value&a,const Value&b,const Fr*kernel){need(++calls==1 && (inputs || a) && (kernel || b),"OPT3D factor oracle invocation");
   auto kernelRead=[&](std::size_t j){return kernel?kernel[j]:b(j);};
   auto dispatchPairs=[&](std::size_t count,const auto&read){
    const auto pairs=count/2;
    const auto jobs=parallel_products && pairs>=1024?std::min<std::size_t>(workers.count(),pairs/512):1;
    m.product_workers_peak=std::max<std::uint64_t>(m.product_workers_peak,jobs);
    if(jobs>1)++m.parallel_product_tiles;
    workers.run(jobs,[&](std::size_t job){
     read(products_by_worker[job],2*(pairs*job/jobs),2*(pairs*(job+1)/jobs));
    },jobs>1);
   };
   
   for(std::size_t first=0;first<length;){auto count=std::min<std::size_t>(length-first,2*options.tile_pairs);
    if(kernel && (options.optimization&U3) && (cached || block==1) && t<d &&
       !options.observe && first<span.stored){
     // The fused path takes a synchronous borrow and does not use the staging
     // tile. Dispatch a larger contiguous range to amortize worker wakeups.
     constexpr std::size_t fused_tiles_per_dispatch=16;
     const auto limit=parallel_products && (span.data || span.stable_windows)
                      ? 2*options.tile_pairs*fused_tiles_per_dispatch : count;
     auto available=std::min({limit,length-first,span.stored-first});
     InputWindow window{span.data?span.data+first:nullptr,available};
     if(!window.data && span.window){
      window=span.window(first,available);
      need(window.count && window.count<=available,"UnifiedInput source window bounds");
     }
     // An odd boundary consumes at most one staged pair across two windows.
     count=std::max<std::size_t>(2,window.count&~std::size_t(1));
     if(window.data && window.count>=2){
      const auto fused_start=Clock::now();
      const auto* source=window.data;
      const auto* public_values=kernel+first;
      dispatchPairs(count,[&](Product&local,std::size_t lo,std::size_t hi){
       for(auto j=lo;j<hi;j+=2)local.add(source[j],source[j+1],public_values[j],public_values[j+1],three,terminal);
      });
      const auto fused_seconds=elapsed(fused_start);
      m.fused_wall+=fused_seconds;m.product_wall+=fused_seconds;
      m.read_elements+=count;
      if(cached)m.retained_read_elements+=count;else m.original_read_elements+=count;
      m.u_span_elements+=count;
      ++m.scan_calls;first+=count;
      continue;
     }
    }
    // A window can decline a direct borrow. Keep the fallback in the
    // existing staging allocation even if we requested a larger range.
    count=std::min(count,2*options.tile_pairs);
    // When only 1 or 2 contractions remain, split their disjoint low-bit ranges.
    unsigned shards=(!cached && inputs && block>=(1u<<18) && count<options.threads)?options.threads/unsigned(count):1;while(shards>1 && block/shards< wp->low.size())shards/=2;
    const auto tasks=count*shards;need(tasks<=tile.size(),"OPT3D tile task bound");auto scan=Clock::now();auto scan_cpu=diagnostic?crypto::processCPU():0;
    workers.run(tasks,[&](std::size_t job){auto local=job/shards,shard=job%shards;auto begin=(first+local)*block;
     auto lo=block*shard/shards,hi=block*(shard+1)/shards;
     auto&rm=reads[job%options.threads].metrics;++rm.scan_calls;
     std::array<Fr,1024>read_buffer;std::size_t buffer_begin=0,buffer_count=0;
     auto read=[&](std::size_t index)->Fr{
      if(!inputs){return a(index);}
      if(index>=span.stored){return Fr(0);}
      if(span.data){++rm.u_span_elements;++rm.read_elements;++rm.original_read_elements;return span.data[index];}
      if(!options.profile_reads || block<1024){++rm.read_elements;++rm.original_read_elements;++rm.mapping_calls;return span.read(index);}
      if(index<buffer_begin || index-buffer_begin>=buffer_count){
       auto started=Clock::now();auto cpu=threadCPU();buffer_begin=index;buffer_count=std::min({read_buffer.size(),span.stored-index,begin+hi-index});
       for(std::size_t z=0;z<buffer_count;++z)read_buffer[z]=span.read(index+z);
       rm.read_wall+=elapsed(started);rm.read_cpu+=threadCPU()-cpu;rm.read_elements+=buffer_count;rm.original_read_elements+=buffer_count;rm.mapping_calls+=buffer_count;rm.timed_read_elements+=buffer_count;++rm.profile_refills;++rm.profile_misses;
      }else ++rm.profile_hits;
      return read_buffer[index-buffer_begin];
     };
     if(cached){tile[job]=span.data[first+local];++rm.read_elements;++rm.retained_read_elements;++rm.u_span_elements;return;}
     // InputSpan's public stored length defines the source's implicit zero
     // suffix. Intersect before dot (including block==1), never by inspecting
     // witness values. All tiles and folded K queries remain present below.
     if(inputs && options.elide_zero_suffix){
      hi=begin<span.stored?std::min(hi,span.stored-begin):0;
      // The interval establishes index<stored once, so the hot read does not
      // repeat the public suffix branch for every scalar or copy via Value.
      tile[job]=lo<hi?wp->dot(begin,lo,hi,[&](std::size_t index)->const Fr&{return span.data[index];},options.factored,options.striped_dot):Fr(0);
     }else tile[job]=wp->dot(begin,lo,hi,read,options.factored,options.striped_dot);
    },!cached && inputs && (!(options.optimization&(U2|U3)) || span.data) && count*block>=(1u<<18));
    m.tile_writes+=tasks;
    m.scan_wall+=elapsed(scan);if(diagnostic)m.scan_cpu+=crypto::processCPU()-scan_cpu;
    if(shards>1)for(std::size_t j=0;j<count;++j){Fr sum=0;for(unsigned z=0;z<shards;++z)sum+=tile[j*shards+z];tile[j]=sum;}
    if(shards>1){m.additions+=count*shards;m.tile_reads+=tasks;m.tile_writes+=count;}
    if(options.observe)options.observe(k,first,tile.data(),count);
    if(folded && diagnostic){auto capture_start=Clock::now();auto capture_cpu=crypto::processCPU();folded->capture(k,first,tile.data(),count);m.state_capture_wall+=elapsed(capture_start);m.state_capture_cpu+=crypto::processCPU()-capture_cpu;}else if(folded)folded->capture(k,first,tile.data(),count);
    auto products=Clock::now();
    if(t<d){
     need(count%2==0,"OPT3D suffix pair tile alignment");
     dispatchPairs(count,[&](Product&local,std::size_t lo,std::size_t hi){
      for(auto j=lo;j<hi;j+=2)local.add(tile[j],tile[j+1],kernelRead(first+j),kernelRead(first+j+1),three,terminal);
     });
    }else{auto scale=fixed(s,prefix,d);auto&target=collapsed[s.offset>>t];target.a+=scale*tile[0];target.b+=scale*kernelRead(0);}
    m.tile_reads+=count;
    m.product_wall+=elapsed(products);first+=count;
   }
   m.fused_load_bytes+=length*sizeof(Fr);
  };
  if(borrow){
   public_k->withRootSpan(k,session,[&](const Fr*data,std::size_t count){
    need(count==length,"UnifiedInput borrowed K dimension");
    consume({}, {}, data);
   });
   m.k_span_elements+=length;
  }else{
   provider(k,[&](const Value&a,const Value&b){consume(a,b,nullptr);});
   m.k_callback_calls+=length;
  }
  need(calls==1,"OPT3D missing factor oracle");
  m.mapping_calls=0;for(const auto&read_metrics:reads)m.mapping_calls+=read_metrics.metrics.mapping_calls;
  m.coordinates+=cached?length:s.padded;m.bytes+=(inputs?span.stored:s.padded)*sizeof(Fr);
  m.fused_load_bytes+=(inputs?span.stored:s.padded)*sizeof(Fr);
  // U was already root-local in OPT3-C: no Root-Centric map queries here.
  const auto visited=cached?length:(inputs && options.elide_zero_suffix)?span.stored:s.padded;
  m.contracted_coordinates+=visited;m.implicit_zero_coordinates+=cached?0:s.padded-visited;
  if(low && !cached){const auto groups=(visited+wp->low.size()-1)/wp->low.size();m.multiplications+=options.factored?visited+(wp->high.size()==1?0:groups):2*visited;m.additions+=visited+(options.factored?groups:0);}
  if(!cached && options.striped_dot && options.factored && low>=2){const auto groups=visited/wp->low.size();m.striped_dot_groups+=groups;m.additions+=3*groups;}
  if(diagnostic)std::cerr<<"R7_ZERO_SUFFIX_ROOT {\"round\":"<<t<<",\"root\":"<<k<<",\"covered_coordinates\":"<<s.padded<<",\"contracted_coordinates\":"<<(cached?s.padded:visited)<<",\"implicit_zero_coordinates\":"<<(cached?0:s.padded-visited)<<",\"source_payload_read_bytes\":"<<(inputs?span.stored:s.padded)*sizeof(Fr)<<"}\n";
  m.extra_bytes=std::max<std::uint64_t>(m.extra_bytes,planned);need(m.extra_bytes<=budget,"OPT3D extra RAM budget");
  if(diagnostic)std::cerr<<"OPT3D_U_ROOT {\"round\":"<<t<<",\"root\":"<<k<<",\"U_scan_coordinates\":"<<(cached?length:s.padded)<<",\"U_bytes_read\":"<<(inputs?span.stored:s.padded)*sizeof(Fr)<<",\"U_address_mapping_calls\":"<<m.mapping_calls-before.mapping_calls<<",\"U_address_mapping_cpu_seconds\":null,\"U_K_fused_load_bytes\":"<<m.fused_load_bytes-before.fused_load_bytes<<",\"U_Fr_multiplications\":"<<m.multiplications-before.multiplications<<",\"U_Fr_additions\":"<<m.additions-before.additions<<",\"U_scan_cpu_seconds\":"<<m.scan_cpu-before.scan_cpu<<",\"U_scan_wall_seconds\":"<<m.scan_wall-before.scan_wall<<",\"U_K_product_wall_seconds\":"<<m.product_wall-before.product_wall<<",\"root_wall_seconds\":"<<elapsed(root_start)<<",\"root_cpu_seconds\":"<<crypto::processCPU()-root_cpu<<",\"U_TILE_RAM_PEAK_BYTES\":"<<m.tile_bytes<<",\"U_THREAD_LOCAL_BYTES\":"<<m.thread_bytes<<",\"U_TOTAL_EXTRA_RAM_BYTES\":"<<m.extra_bytes<<"}\n";
 }
 for(auto it=collapsed.begin();it!=collapsed.end();){auto pair=it->first>>1;Pair left,right;do{if(it->first&1)right=it->second;else left=it->second;++it;}while(it!=collapsed.end() && (it->first>>1)==pair);product(left.a,right.a,left.b,right.b);}
 for(std::size_t i=1;i<products_by_worker.size();++i){
  const auto&local=products_by_worker[i];
  if(!local.pairs)continue;
  for(std::size_t j=0;j<3;++j)serial.sums.coefficients[j]+=local.sums.coefficients[j];
  if(terminal)for(std::size_t j=0;j<2;++j)serial.sums.input_pair[j]+=local.sums.input_pair[j];
  m.product_additions+=terminal?5:3;
  serial.pairs+=local.pairs;
 }
 out=serial.sums;
 m.product_multiplications=serial.pairs*(three?3:4);
 m.product_additions+=serial.pairs*((three?5:6)+(terminal?2:0));
 if(three){out.coefficients[1]-=out.coefficients[0];out.coefficients[1]-=out.coefficients[2];m.product_additions+=2;}
 if(folded)folded->completeRound(session,prefix);
 m.allocations=allocation.count();m.allocated_bytes=allocation.bytes();for(const auto&local:reads){const auto&r=local.metrics;m.scan_calls+=r.scan_calls;m.timed_read_elements+=r.timed_read_elements;m.read_elements+=r.read_elements;m.profile_refills+=r.profile_refills;m.profile_hits+=r.profile_hits;m.profile_misses+=r.profile_misses;m.original_read_elements+=r.original_read_elements;m.u_span_elements+=r.u_span_elements;m.retained_read_elements+=r.retained_read_elements;m.read_wall+=r.read_wall;m.read_cpu+=r.read_cpu;}
 m.storage_chunks_touched=source_chunks.size();if(metrics){*metrics=m;}
 return out;
 }catch(...){if(folded)folded->poison();throw;}
}
Round quadraticRoundStreaming(std::size_t n,const std::vector<Segment>&segments,const std::vector<Fr>&prefix,const Factors&provider,const Inputs&inputs,UOptions options,UMetrics*metrics,FoldedU*folded,const std::string&session,PublicK*public_k,detail::Workers*workers){
 return quadraticRoundStreamingImpl(n,segments,prefix,provider,inputs,std::move(options),metrics,folded,session,false,public_k,workers);
}
Round detail::ValidatedLayout::streamingRound(const std::vector<Fr>&prefix,const Factors&provider,const Inputs&inputs,UOptions options,UMetrics*metrics,FoldedU*folded,const std::string&session,PublicK*public_k,detail::Workers*workers)const{
 return quadraticRoundStreamingImpl(domain(),segments(),prefix,provider,inputs,std::move(options),metrics,folded,session,true,public_k,workers);
}
std::string scanMetricsJSON(unsigned round,const UMetrics&m){std::ostringstream out;
 out<<"{\"round\":"<<round<<",\"u_scan_call_count\":"<<m.scan_calls<<",\"u_scan_total_elements\":"<<m.read_elements<<",\"u_scan_total_bytes\":"<<m.read_elements*sizeof(Fr)<<",\"original_source_read_elements\":"<<m.original_read_elements<<",\"original_source_scalar_payload_bytes\":"<<m.original_read_elements*sizeof(Fr)<<",\"retained_state_read_elements\":"<<m.retained_read_elements<<",\"retained_state_payload_bytes\":"<<m.retained_read_elements*sizeof(Fr)<<",\"u_scan_wall_time\":"<<m.scan_wall<<",\"u_scan_cpu_time\":"<<m.scan_cpu<<",\"coordinate_mapping_calls\":"<<m.mapping_calls<<",\"original_storage_chunks_touched\":";
 if(m.storage_identity_complete)out<<m.storage_chunks_touched;else out<<"null";
 out<<",\"u_span_elements\":"<<m.u_span_elements<<",\"k_span_elements\":"<<m.k_span_elements<<",\"k_callback_calls\":"<<m.k_callback_calls<<",\"tile_writes\":"<<m.tile_writes<<",\"tile_reads\":"<<m.tile_reads<<",\"fused_wall_seconds\":"<<m.fused_wall<<",\"product_workers_peak\":"<<m.product_workers_peak<<",\"parallel_product_tiles\":"<<m.parallel_product_tiles<<",\"storage_identity_complete\":"<<(m.storage_identity_complete?"true":"false")<<",\"chunk_read_count\":null,\"chunk_read_bytes\":null,\"chunk_cache_hit\":null,\"chunk_cache_miss\":null,\"physical_chunk_metrics\":\"NOT_MEASURED_NO_BULK_CHUNK_API\",\"source_chunk_cache\":\"NOT_APPLICABLE_RESIDENT_SCALAR_READERS\",\"read_profile_enabled\":"<<(m.read_profile_enabled?"true":"false")<<",\"profile_buffer_refill_count\":"<<m.profile_refills<<",\"profile_buffer_payload_bytes\":"<<m.timed_read_elements*sizeof(Fr)<<",\"profile_buffer_hit\":"<<m.profile_hits<<",\"profile_buffer_miss\":"<<m.profile_misses<<",\"timed_original_source_elements\":"<<m.timed_read_elements<<",\"profiled_read_mapping_worker_wall\":"<<m.read_wall<<",\"profiled_read_mapping_worker_cpu\":"<<m.read_cpu<<",\"scan_cpu_minus_profiled_read_cpu_includes_untimed_read_Fr_dispatch\":"<<m.scan_cpu-m.read_cpu<<",\"mle_weights_wall\":"<<m.weights_wall<<",\"u_state_capture_wall\":"<<m.state_capture_wall<<",\"u_state_capture_cpu\":"<<m.state_capture_cpu<<",\"u_state_cache_hit\":"<<m.state_hits<<",\"u_state_cache_miss\":"<<m.state_misses<<",\"U_explicit_file_IO_bytes\":0}";return out.str();}

}
