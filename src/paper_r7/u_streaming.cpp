#include "unified_input.hpp"
#include "opt2_metrics.hpp"
#include "thread_joiner.hpp"
#include <pthread.h>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <map>
#include <mutex>
namespace zkgpt::r7::oneblock::unified {
namespace {
using Clock=std::chrono::steady_clock;
double elapsed(Clock::time_point t){return std::chrono::duration<double>(Clock::now()-t).count();}
// Fixed <=4-worker pool for this round only. Workers never touch PublicK or IO.
class Workers {
 unsigned count_;std::array<pthread_t,3>threads_{};unsigned created_=0;std::mutex mutex_;std::condition_variable wake_,done_;
 struct Context {Workers*self;unsigned id;};std::array<Context,3>contexts_{};
 bool stop_=false;std::size_t generation_=0,pending_=0,tasks_=0;const void*job_context_=nullptr;void(*job_run_)(const void*,std::size_t)=nullptr;
 std::array<std::exception_ptr,4>errors_{};
 void work(unsigned id){try{for(std::size_t j=id;j<tasks_;j+=count_)job_run_(job_context_,j);}catch(...){errors_[id]=std::current_exception();}}
 public:explicit Workers(unsigned count):count_(count){
  pthread_attr_t attr;need(pthread_attr_init(&attr)==0,"OPT3D thread attributes");
  if(pthread_attr_setstacksize(&attr,1u<<20)!=0){pthread_attr_destroy(&attr);need(false,"OPT3D bounded thread stack");}
  try{for(unsigned id=1;id<count_;++id){contexts_[id-1]={this,id};auto error=pthread_create(&threads_[id-1],&attr,[](void*arg)->void*{auto context=*static_cast<Context*>(arg);context.self->loop(context.id);return nullptr;},&contexts_[id-1]);need(error==0,"OPT3D worker creation");++created_;}pthread_attr_destroy(&attr);}
  catch(...){pthread_attr_destroy(&attr);shutdown();throw;}
 }
 void loop(unsigned id){std::size_t seen=0;for(;;){std::unique_lock<std::mutex>lock(mutex_);wake_.wait(lock,[&]{return stop_ || generation_!=seen;});if(stop_)return;seen=generation_;lock.unlock();work(id);lock.lock();if(--pending_==0)done_.notify_one();}}
 void shutdown(){ {std::lock_guard<std::mutex>lock(mutex_);stop_=true;}wake_.notify_all();for(unsigned id=0;id<created_;++id)pthread_join(threads_[id],nullptr);created_=0;}
 ~Workers(){shutdown();}
 template<class F>void run(std::size_t count,const F&fn,bool parallel){
  if(!parallel || count_==1){for(std::size_t j=0;j<count;++j)fn(j);return;}
  // Borrow the caller's live closure until this synchronous join completes.
  // No std::function heap allocation/copy for each tile dispatch.
  {std::lock_guard<std::mutex>lock(mutex_);job_context_=&fn;job_run_=[](const void*p,std::size_t j){(*static_cast<const F*>(p))(j);};tasks_=count;pending_=count_-1;errors_={};++generation_;}wake_.notify_all();work(0);
  {std::unique_lock<std::mutex>lock(mutex_);done_.wait(lock,[&]{return pending_==0;});job_context_=nullptr;job_run_=nullptr;}
  for(auto&e:errors_)if(e)std::rethrow_exception(e);
 }
};
struct Weights {
 const std::vector<Fr>&low;std::vector<Fr>high;unsigned split;std::size_t block;
 Weights(const std::vector<Fr>&p,unsigned d,const std::vector<Fr>&shared):low(shared),split(std::min(d,16u)),block(std::size_t(1)<<d){high=r1::weights({p.begin()+split,p.begin()+d});}
 Fr at(std::size_t i)const{return low[i&(low.size()-1)]*high[i>>split];}
 // Contract contiguous low groups before scaling by their common high weight.
 // No division: exactly valid when challenges are0 or1 as well.
 template<class Read>Fr dot(std::size_t begin,std::size_t start,std::size_t stop,const Read&read,bool factored)const{
  Fr result=0;if(block==1)return read(begin);
  if(!factored){for(auto i=start;i<stop;++i)result+=at(i)*read(begin+i);return result;}
  for(auto i=start;i<stop;){auto h=i>>split;auto end=std::min(stop,(h+1)*low.size());Fr partial=0;for(;i<end;++i)partial+=low[i&(low.size()-1)]*read(begin+i);if(high.size()==1)result+=partial;else result+=high[h]*partial;}
  return result;
 }
 std::size_t bytes()const{return high.capacity()*sizeof(Fr);}
};
Fr fixed(const Segment&s,const std::vector<Fr>&p,unsigned begin){Fr w=1;for(unsigned j=begin;j<p.size();++j)w*=((s.offset>>j)&1)?p[j]:Fr(1)-p[j];return w;}
}
UOptions UOptions::environment(){UOptions out;auto budget=zkgpt::workerThreadCount();out.threads=budget>=4?4:budget>=2?2:1;if(const auto*v=std::getenv("ZKGPT_U_THREADS")){std::string s(v);need(s=="1" || s=="2" || s=="4","OPT3D thread budget");out.threads=unsigned(s[0]-'0');need(out.threads<=budget,"OPT3D cannot exceed existing worker budget");}return out;}
Round quadraticRoundStreaming(std::size_t n,const std::vector<Segment>&segments,const std::vector<Fr>&prefix,const Factors&provider,const Inputs&inputs,UOptions options,UMetrics*metrics){
 validate(n,segments);need(prefix.size()<32 && (n>>prefix.size())>=2 && provider,"OPT3D prefix/provider");need((options.threads==1 || options.threads==2 || options.threads==4) && options.tile_pairs && options.tile_pairs<=4096,"OPT3D bounded tile/thread options");
 crypto::AllocationTrace allocation;UMetrics m;Round out;const auto t=unsigned(prefix.size());
 // Check public allocation sizes BEFORE allocating any new private tile/pool.
 std::array<bool,33>seen{};std::array<bool,17>splits{};std::size_t planned=segments.size()*192+16384+(2*options.tile_pairs+4)*sizeof(Fr)+(inputs?options.threads-1:0)*((1u<<20)+4096);
 for(auto s:segments){auto d=std::min(t,r1::log2exact(s.padded)),split=std::min(d,16u);if(!seen[d]){planned+=(std::size_t(1)<<(d-split))*sizeof(Fr);seen[d]=true;}if(!splits[split]){planned+=(std::size_t(1)<<split)*sizeof(Fr);splits[split]=true;}}
 need(planned<=8ull<<20,"OPT3D extra RAM budget preflight");
 Workers workers(inputs?options.threads:1);std::vector<Fr>tile(2*options.tile_pairs+4,Fr(0));m.tile_bytes=tile.capacity()*sizeof(Fr);m.thread_bytes=(inputs?options.threads-1:0)*((1u<<20)+4096)+sizeof(Workers)+4*sizeof(Fr)*8;
 std::map<unsigned,std::vector<Fr>>shared_low;std::map<unsigned,std::unique_ptr<Weights>>weights;std::size_t weight_bytes=0;
 struct Pair{Fr a=0,b=0;};std::map<std::size_t,Pair>collapsed;
 auto product=[&](const Fr&a0,const Fr&a1,const Fr&b0,const Fr&b1){auto da=a1-a0,db=b1-b0;out.coefficients[0]+=a0*b0;out.coefficients[1]+=a0*db+da*b0;out.coefficients[2]+=da*db;m.product_multiplications+=4;m.product_additions+=6;if((n>>t)==2){out.input_pair[0]+=a0;out.input_pair[1]+=a1;out.kernel_pair[0]+=b0;out.kernel_pair[1]+=b1;m.product_additions+=4;}};
 for(std::size_t k=0;k<segments.size();++k){const auto s=segments[k];const auto d=r1::log2exact(s.padded),low=std::min(t,d);auto&owned=weights[low];if(!owned){auto split=std::min(low,16u);auto&shared=shared_low[split];if(shared.empty()){shared=r1::weights({prefix.begin(),prefix.begin()+split});weight_bytes+=shared.capacity()*sizeof(Fr);}owned=std::make_unique<Weights>(prefix,low,shared);weight_bytes+=owned->bytes();}const auto&w=*owned;const auto block=w.block;
  InputSpan span;if(inputs){span=inputs(k);need(span.padded==s.padded && span.stored<=span.padded && (!span.stored || span.data),"OPT3D invalid sealed RowMajor span bounds");}
  auto root_start=Clock::now();auto root_cpu=crypto::processCPU();auto before=m;unsigned calls=0;
  provider(k,[&](const Value&a,const Value&b){need(++calls==1 && a && b,"OPT3D factor oracle invocation");
   auto read=[&](std::size_t index)->Fr{return inputs?(index<span.stored?span.data[index]:Fr(0)):a(index);};
   const auto length=s.padded>>low;
   for(std::size_t first=0;first<length;){auto count=std::min<std::size_t>(length-first,2*options.tile_pairs);
    // When only1/2 contractions remain, split their disjoint low-bit ranges.
    unsigned shards=(inputs && block>=(1u<<18) && count<options.threads)?options.threads/unsigned(count):1;while(shards>1 && block/shards< w.low.size())shards/=2;
    const auto tasks=count*shards;need(tasks<=tile.size(),"OPT3D tile task bound");auto scan=Clock::now();auto scan_cpu=crypto::processCPU();
    workers.run(tasks,[&](std::size_t job){auto local=job/shards,shard=job%shards;auto begin=(first+local)*block;tile[job]=w.dot(begin,block*shard/shards,block*(shard+1)/shards,read,options.factored);},inputs && count*block>=(1u<<18));
    m.scan_wall+=elapsed(scan);m.scan_cpu+=crypto::processCPU()-scan_cpu;
    if(shards>1)for(std::size_t j=0;j<count;++j){Fr sum=0;for(unsigned z=0;z<shards;++z)sum+=tile[j*shards+z];tile[j]=sum;}
    if(shards>1)m.additions+=count*shards;
    if(options.observe)options.observe(k,first,tile.data(),count);
    auto products=Clock::now();
    if(t<d){need(count%2==0,"OPT3D suffix pair tile alignment");for(std::size_t j=0;j<count;j+=2){auto b0=b(first+j),b1=b(first+j+1);product(tile[j],tile[j+1],b0,b1);}}
    else{auto scale=fixed(s,prefix,d);auto&target=collapsed[s.offset>>t];target.a+=scale*tile[0];target.b+=scale*b(0);}
    m.product_wall+=elapsed(products);first+=count;
   }
   m.fused_load_bytes+=length*sizeof(Fr);
  });need(calls==1,"OPT3D missing factor oracle");
  m.coordinates+=s.padded;m.bytes+=(inputs?span.stored:s.padded)*sizeof(Fr);
  m.fused_load_bytes+=(inputs?span.stored:s.padded)*sizeof(Fr);
  // U was already root-local in OPT3-C: no Root-Centric map queries here.
  if(low){m.multiplications+=options.factored?s.padded+(w.high.size()==1?0:(s.padded/w.low.size())):2*s.padded;m.additions+=s.padded+(options.factored?s.padded/w.low.size():0);}
  m.extra_bytes=std::max<std::uint64_t>(m.extra_bytes,planned);need(m.extra_bytes<=8ull<<20,"OPT3D extra RAM budget");
  if(crypto::opt2MetricsEnabled())std::cerr<<"OPT3D_U_ROOT {\"round\":"<<t<<",\"root\":"<<k<<",\"U_scan_coordinates\":"<<s.padded<<",\"U_bytes_read\":"<<(inputs?span.stored:s.padded)*sizeof(Fr)<<",\"U_address_mapping_calls\":"<<m.mapping_calls-before.mapping_calls<<",\"U_address_mapping_cpu_seconds\":0,\"U_K_fused_load_bytes\":"<<m.fused_load_bytes-before.fused_load_bytes<<",\"U_Fr_multiplications\":"<<m.multiplications-before.multiplications<<",\"U_Fr_additions\":"<<m.additions-before.additions<<",\"U_scan_cpu_seconds\":"<<m.scan_cpu-before.scan_cpu<<",\"U_scan_wall_seconds\":"<<m.scan_wall-before.scan_wall<<",\"U_K_product_wall_seconds\":"<<m.product_wall-before.product_wall<<",\"root_wall_seconds\":"<<elapsed(root_start)<<",\"root_cpu_seconds\":"<<crypto::processCPU()-root_cpu<<",\"U_TILE_RAM_PEAK_BYTES\":"<<m.tile_bytes<<",\"U_THREAD_LOCAL_BYTES\":"<<m.thread_bytes<<",\"U_TOTAL_EXTRA_RAM_BYTES\":"<<m.extra_bytes<<"}\n";
 }
 for(auto it=collapsed.begin();it!=collapsed.end();){auto pair=it->first>>1;Pair left,right;do{if(it->first&1)right=it->second;else left=it->second;++it;}while(it!=collapsed.end() && (it->first>>1)==pair);product(left.a,right.a,left.b,right.b);}
 m.allocations=allocation.count();m.allocated_bytes=allocation.bytes();if(metrics)*metrics=m;return out;
}
}
