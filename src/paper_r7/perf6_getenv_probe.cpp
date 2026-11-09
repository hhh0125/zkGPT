// Test-only LD_PRELOAD guard. Never linked into production executables.
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
static unsigned reads=0;
extern "C" char* getenv(const char* name) noexcept {
 using Get=char*(*)(const char*);
 static auto real=reinterpret_cast<Get>(dlsym(RTLD_NEXT,"getenv"));
 if(std::strcmp(name,"ZKGPT_PERF5_PRODUCT_REFERENCE")==0 || std::strcmp(name,"ZKGPT_PERF6_A2")==0 || std::strcmp(name,"ZKGPT_PERF6_C2")==0)++reads;
 return real(name);
}
__attribute__((destructor)) static void report(){std::fprintf(stderr,"PERF6_CANDIDATE_ENV_READS %u\n",reads);}
