// Test-only sampled allocation stack collector; never linked into product code.
#include <cstddef>
#include <cstdio>
#include <execinfo.h>
#include <dlfcn.h>
#include <time.h>
extern "C" void* __libc_malloc(std::size_t);
static thread_local bool active=false;
static unsigned long calls=0,bytes=0;
static double allocator_seconds=0;
extern "C" void* malloc(std::size_t n) noexcept {
 if(active)return __libc_malloc(n);
 active=true;timespec a{},b{};clock_gettime(CLOCK_MONOTONIC,&a);
 void*p=__libc_malloc(n);clock_gettime(CLOCK_MONOTONIC,&b);
 allocator_seconds+=(b.tv_sec-a.tv_sec)+(b.tv_nsec-a.tv_nsec)*1e-9;
 bytes+=n;++calls;
 if(calls%1000==0){void*frames[12];int count=backtrace(frames,12);std::fprintf(stderr,"PERF6_ALLOC %lu %zu",calls,n);for(int i=0;i<count;++i){Dl_info info{};if(dladdr(frames[i],&info))std::fprintf(stderr," %s@%zx",info.dli_fname,(std::size_t)frames[i]-(std::size_t)info.dli_fbase);}std::fprintf(stderr,"\n");}
 active=false;return p;
}
__attribute__((destructor)) static void report(){active=true;std::fprintf(stderr,"PERF6_ALLOC_TOTAL {\"calls\":%lu,\"bytes\":%lu,\"malloc_only_seconds\":%.9f,\"instrumented\":true}\n",calls,bytes,allocator_seconds);}
