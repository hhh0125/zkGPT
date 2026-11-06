#pragma once
#include "opt2_metrics.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
namespace zkgpt::r7::perf4 {
// Public dimension/allocation metadata only. Nested child time is subtracted
// once; process CPU includes workers, allocation counts cover this thread only.
class Scope {
 using Clock=std::chrono::steady_clock;
 inline static thread_local Scope*current_=nullptr;
 Scope*parent_=nullptr;const char*kind_;std::string role_;std::size_t size_,copies_;
 bool enabled_;Clock::time_point start_;double cpu_=0,child_wall_=0,child_cpu_=0;
 std::uint64_t child_allocs_=0,child_bytes_=0;crypto::AllocationTrace allocations_;
 public:Scope(const char*kind,const std::string&role,std::size_t size,std::size_t copies=0):kind_(kind),role_(role),size_(size),copies_(copies),enabled_(std::getenv("ZKGPT_PERF4_PROFILE")!=nullptr){
  if(enabled_){parent_=current_;current_=this;start_=Clock::now();cpu_=crypto::processCPU();}
 }
 ~Scope(){if(!enabled_)return;
  auto wall=std::chrono::duration<double>(Clock::now()-start_).count(),cpu=crypto::processCPU()-cpu_;
  auto count=allocations_.count(),bytes=allocations_.bytes();current_=parent_;
  if(parent_){parent_->child_wall_+=wall;parent_->child_cpu_+=cpu;parent_->child_allocs_+=count;parent_->child_bytes_+=bytes;}
  std::cerr<<"PERF4_SCOPE {\"kind\":"<<std::quoted(kind_)<<",\"role\":"<<std::quoted(role_)<<",\"size\":"<<size_<<",\"inclusive_wall\":"<<wall<<",\"exclusive_wall\":"<<std::max(0.,wall-child_wall_)<<",\"inclusive_cpu\":"<<cpu<<",\"exclusive_cpu\":"<<std::max(0.,cpu-child_cpu_)<<",\"allocations\":"<<count<<",\"allocated_bytes\":"<<bytes<<",\"exclusive_allocations\":"<<(count>=child_allocs_?count-child_allocs_:0)<<",\"exclusive_allocated_bytes\":"<<(bytes>=child_bytes_?bytes-child_bytes_:0)<<",\"analytical_copy_bytes\":"<<copies_<<"}\n";
 }
};
}
