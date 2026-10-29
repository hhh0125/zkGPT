#include "opt2_metrics.hpp"
#include <ctime>
#include <cstdlib>
#include <new>
#include <iostream>
#include <array>
namespace {thread_local std::uint64_t allocations=0;thread_local unsigned tracing=0;}
void* operator new(std::size_t n){for(;;){if(void*p=std::malloc(n?n:1)){if(tracing)++allocations;return p;}auto handler=std::get_new_handler();if(!handler)throw std::bad_alloc();handler();}}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,std::size_t)noexcept{std::free(p);}
void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
namespace zkgpt::r7::crypto {
double processCPU(){timespec t{};clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t);return t.tv_sec+t.tv_nsec*1e-9;}
bool opt2MetricsEnabled(){static bool enabled=std::getenv("ZKGPT_OPT2_METRICS")!=nullptr;return enabled;}
namespace {
struct Bin {std::uint64_t calls=0,terms=0,allocations=0;double cpu=0;};
struct Stats {std::array<Bin,6>bins{};~Stats(){if(!opt2MetricsEnabled())return;const char*names[]={"1","2","3-8","9-64","65-1024",">1024"};double total=0;for(auto&b:bins)total+=b.cpu;std::cerr<<"OPT2_MSM {\"cpu_seconds\":"<<total<<",\"allocation_scope\":\"ordinary C++ new on tracing thread inside wrappers; excludes caller construction, C malloc, aligned and worker allocation\",\"bins\":[";for(unsigned i=0;i<6;++i){if(i)std::cerr<<',';auto&b=bins[i];std::cerr<<"{\"length\":\""<<names[i]<<"\",\"calls\":"<<b.calls<<",\"terms\":"<<b.terms<<",\"cpu_seconds\":"<<b.cpu<<",\"cpp_allocations\":"<<b.allocations<<'}';}std::cerr<<"]}\n";}};
Stats&stats(){static Stats s;return s;}
}
MSMTrace::MSMTrace(std::size_t n):bin_(n==1?0:n==2?1:n<=8?2:n<=64?3:n<=1024?4:5),started_(0),allocations_(0){if(opt2MetricsEnabled()){auto&b=stats().bins[bin_];++b.calls;b.terms+=n;allocations_=allocations;++tracing;started_=processCPU();}}
MSMTrace::~MSMTrace(){if(opt2MetricsEnabled()){auto&b=stats().bins[bin_];b.cpu+=processCPU()-started_;b.allocations+=allocations-allocations_;--tracing;}}
}
