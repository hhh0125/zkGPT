#include "paper_r7_classified_commitments.hpp"
#include "paper_r7_public_k.hpp"
#include "paper_r7_unified_workers.hpp"
#include "paper_r7_category_tiles.hpp"
#include "paper_r7_k_terminal.hpp"
#include "paper_r7_carry_terminal.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <openssl/evp.h>
namespace zkgpt::r7::oneblock::classified {
std::vector<CategoryTile>categoryTiles(const Layout&l){
 std::vector<CategoryTile>out;
 for(std::size_t k=0;k<l.roots.size();++k){const auto&r=l.roots[k];need(r.category<5 && r.category_offset<=l.categories[r.category].actual && r.actual<=l.categories[r.category].actual-r.category_offset,"category root bounds");
  auto logical=l.categories[r.category].logical_offset+r.category_offset;std::size_t begin=0;
  while(begin<r.actual){std::size_t n=1;while(n<(1u<<20) && n<=(r.actual-begin)/2 && (logical+begin)%(2*n)==0)n*=2;out.push_back({{logical+begin,n},k,begin});begin+=n;}
 }
 std::sort(out.begin(),out.end(),[](const CategoryTile&a,const CategoryTile&b){return a.segment.offset<b.segment.offset;});return out;
}
void Commitments::categoryKernelInto(std::size_t root,std::size_t begin,std::size_t count,const std::vector<Fr>&rho,const Authentication&auth,Fr*out)const{
 need(!category_failed_ && auth.session==session_ && auth.transcript.lock()==io_.transcriptOwner() && io_.session()==session_ && auth.claims.size()==claims_.size() && rho.size()==claims_.size(),"category K prepared session/dimensions");
 const auto kernel_started=std::chrono::steady_clock::now();
 need(root<roots_.size() && begin<=roots_[root].spec.actual && count<=roots_[root].spec.actual-begin && count<=(1u<<20),"category bounded K tile");
 ++auth_metrics_.kernel_calls;auth_metrics_.kernel_peak_bytes=std::max<std::uint64_t>(auth_metrics_.kernel_peak_bytes,count*sizeof(Fr));need(out || !count,"category K missing output buffer");if(count)std::fill(out,out+count,Fr(0));
 for(std::size_t i=0;i<claims_.size();++i){const auto&c=claims_[i];const auto&p=auth.claims[i];need(p.ready && p.plan().frozen,"category K requires frozen mapping");if(c.only_root>=0 && std::size_t(c.only_root)!=root)continue;if(!p.plan().hasRoot(root))continue;
  std::unique_ptr<Weights>scratch;if(!p.weights)scratch=std::make_unique<Weights>(c.point,&auth_metrics_);const auto&w=p.weights?*p.weights:*scratch;
  p.plan().forRoot(root,auth.store,[&](const mapping::Run&r){
   ++auth_metrics_.kernel_run_visits;++auth_metrics_.kernel_run_clips;
   const auto actual=roots_[root].spec.actual;
   need(r.count && r.logical_step && r.begin<c.padded && r.count<=1+(c.padded-1-r.begin)/r.logical_step && r.first.root==root && r.first.index<actual && r.stride>=-std::int64_t(actual) && r.stride<=std::int64_t(actual),"category K frozen run bounds");
   auto last=std::int64_t(r.first.index)+std::int64_t(r.count-1)*r.stride;need(last>=0 && std::uint64_t(last)<actual,"category K frozen run end");
   const std::int64_t first=r.first.index,lo=begin,hi=begin+count;std::size_t a=0,b=r.count;
   if(r.stride>0){auto d=r.stride;if(lo>first)a=std::min(r.count,std::size_t((lo-first+d-1)/d));b=hi>first?std::min(r.count,std::size_t((hi-1-first)/d+1)):0;}
   else if(r.stride<0){auto d=-r.stride;if(first>=hi)a=std::min(r.count,std::size_t((first-hi)/d+1));b=first>=lo?std::min(r.count,std::size_t((first-lo)/d+1)):0;}
   else if(first<lo || first>=hi)return;
   if(a>=b)return;
   ++auth_metrics_.kernel_run_intersections;
   auto at=first+std::int64_t(a)*r.stride;w.scaled(r.begin+a*r.logical_step,b-a,rho[i],[&](const Fr&value){need(at>=lo && at<hi,"category K clipped map");out[std::size_t(at-lo)]+=value;at+=r.stride;},r.logical_step);auth_metrics_.kernel_coordinates+=b-a;
  });

 }
 auth_metrics_.kernel_wall+=std::chrono::duration<double>(std::chrono::steady_clock::now()-kernel_started).count();
}
namespace {
using CategoryClock=std::chrono::steady_clock;
constexpr unsigned category_opts=unified::K1|unified::K2|unified::K3|unified::K4;
constexpr std::size_t category_weight_limit=32u<<20;
double categorySeconds(CategoryClock::time_point start){
 return std::chrono::duration<double>(CategoryClock::now()-start).count();
}
struct Clip {std::size_t first,last;};
std::size_t validateRun(const mapping::Run&r,std::size_t root,std::size_t actual,std::size_t padded){
 need(r.count && r.logical_step && r.begin<padded && r.count<=1+(padded-1-r.begin)/r.logical_step
      && r.first.root==root && r.first.index<actual && r.stride>=-std::int64_t(actual)
      && r.stride<=std::int64_t(actual),"category K frozen run bounds");
 const auto first=std::size_t(r.first.index);
 if(r.stride>0)need(r.count-1<=(actual-1-first)/std::size_t(r.stride),"category K frozen run end");
 if(r.stride<0)need(r.count-1<=first/std::size_t(-r.stride),"category K frozen run end");
 return std::size_t(std::int64_t(first)+std::int64_t(r.count-1)*r.stride);
}
Clip clipRun(const mapping::Run&r,std::size_t lo,std::size_t hi){
 const auto first=std::size_t(r.first.index);
 if(r.stride>0){
  const auto d=std::size_t(r.stride);
  return {lo>first?std::min(r.count,1+(lo-1-first)/d):0,hi>first?std::min(r.count,1+(hi-1-first)/d):0};
 }
 if(r.stride<0){
  const auto d=std::size_t(-r.stride);
  return {first>=hi?std::min(r.count,1+(first-hi)/d):0,first>=lo?std::min(r.count,1+(first-lo)/d):0};
 }
 return {0,lo<=first && first<hi?r.count:0};
}
struct KernelTasks {
 std::uint64_t visits=0,clips=0,intersections=0,coordinates=0,weight_rebuilds=0,weight_cache_uses=0;
 std::uint64_t products=0,reference_products=0,cached_groups=0,fast_blocks=0,fast_coordinates=0;
 std::array<std::uint64_t,4>strides{};
 std::array<std::uint64_t,2>logical_steps{};
 mapping::ReadMetrics reads;
 void merge(const KernelTasks&m){
  visits+=m.visits;clips+=m.clips;intersections+=m.intersections;coordinates+=m.coordinates;
  weight_rebuilds+=m.weight_rebuilds;weight_cache_uses+=m.weight_cache_uses;products+=m.products;
  reference_products+=m.reference_products;cached_groups+=m.cached_groups;fast_blocks+=m.fast_blocks;
  fast_coordinates+=m.fast_coordinates;
  for(std::size_t k=0;k<strides.size();++k)strides[k]+=m.strides[k];
  for(std::size_t k=0;k<logical_steps.size();++k)logical_steps[k]+=m.logical_steps[k];
  reads.bytes+=m.reads.bytes;reads.blocks+=m.reads.blocks;
 }
};
struct alignas(64) KernelLocal {KernelTasks stats;};
}

// Keeps at most one root's clipped schedule. Each bucket owns an output
// interval, and each entry is a slice of a validated immutable frozen run.
class Commitments::CategoryKernelPlan {
 // Keep the compact representation; report unsupported coordinates explicitly.
 struct Entry {
  std::uint32_t begin,count,index,step;
  std::int32_t stride;
 };
 static_assert(sizeof(Entry)==20,"category K compact entry size");
 static constexpr std::size_t entries_per_page=256;
 using EntryPage=std::array<Entry,entries_per_page>;
 using PagePtr=std::unique_ptr<EntryPage>;
 // The construction scan visits each mapping group exactly once per root.
 // Record its exclusive end in each bucket once, without a group ID per slice.
 struct GroupSpan {std::size_t end;std::uint32_t group;};
 struct Bucket {
  std::size_t lo,hi,tile;
  std::vector<PagePtr>pages;
  std::vector<GroupSpan>groups;
  std::size_t count=0;
 };
 struct Range {std::size_t first=0,count=0;};
 struct WeightCache {std::unique_ptr<Weights>weights;std::vector<Fr>aggregate;};
 const Commitments&c_;
 const std::vector<CategoryTile>&tiles_;
 const std::vector<Fr>&rho_;
 const Authentication&auth_;
 const unsigned options_;
 unified::detail::Workers*workers_;
 std::vector<Range>roots_,buckets_by_tile_;
 std::vector<Bucket>buckets_;
 std::vector<WeightCache>weights_;
 // Each spatial run belongs to one immutable mapping. Claim coefficients and
 // MLE weights are still applied independently for every member.
 std::vector<std::vector<std::size_t>>mapping_groups_;

