#pragma once
#include "paper_r7_unified_workers.hpp"
#include <algorithm>
#include <cstddef>
#include <vector>

namespace zkgpt::r7::oneblock::unified::detail {
inline constexpr std::size_t fold_serial_prefix=1024;
inline constexpr std::size_t fold_parallel_minimum=8192;

struct FoldRange {std::size_t root,begin,end;};

// Every entry in counts is an output length. Out-of-place callers may start
// all ranges at zero. In-place callers provide a single dependency-safe wave.
template<class Fold>
std::size_t foldRanges(const std::vector<std::size_t>&counts,std::size_t begin,
                      std::size_t end,Workers&workers,bool per_root,Fold&&fold){
 std::size_t total=0;
 for(auto count:counts)if(count>begin)total+=std::min(count,end)-begin;
 if(total<fold_parallel_minimum){
  for(std::size_t k=0;k<counts.size();++k)
   if(counts[k]>begin)fold(k,begin,std::min(counts[k],end));
  return 0;
 }
 const auto chunk=std::max(fold_serial_prefix,total/(8*std::size_t(workers.count())));
 std::vector<FoldRange> jobs;
 for(std::size_t k=0;k<counts.size();++k){
  const auto last=std::min(counts[k],end);
  for(auto first=begin;first<last;){
   const auto next=per_root?last:first+std::min(chunk,last-first);
   jobs.push_back({k,first,next});
   first=next;
  }
 }
 if(jobs.size()<2){
  for(const auto&job:jobs)fold(job.root,job.begin,job.end);
  return 0;
 }
 workers.run(jobs.size(),[&](std::size_t i){
  const auto&job=jobs[i];
  fold(job.root,job.begin,job.end);
 },true);
 return jobs.size();
}

template<class Fold>
std::size_t foldInPlace(const std::vector<std::size_t>&counts,Workers&workers,Fold&&fold){
 std::size_t largest=0,total=0;
 for(auto count:counts){largest=std::max(largest,count);total+=count;}
 if(total<fold_parallel_minimum){
  for(std::size_t k=0;k<counts.size();++k)if(counts[k])fold(k,0,counts[k]);
  return 0;
 }
 auto jobs=foldRanges(counts,0,fold_serial_prefix,workers,true,fold);
 // Earlier waves have consumed inputs below 2*begin. In this wave all
 // writes are below 2*begin and every read is at or above 2*begin.
 for(auto begin=fold_serial_prefix;begin<largest;begin*=2)
  jobs+=foldRanges(counts,begin,begin*2,workers,false,fold);
 return jobs;
}
}
