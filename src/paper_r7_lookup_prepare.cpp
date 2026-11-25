#include "paper_r7_lookup_prepare.hpp"
#include "paper_r7_native_workers.hpp"
#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>

namespace zkgpt::r7::oneblock::classified {
namespace {
using Clock=std::chrono::steady_clock;
double seconds(Clock::time_point start){return std::chrono::duration<double>(Clock::now()-start).count();}
constexpr std::size_t minimum_rows=8192;

std::uint32_t u32(const Fr& value){
 auto bytes=r1::encode(value);
 for(unsigned j=4;j<bytes.size();++j)need(!bytes[j],"prepared table/query bound");
 std::uint32_t out=0;
 for(unsigned j=0;j<4;++j)out|=std::uint32_t(bytes[j])<<(8*j);
 return out;
}
std::vector<Fr>& saved(Prepared& p,const std::string& id,std::size_t n){
 auto& values=p[id];
 if(!values)values=std::make_shared<std::vector<Fr>>(n);
 return *values;
}

using Histograms=std::vector<std::vector<std::size_t>>;
void orderedOffsets(Histograms& counts,std::vector<Fr>& final){
 for(std::size_t digit=0;digit<final.size();++digit){
  std::size_t before=0;
  for(auto& local:counts){
   const auto count=local[digit];
   local[digit]=before;
   before+=count;
  }
  final[digit]=Fr(before);
 }
}

template<class ReadAddress>
void memory(Prepared& p,const std::string& id,std::size_t n,std::size_t table,
            const ReadAddress& address,detail::NativeWorkers& workers){
 auto& read=saved(p,id+"/read",n);
 auto& final=saved(p,id+"/final",table);
 const auto parts=detail::nativePartitions(n,workers,minimum_rows);
 Histograms counts(parts,std::vector<std::size_t>(table));
 std::vector<std::uint32_t> addresses(parts>1?n:0);
 detail::nativeRanges(n,workers,minimum_rows,[&](std::size_t k,std::size_t first,std::size_t last){
  auto& local=counts[k];
  for(auto i=first;i<last;++i){
   const auto j=address(i);
   read[i]=Fr(local[j]++);
   if(parts>1)addresses[i]=j;
  }
 });
 orderedOffsets(counts,final);
 if(parts>1)detail::nativeRanges(n,workers,minimum_rows,[&](std::size_t k,std::size_t first,std::size_t last){
  if(!k)return;
  const auto& offsets=counts[k];
  for(auto i=first;i<last;++i)read[i]+=Fr(offsets[addresses[i]]);
 });
}

template<class GetDigit>
void range(Prepared& p,const std::string& id,std::size_t n,unsigned bits,const GetDigit& digits,
           detail::NativeWorkers& workers){
 struct Lane {
  std::vector<Fr>* digit;
  std::vector<Fr>* read;
  std::vector<Fr>* final;
  unsigned mask;
  Histograms counts;
  std::vector<std::uint16_t> addresses;
 };
 const auto parts=detail::nativePartitions(n,workers,minimum_rows);
 std::vector<Lane> lanes;
 const unsigned lanes_count=(bits+8)/9;
 lanes.reserve(lanes_count);
 for(unsigned h=0;h<lanes_count;++h){
  const auto digit=id+"/digit/"+std::to_string(h);
  const auto memory=id+"/memory/"+std::to_string(h);
  const unsigned size=1u<<std::min(9u,bits-9*h);
  lanes.push_back({&saved(p,digit,n),&saved(p,memory+"/read",n),&saved(p,memory+"/final",size),size-1,
                   Histograms(parts,std::vector<std::size_t>(size)),std::vector<std::uint16_t>(parts>1?n:0)});
 }
 detail::nativeRanges(n,workers,minimum_rows,[&](std::size_t k,std::size_t first,std::size_t last){
  std::array<unsigned,32> masks{};
  for(unsigned h=0;h<lanes_count;++h)masks[h]=lanes[h].mask;
  for(auto i=first;i<last;++i){
   auto value=digits(i);
   for(unsigned h=0;h<lanes_count;++h){
    auto& lane=lanes[h];
    const auto digit=value(h)&masks[h];
    (*lane.digit)[i]=Fr(digit);
    (*lane.read)[i]=Fr(lane.counts[k][digit]++);
    if(parts>1)lane.addresses[i]=std::uint16_t(digit);
   }
  }
 });
 for(auto& lane:lanes)orderedOffsets(lane.counts,*lane.final);
 if(parts>1)detail::nativeRanges(n,workers,minimum_rows,[&](std::size_t k,std::size_t first,std::size_t last){
  if(!k)return;
  for(auto& lane:lanes){
   const auto& offsets=lane.counts[k];
   for(auto i=first;i<last;++i)(*lane.read)[i]+=Fr(offsets[lane.addresses[i]]);
  }
 });
}

void rangeFromField(Prepared& p,const std::string& id,std::size_t n,unsigned bits,const Value& query,
                   detail::NativeWorkers& workers){
 range(p,id,n,bits,[&](std::size_t i){
  return [bytes=r1::encode(query(i))](unsigned lane){
   const auto offset=9*lane;
   const auto word=unsigned(bytes[offset/8])|(unsigned(bytes[offset/8+1])<<8);
   return word>>(offset%8);
  };
 },workers);
}

template<class ValueAt>
void rangeFromU32(Prepared& p,const std::string& id,std::size_t n,unsigned bits,const ValueAt& value,
                 detail::NativeWorkers& workers){
 range(p,id,n,bits,[&](std::size_t i){
  return [number=value(i)](unsigned lane){return number>>(9*lane);};
 },workers);
}

struct TaskResult {
 Prepared prepared;
 double range=0,exp=0,exp_range=0,indexed=0;
};

void prepareExp(TaskResult& result,const r1::ExpLookup& lookup,const r1::PCSpec& spec,
                const Value& query,const Value& exp,detail::NativeWorkers& workers){
 const auto id="r7/"+lookup.id;
 const auto start=Clock::now();
 auto& p=result.prepared;
 auto& x=saved(p,id+"/x",spec.padded);
 auto& selector=saved(p,id+"/selector",spec.padded);
 auto& delta=saved(p,id+"/delta",spec.padded);
 auto& native=saved(p,id+"/native-E",spec.padded);
 std::vector<std::uint32_t> raw(spec.padded);
 const auto index=[&](std::size_t i){return std::min(8191u,raw[i]);};
 const auto select=[&](std::size_t i){return std::uint32_t(i<spec.actual && raw[i]<8191);};
 const auto difference=[&](std::size_t i){return i>=spec.actual?0u:raw[i]<8191?8190-raw[i]:raw[i]-8191;};
 detail::nativeRanges(spec.padded,workers,minimum_rows,[&](std::size_t,std::size_t first,std::size_t last){
  for(auto i=first;i<last;++i){
   if(i<spec.actual)raw[i]=u32(query(i));
   x[i]=Fr(index(i));selector[i]=Fr(select(i));delta[i]=Fr(difference(i));
   native[i]=exp(i);
   if(i>=spec.actual)native[i]+=Fr(1048576);
  }
 });
 result.exp=seconds(start);
 auto phase=Clock::now();
 rangeFromU32(p,id+"/range/t",spec.padded,32,[&](std::size_t i){return raw[i];},workers);
 rangeFromU32(p,id+"/range/x",spec.padded,13,index,workers);
 rangeFromU32(p,id+"/range/selector",spec.padded,1,select,workers);
 rangeFromU32(p,id+"/range/delta",spec.padded,32,difference,workers);
 result.exp_range=seconds(phase);
 phase=Clock::now();
 memory(p,id+"/indexed",spec.padded,r1::expTable(lookup.profile).size(),index,workers);
 result.indexed=seconds(phase);
}
}

Prepared prepareLookupWithWorkers(const R7Context& c,const Value& base,detail::NativeWorkers& workers){
 const auto started=Clock::now();
 need(bool(base),"prepared requires private base");
 std::vector<Value> sources;
 for(const auto& source:c.plan.sources){
  const auto* spans=&source.spans;
  const auto actual=source.spec.actual;
  sources.push_back([&base,spans,actual](std::size_t i){
   if(i>=actual)return Fr(0);
   for(const auto& s:*spans){if(i<s.count)return base(s.offset+i)+s.bias;i-=s.count;}
   throw std::runtime_error("prepared source map");
  });
 }

 const auto range_count=c.plan.ranges.size(),count=range_count+c.plan.exps.size();
 const auto taskRows=[&](std::size_t j){
  const auto child=j<range_count?c.plan.ranges[j].child:c.plan.exps[j-range_count].index_child;
  return c.plan.sources.at(child).spec.padded;
 };
 const auto execute=[&](std::size_t j,TaskResult& out,detail::NativeWorkers& selected){
  if(j<range_count){
   const auto& lookup=c.plan.ranges[j];
   const auto start=Clock::now();
   rangeFromField(out.prepared,"r7/"+lookup.id,taskRows(j),lookup.bits,sources.at(lookup.child),selected);
   out.range=seconds(start);
  }else{
   const auto& lookup=c.plan.exps[j-range_count];
   prepareExp(out,lookup,c.plan.sources.at(lookup.index_child).spec,
              sources.at(lookup.index_child),sources.at(lookup.value_child),selected);
  }
 };

 Prepared prepared;
 std::array<double,4> kernel_seconds{};
 double grouped_wall=0,merge_wall=0;
 std::size_t grouped_tasks=0;
 detail::NativeWorkers serial(1);
 for(std::size_t first=0;first<count;){
  // Large helpers use row partitions. Consecutive small helpers share the
  // same pool at task granularity, with one private result map per task.
  auto last=first+1;
  if(taskRows(first)<2*minimum_rows)
   while(last<count && last-first<workers.count() && taskRows(last)<2*minimum_rows)++last;
  const auto group=last-first;
  std::vector<TaskResult> result(group);
  const auto phase=Clock::now();
  if(group>1){
   workers.run(group,[&](std::size_t i){execute(first+i,result[i],serial);},true);
   grouped_wall+=seconds(phase);
   grouped_tasks+=group;
  }else execute(first,result[0],workers);
  const auto merge=Clock::now();
  for(auto& out:result){
   kernel_seconds[0]+=out.range;kernel_seconds[1]+=out.exp;
   kernel_seconds[2]+=out.exp_range;kernel_seconds[3]+=out.indexed;
   prepared.merge(out.prepared);
  }
  merge_wall+=seconds(merge);
  first=last;
 }
 std::cerr<<std::setprecision(12)<<"LOOKUP_PREPARE {\"wall_seconds\":"<<seconds(started)
          <<",\"range_task_seconds\":"<<kernel_seconds[0]<<",\"exp_helpers_task_seconds\":"<<kernel_seconds[1]
          <<",\"exp_range_task_seconds\":"<<kernel_seconds[2]<<",\"memory_counter_task_seconds\":"<<kernel_seconds[3]
          <<",\"grouped_wall_seconds\":"<<grouped_wall<<",\"merge_wall_seconds\":"<<merge_wall
          <<",\"range_tasks\":"<<range_count<<",\"exp_tasks\":"<<c.plan.exps.size()
          <<",\"grouped_tasks\":"<<grouped_tasks<<",\"workers\":"<<workers.count()
          <<",\"helpers\":"<<prepared.size()<<"}\n";
 return prepared;
}

Prepared prepareLookup(const R7Context& c,const Value& base){
 return prepareLookupWithWorkers(c,base,detail::nativeWorkers());
}
}