 std::size_t active_root_=std::size_t(-1),index_bytes_=0,index_peak_=0,weight_bytes_=0;
 bool indexed_=false;
 std::uint64_t original_runs_=0,indexed_runs_=0,index_clips_=0,index_intersections_=0,index_entries_=0,index_fallbacks_=0;
 std::uint64_t noncompact_roots_=0,weight_rebuilds_=0,aggregate_mul_=0,parallel_tiles_=0,worker_peak_=1;
 std::uint64_t tile_slices_=0,extra_worker_slices_=0,extra_tile_slices_=0,empty_buckets_skipped_=0;
 std::uint64_t noncontiguous_runs_=0,shared_group_runs_=0,shared_group_entries_=0,group_span_bytes_=0;
 double index_seconds_=0,weight_seconds_=0,parallel_seconds_=0,worker_load_ratio_peak_=0;
 KernelTasks computed_;
 mapping::ReadMetrics index_reads_;
 unsigned parts(std::size_t count)const{
  if(!(options_&unified::K2) || !workers_)return 1;
  return std::min(workers_->count(),std::max(1u,unsigned(count/(1u<<15))));
 }
 bool onRoot(std::size_t i,std::size_t root)const{
  const auto&c=c_.claims_[i];const auto&p=auth_.claims[i];
  return (c.only_root<0 || std::size_t(c.only_root)==root) && p.plan().hasRoot(root);
 }
 void clearIndex(){std::vector<Bucket>().swap(buckets_);index_bytes_=roots_.capacity()*sizeof(Range)+buckets_by_tile_.capacity()*sizeof(Range);indexed_=false;}
 void append(Bucket&bucket,const Entry&e,std::uint32_t group){
  auto&groups=bucket.groups;
  if(groups.empty() || groups.back().group!=group){
   const auto old=groups.capacity();
   groups.push_back({0,group});
   if(groups.capacity()!=old){
    index_peak_=std::max(index_peak_,index_bytes_+groups.capacity()*sizeof(GroupSpan));
    const auto growth=(groups.capacity()-old)*sizeof(GroupSpan);
    index_bytes_+=growth;group_span_bytes_+=growth;
   }
  }
  groups.back().end=bucket.count+1;
  auto&pages=bucket.pages;
  const auto slot=bucket.count%entries_per_page;
  if(!slot){
   if(pages.size()==pages.capacity()){
    const auto next=std::max<std::size_t>(4,2*pages.capacity());
    // Reallocation briefly retains both containers of page pointers. Entry
    // pages stay in place and are charged only once.
    const auto old=pages.capacity();pages.reserve(next);
    index_peak_=std::max(index_peak_,index_bytes_+pages.capacity()*sizeof(PagePtr));
    index_bytes_+=(pages.capacity()-old)*sizeof(PagePtr);
   }
   pages.emplace_back(new EntryPage);
   index_bytes_+=sizeof(EntryPage);
   index_peak_=std::max(index_peak_,index_bytes_);
  }
  (*pages.back())[slot]=e;++bucket.count;++index_entries_;
 }
 void prepareWeights(std::size_t root){
  if(!(options_&unified::K3))return;
  const auto started=CategoryClock::now();
  for(std::size_t i=0;i<weights_.size();++i){
   if(!onRoot(i,root))continue;
   auto&cache=weights_[i];const auto&p=auth_.claims[i];const auto&point=c_.claims_[i].point;
   const auto half=point.size()/2;
   const auto high=std::size_t(1)<<(point.size()-half);
   const auto required=sizeof(Weights)+sizeof(Fr)*((std::size_t(1)<<half)+high);
   if(!p.weights && !cache.weights && required<=category_weight_limit-weight_bytes_){
    auto candidate=std::make_unique<Weights>(point);++weight_rebuilds_;
    const auto bytes=sizeof(Weights)+candidate->bytes();
    if(bytes<=category_weight_limit-weight_bytes_){weight_bytes_+=bytes;cache.weights=std::move(candidate);}
   }
   const auto*w=p.weights?p.weights.get():cache.weights.get();
   // Long, nearly contiguous claims already reuse each high factor within a
   // group. Admit aggregation when fragmented runs justify precomputation.
   if(w && cache.aggregate.empty() && p.plan().run_count>=2*w->highSize()
      && w->highSize()*sizeof(Fr)<=category_weight_limit-weight_bytes_){
    auto scaled=w->aggregate(rho_[i]);aggregate_mul_+=scaled.size();
    const auto bytes=scaled.capacity()*sizeof(Fr);
    if(bytes<=category_weight_limit-weight_bytes_){weight_bytes_+=bytes;cache.aggregate=std::move(scaled);}
   }
  }
  weight_seconds_+=categorySeconds(started);
 }
 void indexRoot(std::size_t root){
  if(active_root_==root)return;
  active_root_=root;clearIndex();prepareWeights(root);
  if(!(options_&unified::K1))return;
  const auto started=CategoryClock::now();
  const auto range=roots_.at(root);
  std::size_t parts_total=0;
  for(std::size_t k=range.first;k<range.first+range.count;++k)parts_total+=parts(tiles_[k].segment.padded);
  buckets_.reserve(parts_total);index_bytes_+=buckets_.capacity()*sizeof(Bucket);
  index_peak_=std::max(index_peak_,index_bytes_);
  for(std::size_t k=range.first;k<range.first+range.count;++k){
   const auto&t=tiles_[k];const auto count=t.segment.padded;const auto n=parts(count);
   buckets_by_tile_[k]={buckets_.size(),n};
   for(unsigned j=0;j<n;++j)buckets_.push_back({t.begin+count*j/n,t.begin+count*(j+1)/n,k,{},{}});
  }
  const auto actual=c_.roots_[root].spec.actual;
  const auto entries_before=index_entries_,runs_before=indexed_runs_;
  const auto root_base=index_bytes_;
  const auto skipped_before=empty_buckets_skipped_,tiles_before=tile_slices_;
  const auto spans_before=group_span_bytes_;
  for(std::size_t g=0;g<mapping_groups_.size();++g){
   const auto i=mapping_groups_[g].front();
   if(!onRoot(i,root))continue;
   const auto&p=auth_.claims[i];const auto padded=c_.claims_[i].padded;
   p.plan().forRoot(root,auth_.store,[&](const mapping::Run&r){
    ++indexed_runs_;
    const auto last=validateRun(r,root,actual,padded);
    const auto first=std::size_t(r.first.index),minimum=std::min(first,last),maximum=std::max(first,last);
    const auto past=[](const Bucket&bucket,std::size_t at){return bucket.hi<=at;};
    auto b=std::lower_bound(buckets_.begin(),buckets_.end(),minimum,past);
    const auto shared=mapping_groups_[g].size()>1;
    shared_group_runs_+=shared;
    noncontiguous_runs_+=(r.count>1 && (r.logical_step!=1 || r.stride>1 || r.stride<-1));
    std::size_t run_tiles=0,run_entries=0,last_tile=std::size_t(-1);
    while(b!=buckets_.end() && b->lo<=maximum){
     ++index_clips_;
     const auto slice=clipRun(r,b->lo,b->hi);
     if(slice.first<slice.last){
      ++index_intersections_;++run_entries;
      if(last_tile!=b->tile){++run_tiles;last_tile=b->tile;}
      const auto begin=r.begin+slice.first*r.logical_step,count=slice.last-slice.first;
      constexpr auto u32=std::numeric_limits<std::uint32_t>::max();
      if(begin>u32 || count>u32 || r.logical_step>u32 || g>u32
         || r.stride<std::numeric_limits<std::int32_t>::min() || r.stride>std::numeric_limits<std::int32_t>::max()){
       throw std::runtime_error("category K noncompact index representation required: root="+std::to_string(root)+" claim="+std::to_string(i)+" begin="+std::to_string(begin)+" count="+std::to_string(count)+" step="+std::to_string(r.logical_step)+" stride="+std::to_string(r.stride));
      }
      const auto index=std::uint32_t(std::int64_t(r.first.index)+std::int64_t(slice.first)*r.stride);
      append(*b,{std::uint32_t(begin),std::uint32_t(count),index,std::uint32_t(r.logical_step),
                 std::int32_t(r.stride)},std::uint32_t(g));
     }
     // The next physical coordinate is also the next possible intersection.
     // Unit strides reach the adjacent bucket and need no extra search.
     if(maximum<b->hi)break;
     auto next=b+1;
     if(next!=buckets_.end() && (r.stride>1 || r.stride<-1)){
      const auto ordinal=r.stride>0?slice.last:slice.first-1;
      const auto coordinate=std::size_t(std::int64_t(first)+std::int64_t(ordinal)*r.stride);
      if(next->hi<=coordinate){
       const auto found=std::lower_bound(next,buckets_.end(),coordinate,past);
       empty_buckets_skipped_+=found-next;
       next=found;
      }
     }
     b=next;
    }
    tile_slices_+=run_tiles;
    extra_worker_slices_+=run_entries-run_tiles;
    if(run_tiles)extra_tile_slices_+=run_tiles-1;
    if(shared)shared_group_entries_+=run_entries;
   },&index_reads_);
  }
  indexed_=true;
  const auto elapsed=categorySeconds(started);index_seconds_+=elapsed;
  std::cerr<<"CATEGORY_K_ROOT_INDEX {\"root\":"<<root<<",\"entries\":"<<(index_entries_-entries_before)
   <<",\"runs\":"<<(indexed_runs_-runs_before)<<",\"bytes\":"<<index_bytes_
   <<",\"worker_buckets\":"<<buckets_.size()<<",\"metadata_and_buckets_bytes\":"<<root_base<<",\"build_seconds\":"<<elapsed
   <<",\"tile_slices\":"<<(tile_slices_-tiles_before)<<",\"group_span_bytes\":"<<(group_span_bytes_-spans_before)
   <<",\"empty_worker_buckets_skipped\":"<<(empty_buckets_skipped_-skipped_before)<<",\"budget_fallback\":false}\n";
 }
 void apply(const mapping::Run&r,std::size_t claim,const Weights&w,const std::vector<Fr>*aggregate,
            std::size_t lo,Fr*out,KernelTasks&m)const{
  const auto stride=r.stride;
  const auto begin=r.begin,count=r.count,step=r.logical_step;
  const auto first=r.first.index-lo;
  m.coordinates+=count;m.strides[stride==0?0:stride==1?1:stride==-1?2:3]+=count;
  const bool power_step=(step&(step-1))==0;
  m.logical_steps[power_step?0:1]+=count;
  m.reference_products+=count+w.highGroups(begin,count,step);
  auto scale=[&](std::size_t start,std::size_t n,const auto&consume){
   const auto groups=w.scaled(start,n,rho_[claim],consume,step,aggregate);
   m.products+=n+(aggregate?0:groups);if(aggregate)m.cached_groups+=groups;
  };
  if(!(options_&unified::K4)){
   auto at=std::int64_t(first);scale(begin,count,[&](const Fr&v){out[std::size_t(at)]+=v;at+=stride;});return;
  }
  if(stride==1){
   Fr*dest=out+first;scale(begin,count,[&](const Fr&v){*dest+=v;++dest;});return;
  }
  if(stride==-1){
   auto at=first;scale(begin,count,[&](const Fr&v){out[at--]+=v;});return;
  }
  if(stride!=0){
   auto at=std::int64_t(first);scale(begin,count,[&](const Fr&v){out[std::size_t(at)]+=v;at+=stride;});return;
  }
  auto&dest=out[first];
  auto add=[&](const Fr&v){dest+=v;};
  if(!power_step || count<64){scale(begin,count,add);return;}
  const auto&point=c_.claims_[claim].point;
  const auto shift=unsigned(__builtin_ctzll(step));
  std::size_t j=begin,remaining=count,pending=0;
  while(remaining){
   std::size_t n=1;unsigned bits=0;
   while(n<=remaining/2 && !((j>>shift)&n)){n*=2;++bits;}
   if(n<64){pending+=n;}
   else{
    if(pending){scale(j-pending*step,pending,add);pending=0;}
    Fr term=rho_[claim];
    // Free bits sum to one, including when a challenge is zero or one.
    for(unsigned bit=0;bit<point.size();++bit)
     if(bit<shift || bit>=shift+bits){term*=((j>>bit)&1)?point[bit]:Fr(1)-point[bit];++m.products;}
    dest+=term;++m.fast_blocks;m.fast_coordinates+=n;
   }
   j+=n*step;remaining-=n;
  }
  if(pending)scale(j-pending*step,pending,add);
 }
 void compute(std::size_t root,std::size_t lo,std::size_t hi,Fr*out,
              const Bucket*bucket,KernelTasks&m)const{
  std::fill_n(out,hi-lo,Fr(0));
  std::size_t active_claim=std::size_t(-1);std::unique_ptr<Weights>scratch;
  const Weights*w=nullptr;const std::vector<Fr>*aggregate=nullptr;
  auto consume=[&](const mapping::Run&r,std::size_t i){
   if(active_claim!=i){
    active_claim=i;const auto&p=auth_.claims[i];const auto*cache=weights_.empty()?nullptr:&weights_[i];
    w=p.weights.get();if(!w && cache)w=cache->weights.get();
    if(w){scratch.reset();++m.weight_cache_uses;}
    else{scratch=std::make_unique<Weights>(c_.claims_[i].point);w=scratch.get();++m.weight_rebuilds;}
    aggregate=cache && !cache->aggregate.empty()?&cache->aggregate:nullptr;
   }
   apply(r,i,*w,aggregate,lo,out,m);
  };
  if(bucket){
   struct ClaimWeights {const Weights*source;const std::vector<Fr>*aggregate;std::unique_ptr<Weights>scratch;};
   std::vector<ClaimWeights>group_weights;
   std::size_t pos=0;
   for(const auto&span:bucket->groups){
    const auto&group=mapping_groups_[span.group];
    if(group.size()>1){
     group_weights.clear();group_weights.reserve(group.size());
     for(auto i:group){
      const auto&p=auth_.claims[i];const auto*cache=weights_.empty()?nullptr:&weights_[i];
      const auto*source=p.weights.get();if(!source && cache)source=cache->weights.get();
      if(source){++m.weight_cache_uses;group_weights.push_back({source,cache && !cache->aggregate.empty()?&cache->aggregate:nullptr,{}});}
      else{
       auto local=std::make_unique<Weights>(c_.claims_[i].point);
       ++m.weight_rebuilds;group_weights.push_back({local.get(),nullptr,std::move(local)});
      }
     }
    }
    for(;pos<span.end;++pos){
     const auto&e=(*bucket->pages[pos/entries_per_page])[pos%entries_per_page];
     ++m.visits;++m.intersections;
     const mapping::Run run{e.begin,e.count,{std::uint32_t(root),e.index},e.stride,e.step};
     if(group.size()==1){consume(run,group.front());continue;}
     for(std::size_t k=0;k<group.size();++k){
      const auto&local=group_weights[k];
      apply(run,group[k],*local.source,local.aggregate,lo,out,m);
     }
    }
   }
   return;
  }
  const auto actual=c_.roots_[root].spec.actual;
  for(std::size_t i=0;i<auth_.claims.size();++i){
   if(!onRoot(i,root))continue;
   const auto&p=auth_.claims[i];const auto padded=c_.claims_[i].padded;
   p.plan().forRoot(root,auth_.store,[&](const mapping::Run&r){
    ++m.visits;++m.clips;validateRun(r,root,actual,padded);const auto slice=clipRun(r,lo,hi);
    if(slice.first>=slice.last)return;
    ++m.intersections;auto clipped=r;clipped.begin+=slice.first*r.logical_step;
    clipped.first.index=std::uint32_t(std::int64_t(r.first.index)+std::int64_t(slice.first)*r.stride);
    clipped.count=slice.last-slice.first;consume(clipped,i);
   },&m.reads);
  }
 }
 public:
 CategoryKernelPlan(const Commitments&c,const std::vector<CategoryTile>&tiles,const std::vector<Fr>&rho,
                    const Authentication&auth,unsigned options,unified::detail::Workers*workers)
 :c_(c),tiles_(tiles),rho_(rho),auth_(auth),options_(options&category_opts),workers_(workers){
  need(!c_.category_failed_ && auth_.session==c_.session_ && auth_.transcript.lock()==c_.io_.transcriptOwner()
       && c_.io_.session()==c_.session_ && auth_.claims.size()==c_.claims_.size()
       && rho_.size()==c_.claims_.size(),"category K prepared session/dimensions");
  for(const auto&p:auth_.claims){need(p.ready && p.plan().frozen,"category K requires frozen mapping");original_runs_+=p.plan().run_count;}
  if(options_&unified::K3){weights_.resize(rho_.size());weight_bytes_=weights_.capacity()*sizeof(WeightCache);need(weight_bytes_<=category_weight_limit,"category K weight cache metadata");}
  if(!(options_&unified::K1))return;
  std::unordered_map<const mapping::Plan*,std::size_t>groups;
  for(std::size_t i=0;i<auth_.claims.size();++i){
   const auto&plan=auth_.claims[i].plan();
   auto group=groups.emplace(&plan,mapping_groups_.size());
   if(group.second)mapping_groups_.emplace_back();
   mapping_groups_[group.first->second].push_back(i);
  }
  roots_.resize(c_.roots_.size());buckets_by_tile_.resize(tiles_.size());
  for(std::size_t k=0;k<tiles_.size();++k){
   const auto&t=tiles_[k];auto&r=roots_.at(t.root);
   if(r.count){
    need(r.first+r.count==k && t.begin==tiles_[k-1].begin+tiles_[k-1].segment.padded,"category K root tile order");
   }else r.first=k;
   ++r.count;
  }
  clearIndex();
  index_peak_=index_bytes_;
 }
 void tile(std::size_t k,Fr*out,std::size_t count){
  const auto&t=tiles_.at(k);const auto root=t.root,begin=t.begin;
  const auto actual=c_.roots_.at(root).spec.actual;
  need(count==t.segment.padded && begin<=actual && count<=actual-begin && count<=(1u<<20)
       && (out || !count),"category bounded K tile");
  auto&m=c_.auth_metrics_;
  if(!options_){
   const auto visits=m.kernel_run_visits,clips=m.kernel_run_clips,intersections=m.kernel_run_intersections,coordinates=m.kernel_coordinates;
   const auto file_bytes=auth_.store.read_bytes,file_blocks=auth_.store.read_blocks;
   c_.categoryKernelInto(root,begin,count,rho_,auth_,out);
   computed_.visits+=m.kernel_run_visits-visits;computed_.clips+=m.kernel_run_clips-clips;
   computed_.intersections+=m.kernel_run_intersections-intersections;computed_.coordinates+=m.kernel_coordinates-coordinates;
   computed_.reads.bytes+=auth_.store.read_bytes-file_bytes;computed_.reads.blocks+=auth_.store.read_blocks-file_blocks;
   return;
  }
  const auto started=CategoryClock::now();
  ++m.kernel_calls;m.kernel_peak_bytes=std::max<std::uint64_t>(m.kernel_peak_bytes,count*sizeof(Fr));
  if(!count)return;
  indexRoot(root);
  const auto n=parts(count);
  std::array<KernelLocal,unified::detail::Workers::capacity>local{};
  const auto job=[&](std::size_t j){
   const auto lo=begin+count*j/n,hi=begin+count*(j+1)/n;
   const Bucket*b=indexed_?&buckets_[buckets_by_tile_[k].first+j]:nullptr;
   compute(root,lo,hi,out+(lo-begin),b,local[j].stats);
  };
  if(n>1){
   const auto parallel_started=CategoryClock::now();
   workers_->run(n,job,true);++parallel_tiles_;worker_peak_=std::max<std::uint64_t>(worker_peak_,n);
   parallel_seconds_+=categorySeconds(parallel_started);
  }else job(0);
  std::uint64_t total_coordinates=0,maximum_coordinates=0;
  for(unsigned j=0;j<n;++j){
   const auto&stats=local[j].stats;computed_.merge(stats);
   total_coordinates+=stats.coordinates;
   maximum_coordinates=std::max(maximum_coordinates,stats.coordinates);
   m.kernel_coordinates+=stats.coordinates;m.kernel_run_visits+=stats.visits;
   m.kernel_run_clips+=stats.clips;m.kernel_run_intersections+=stats.intersections;
   auth_.store.read_bytes+=stats.reads.bytes;auth_.store.read_blocks+=stats.reads.blocks;
  }
  if(total_coordinates)worker_load_ratio_peak_=std::max(worker_load_ratio_peak_,double(maximum_coordinates)*n/total_coordinates);
  m.kernel_wall+=categorySeconds(started);
  if(indexed_ && k+1==roots_[root].first+roots_[root].count)clearIndex();
 }
 void report()const{
  const auto file_reads=computed_.reads.blocks+index_reads_.blocks;
  const auto charged_products=computed_.products+aggregate_mul_;
  std::cerr<<"CATEGORY_K_OPT {\"mask\":"<<options_<<",\"original_runs\":"<<original_runs_
   <<",\"unique_index_mapping_groups\":"<<mapping_groups_.size()<<",\"visited_runs\":"<<computed_.visits<<",\"intersecting_runs\":"<<computed_.intersections
   <<",\"clip_attempts\":"<<(computed_.clips+index_clips_)<<",\"indexed_runs\":"<<indexed_runs_
   <<",\"index_intersections\":"<<index_intersections_<<",\"index_entries\":"<<index_entries_
   <<",\"tile_slices\":"<<tile_slices_<<",\"extra_tile_slices\":"<<extra_tile_slices_
   <<",\"extra_worker_slices\":"<<extra_worker_slices_<<",\"empty_worker_buckets_skipped\":"<<empty_buckets_skipped_
   <<",\"noncontiguous_runs\":"<<noncontiguous_runs_<<",\"shared_group_runs\":"<<shared_group_runs_
   <<",\"shared_group_entries\":"<<shared_group_entries_<<",\"group_span_allocated_bytes\":"<<group_span_bytes_
   <<",\"worker_coordinate_max_over_mean_peak\":"<<worker_load_ratio_peak_
   <<",\"index_fallback_roots\":"<<index_fallbacks_<<",\"index_noncompact_roots\":"<<noncompact_roots_
   <<",\"index_entry_bytes\":"<<sizeof(Entry)<<",\"index_seconds\":"<<index_seconds_
   <<",\"index_peak_bytes\":"<<index_peak_<<",\"index_limit_bytes\":"<<0
   <<",\"frozen_file_block_reads\":"<<file_reads<<",\"frozen_file_bytes\":"<<(computed_.reads.bytes+index_reads_.bytes)
   <<",\"coordinates\":"<<computed_.coordinates<<",\"parallel_tiles\":"<<parallel_tiles_
   <<",\"workers_peak\":"<<worker_peak_<<",\"parallel_seconds\":"<<parallel_seconds_
   <<",\"weight_cache_peak_bytes\":"<<weight_bytes_<<",\"weight_cache_limit_bytes\":"<<category_weight_limit
   <<",\"weight_cache_seconds\":"<<weight_seconds_<<",\"weight_rebuilds\":"<<(computed_.weight_rebuilds+weight_rebuilds_)
   <<",\"weight_cache_uses\":"<<computed_.weight_cache_uses<<",\"aggregate_group_hits\":"<<computed_.cached_groups
   <<",\"aggregate_precompute_multiplications\":"<<aggregate_mul_
   <<",\"stride_zero_coordinates\":"<<computed_.strides[0]<<",\"stride_forward_coordinates\":"<<computed_.strides[1]
   <<",\"stride_reverse_coordinates\":"<<computed_.strides[2]<<",\"stride_general_coordinates\":"<<computed_.strides[3]
   <<",\"logical_power_of_two_coordinates\":"<<computed_.logical_steps[0]<<",\"logical_general_coordinates\":"<<computed_.logical_steps[1]
   <<",\"fast_blocks\":"<<computed_.fast_blocks<<",\"fast_coordinates\":"<<computed_.fast_coordinates
   <<",\"reference_multiplications\":"<<computed_.reference_products<<",\"charged_multiplications\":"<<charged_products
   <<",\"eliminated_multiplications\":"<<(std::int64_t(computed_.reference_products)-std::int64_t(charged_products))
   <<",\"background_pipeline_enabled\":false}\n";
 }
};
std::vector<Fr>Commitments::categoryKernel(std::size_t root,std::size_t begin,std::size_t count,const std::vector<Fr>&rho,const Authentication&auth,unsigned optimization,unified::detail::Workers*workers)const{
 need(root<roots_.size() && begin<=roots_[root].spec.actual && count<=roots_[root].spec.actual-begin && count<=(1u<<20),"category bounded K tile");
 std::vector<Fr>out(count);
 if(optimization&category_opts){
  const std::vector<CategoryTile>tiles{{{0,count},root,begin}};
  CategoryKernelPlan kernel(*this,tiles,rho,auth,optimization,workers);
  kernel.tile(0,out.data(),count);
 }else categoryKernelInto(root,begin,count,rho,auth,out.data());
 if(crypto::opt2MetricsEnabled())std::cerr<<"CATEGORY_K_VECTOR_WRAPPER {\"elements\":"<<count<<",\"bytes\":"<<count*sizeof(Fr)<<",\"PublicK_initial_builder\":false}\n";
 return out;
}
std::optional<Fr>Commitments::categoryKernelEndpoint(const std::vector<Fr>&q,const std::vector<Fr>&rho,const Authentication&auth,bool require_profitable)const{
 need(!category_failed_ && auth.session==session_ && auth.transcript.lock()==io_.transcriptOwner() && io_.session()==session_,"category endpoint prepared session");
 need(layout_.category_mode && q.size()<=32 && rho.size()==claims_.size() && auth.claims.size()==claims_.size(),"category endpoint dimensions");
 auto started=std::chrono::steady_clock::now();std::uint64_t total=0,eligible=0,fast=0,enumerated=0,blocks=0;
 auto power=[](std::size_t n){return n && !(n&(n-1));};
 auto base=[&](std::size_t k){const auto&r=roots_.at(k).spec;need(r.category<5,"category endpoint root category");const auto&c=layout_.categories[r.category];need(r.category_offset<=c.actual && r.actual<=c.actual-r.category_offset,"category endpoint actual bounds");auto z=c.logical_offset+r.category_offset;need(z<=(std::size_t(1)<<q.size()) && r.actual<=(std::size_t(1)<<q.size())-z,"category endpoint global bounds");return z;};
 auto each=[&](std::size_t i,const auto&consume){const auto&c=claims_[i];const auto&p=auth.claims[i];for(std::size_t root=0;root<roots_.size();++root){if(c.only_root>=0 && std::size_t(c.only_root)!=root)continue;if(!p.plan().hasRoot(root))continue;auto offset=base(root);p.plan().forRoot(root,auth.store,[&](const mapping::Run&r){
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
 for(std::size_t i=0;i<claims_.size();++i){need(auth.claims[i].ready && auth.claims[i].plan().frozen,"category endpoint requires frozen mapping");each(i,[&](const mapping::Run&r,std::size_t offset){total+=r.count;chunks(r,offset,[&](std::size_t,std::size_t,std::size_t length,unsigned,bool use){if(use)eligible+=length;});});}
 if(require_profitable && (eligible<1024 || eligible*4<total*3)){std::cerr<<"CATEGORY_K_ENDPOINT {\"admitted\":false,\"basis\":\"POWER_STRIDE_CARRY_BORROW-v2\",\"eligible_coordinates\":"<<eligible<<",\"map_coordinates\":"<<total<<",\"wall_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<"}\n";return std::nullopt;}
 Weights global(q);std::size_t weight_peak=global.bytes();Fr out=0;
 for(std::size_t i=0;i<claims_.size();++i){const auto&c=claims_[i];const auto&p=auth.claims[i];std::unique_ptr<Weights>scratch;if(!p.weights)scratch=std::make_unique<Weights>(c.point);const auto&w=p.weights?*p.weights:*scratch;weight_peak=std::max(weight_peak,global.bytes()+(scratch?scratch->bytes():0));TerminalBasis aligned(c.point,q);CarryTerminalBasis carry(c.point,q);
  each(i,[&](const mapping::Run&r,std::size_t offset){chunks(r,offset,[&](std::size_t j,std::size_t z,std::size_t length,unsigned bits,bool use){
   if(use){bool simple=r.logical_step==1 && (r.stride==0 || (r.stride==1 && !(z&(length-1))));out+=rho[i]*(simple?aligned.block(j,z,bits,r.stride==0):carry.block(j,z,bits,r.logical_step,r.stride));fast+=length;++blocks;}
   else{std::int64_t index=std::int64_t(z);w.scaled(j,length,rho[i],[&](const Fr&value){out+=value*global.at(std::size_t(index));index+=r.stride;},r.logical_step);enumerated+=length;}
  });});

 }
 need(weight_peak<=8u<<20,"category endpoint public weight tables bound");auth_metrics_.endpoint_fast_coordinates+=fast;auth_metrics_.endpoint_fast_blocks+=blocks;std::cerr<<"CATEGORY_K_ENDPOINT {\"admitted\":true,\"basis\":\"POWER_STRIDE_CARRY_BORROW-v2\",\"eligible_coordinates\":"<<eligible<<",\"map_coordinates\":"<<total<<",\"fast_coordinates\":"<<fast<<",\"enumerated_coordinates\":"<<enumerated<<",\"blocks\":"<<blocks<<",\"additional_endpoint_weight_table_peak_bytes\":"<<weight_peak<<",\"K_dense_array_bytes\":0,\"wall_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<"}\n";return out;
}
void Commitments::authenticateCategoryInput(const std::vector<Fr>&x){
 namespace ui=unified;auto started=std::chrono::steady_clock::now();
 need(!category_failed_ && layout_.category_mode && single_pc_ && sealed_ && tails_constrained_ && !authenticated_ && io_.session()==session_ && identity(layout_)==identity_ && x==single_x_,"category authentication lifecycle/statement");
 // C_U and the verifier-owned statement/map are fixed before r_X is sampled.
 Writer xb;xb.text(single::suite);xb.text(identity_);xb.number(x.size(),8);for(const auto&v:x)xb.scalar(v);bindPublic(io_,"CategoryInput/X-statement-and-seal",xb.b);
 std::vector<Fr>rx;const auto padded=layout_.categories[0].padded;for(unsigned j=0;j<single::bits(padded);++j)rx.push_back(io_.coin("CategoryInput/X-query/"+std::to_string(j)));
 Weights wx(rx);Fr expected=0;for(std::size_t j=0;j<x.size();++j)expected+=wx.at(j)*x[j];
 auto spans=layout_.x_spans;auto xindex=[spans](std::size_t j){for(const auto&s:spans){if(j<s.count)return s.offset+j;j-=s.count;}throw std::runtime_error("category X trusted map omission");};
 deferBase("public/X-committed-consistency",x.size(),padded,xindex,publicHidden(io_,"CategoryInput/X-expected",expected),rx);
 need(names_.count("public/X-committed-consistency")==1 && !claims_.empty() && claims_.size()<=8192,"category mandatory X claim");
 auto tile_started=std::chrono::steady_clock::now();auto plan=categoryTiles(layout_);
 std::cerr<<"CATEGORY_TILE_DETAIL {\"tile_enumeration_count\":1,\"tiles\":"<<plan.size()<<",\"tile_mapping_time\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-tile_started).count()<<"}\n";std::vector<ui::Segment>segments;for(const auto&t:plan)segments.push_back(t.segment);
 Writer declaration;declaration.text(single::suite);declaration.text(identity_);declaration.number(claims_.size());for(const auto&c:claims_){declaration.text(c.id);declaration.raw(Bytes(c.map_digest.begin(),c.map_digest.end()));}
 // Tiles are derived local computation views; no tile commitment or independent
 // claim is ever sent. Their descriptor is bound as an execution consistency aid.
 declaration.number(segments.size());for(auto s:segments){declaration.number(s.offset,8);declaration.number(s.padded,8);}bindPublic(io_,"CategoryInput/all-claims-fixed",declaration.b);
 need(category_auth_ && category_auth_->claims.size()==claims_.size(),"category missing prepared session");auto&auth=*category_auth_;std::vector<Fr>biases;
 for(std::size_t i=0;i<claims_.size();++i)biases.push_back(prepareClaim(claims_[i],auth,i));
 std::vector<Fr>rho;std::vector<Hidden>values;Fr bias=0;
 for(std::size_t i=0;i<claims_.size();++i){rho.push_back(io_.coin("CategoryInput/rho/"+std::to_string(i)));need(!rho.back().isZero(),"category zero aggregation");values.push_back(claims_[i].value);bias+=rho.back()*biases[i];}
 const auto u_options=io_.prover()?ui::UOptions::environment():ui::UOptions{};
 std::unique_ptr<ui::detail::Workers>workers;
 if(io_.prover() && (u_options.optimization&(ui::U2|ui::K2|ui::F1)))
  workers=std::make_unique<ui::detail::Workers>(u_options.threads);
 auto initial=combine(session_,"CategoryInput/initial",values,rho);initial.value-=bias;initial.handle.point-=scalarCommit(bias,Fr(0));bindPublic(io_,"CategoryInput/initial",encode(initial.handle));
 auto kernel=std::make_unique<CategoryKernelPlan>(*this,plan,rho,auth,u_options.optimization,workers.get());
 auto generate=[&](std::size_t k,Fr*out,std::size_t count){kernel->tile(k,out,count);};
 std::unique_ptr<ui::PublicK>state;if(io_.prover())state=std::make_unique<ui::PublicK>(segments,generate,ui::KBudget::environment(),session_,single_pc_->plan().domain);
 ui::Factors factors;ui::Inputs inputs;if(io_.prover()){
  factors=[&](std::size_t k,const ui::Consume&consume){const auto&t=plan.at(k);auto a=[&,root=t.root,begin=t.begin](std::size_t j){return roots_[root].value(begin+j);};state->withRoot(k,session_,[&](const Value&b){consume(a,b);});};
  inputs=[&](std::size_t k){
   const auto&t=plan.at(k);const auto&root=roots_[t.root];
   ui::InputSpan s;s.stored=s.padded=t.segment.padded;s.storage_identity=&root;s.storage_offset=t.begin;
   if(u_options.optimization&(ui::U3|ui::F1)){
    need(root.sealed_storage_valid && root.sealed_storage_valid() && t.begin<=root.sealed_stored &&
         s.stored<=root.sealed_stored-t.begin,"category sealed input tile bounds");
    if(root.sealed_span){
     auto first=root.sealed_span(t.begin,s.stored);
     if(first.count==s.stored)s.data=first.data;
     if(!s.data){
      s.window=[&,index=t.root,begin=t.begin,n=s.stored](std::size_t at,std::size_t count){
       need(at<n && count && count<=n-at,"category input window relative bounds");
       return roots_[index].sealed_span(begin+at,count);
      };
      s.stable_windows=true;
     }
    }
   }
   if(!s.data)s.read=[&,index=t.root,begin=t.begin](std::size_t j){return roots_[index].value(begin+j);};
   return s;
  };
 }
 bool fast_endpoint=true;if(const auto*v=std::getenv("ZKGPT_CATEGORY_K_ENDPOINT")){std::string text(v);need(text=="0" || text=="1","category K endpoint switch");fast_endpoint=text=="1";}
 auto endpoint=[&](const std::vector<Fr>&q){auto endpoint_started=std::chrono::steady_clock::now();++auth_metrics_.endpoint_calls;auto compute=[&]()->Fr{if(fast_endpoint){auto out=categoryKernelEndpoint(q,rho,auth);if(out)return *out;}Fr out=0;for(std::size_t k=0;k<plan.size();++k){const auto&t=plan.at(k);std::vector<Fr>v(t.segment.padded);generate(k,v.data(),v.size());auto width=v.size();unsigned j=0;for(;width>1;width/=2,++j)for(std::size_t a=0;a<width/2;++a)v[a]=v[2*a]+q[j]*(v[2*a+1]-v[2*a]);out+=ui::selector(segments[k],q)*v[0];}return out;};auto result=compute();auth_metrics_.endpoint_seconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-endpoint_started).count();return result;};
 auto reduced=ui::reduce(io_,single_pc_->plan().domain,segments,initial,factors,endpoint,rng_,"UnifiedInput/sumcheck",state.get(),inputs,workers.get());if(state)state->report();state.reset();kernel->report();kernel.reset();
 single::open(io_,*single_pc_,{reduced.point,reduced.endpoints[0].handle},x,io_.prover()?&*single_secret_:nullptr,io_.prover()?&reduced.endpoints[0]:nullptr,rng_,"UnifiedInput/opening/C_U");
 Writer done;done.text(single::suite);done.text("public/X-committed-consistency");done.number(claims_.size());bindPublic(io_,"CategoryInput/all-claims-authenticated",done.b);io_.coin("CategoryInput/all-claims-authenticated/ack");authenticated_=true;
 std::cerr<<"CATEGORY_INPUT {\"claims\":"<<claims_.size()<<",\"category_count\":"<<single_pc_->plan().segments.size()<<",\"computation_tiles\":"<<plan.size()<<",\"storage_chunks\":"<<roots_.size()<<",\"K_tile_peak_bound_bytes\":33554432,\"full_witness_copy_bytes\":0,\"X_claim_authenticated\":true,\"seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<",\"production_gate\":\"BLOCKED_CRYPTOGRAPHIC_DESIGN\",\"may_enter_stage2\":false}\n";
}
}
