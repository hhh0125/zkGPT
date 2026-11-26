#pragma once
#include "paper_r7_unified_workers.hpp"
#include "thread_joiner.hpp"
#include <algorithm>
#include <sched.h>

namespace zkgpt::r7::oneblock::detail {
using NativeWorkers=unified::detail::Workers;

inline unsigned nativeWorkerBudget(){
 cpu_set_t affinity;
 CPU_ZERO(&affinity);
 const auto configured=std::min<std::size_t>(NativeWorkers::capacity,zkgpt::workerThreadCount());
 if(sched_getaffinity(0,sizeof(affinity),&affinity)!=0)return configured;
 return std::clamp<unsigned>(CPU_COUNT(&affinity),1,configured);
}
inline NativeWorkers& nativeWorkers(){
 static thread_local NativeWorkers workers(nativeWorkerBudget());
 return workers;
}

inline unsigned nativePartitions(std::size_t count,const NativeWorkers& workers,std::size_t minimum){
 return unsigned(std::min<std::size_t>(workers.count(),std::max<std::size_t>(1,count/minimum)));
}

template<class Work>
unsigned nativeRanges(std::size_t count,NativeWorkers& workers,std::size_t minimum,const Work& work){
 const auto partitions=nativePartitions(count,workers,minimum);
 workers.run(partitions,[&](std::size_t i){
  work(i,count*i/partitions,count*(i+1)/partitions);
 },partitions>1);
 return partitions;
}
}
