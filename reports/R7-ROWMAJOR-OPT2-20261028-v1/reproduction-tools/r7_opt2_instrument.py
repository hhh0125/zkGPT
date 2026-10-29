from pathlib import Path
import json
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd().resolve()==R
P=R/'src/paper_r7'
def write(n,s):(P/n).write_text(s)
def edit(n,a,b):
 p=P/n;s=p.read_text();assert a in s,(n,a[:100]);p.write_text(s.replace(a,b))
plan=R/'docs/superpowers/plans/2026-10-28-rowmajor-opt2.md';plan.parent.mkdir(parents=True,exist_ok=True)
plan.write_text('''# R7 RowMajor OPT2 implementation plan

User-approved scope: R7-ROWMAJOR-FS-PERFORMANCE-OPT2, baseline a7003d3a5e02c6a2c9d6f57a577420e404e4eb8f. Implement directly; Windows is source authority, edits/builds/tests in fixed zkGPT-work, then restore exact source bytes to Windows. No other source copy or worktree, no full12, no deletion. Retain RowMajor anchor/pins, FS suite, RNG and challenge order, equations, six roots, 936 claims, 8193 rows, 627 GKR rounds, six openings, 5351 relations and blocked security state.

1. Instrument baseline mapDigest/kernel/Weights and all four R7 MSM wrappers. Record bins, CPU and wall, coordinates and allocation telemetry. Save baseline binaries. Directed kernel reference and malicious mapping tests before optimizing.
2. Build bounded, private immutable compressed mapping runs after the necessary digest revalidation, before each unchanged aggregation coin. Cache only public weights/indices within the authentication session. Limit maps to 16 MiB, weights to 32 MiB and use the checked old path if the cache cannot fit. Compute partial dot without building six kernels. Dense reference tests, mutation, root-only and budget fallback tests.
3. Compare mulVec, two multiplies/add, and bounded public fixed-base precomputation with seven alternating samples. Replace scalarCommit only on measured win; preserve logical MSM counts. Deterministic full-width scalar equivalence and allocation tests. Never cache a scalar.
4. Extract pure degree 0..3 polynomial computation with deterministic coefficient and folded/terminal reference tests. Try direct polynomial convolution and reuse fixed constants/buffers; retain generic reference if no repeatable benefit. Preserve all IO/RNG/coins.
5. Check legacy hyrax_a3::opening call chain. Run directed tests then independent small FS positives, cross-version compatibility, malformed/mutated-source negatives. Build full binaries and run exactly one full independent Oneblock P then V after measuring benefit. Run requested full public-input negatives without another prover.
6. Archive exact final source/patch/SHA/dependency/config/parameter identity, accepted proof/statement, logs and measurements to Windows; verify compressed members and restore-read SHA. Commit source and deliver the three OPT2 report files with actual final HEAD. No security status promotion.

Review focus: map mutation between defer/barrier; mutation after immutable snapshot; repeated/constant/bias/root-specific coordinates; cache overflow fallback; full-width/zero/negative scalars; degree 0..3 repeated factor expressions; hidden coefficient/challenge equality; public proof tampering. Only public metrics are emitted.
''')
write('opt2_metrics.hpp',r'''#pragma once
#include <cstddef>
#include <cstdint>
namespace zkgpt::r7::crypto {
// Public length/time/allocation metadata only. Never inspect scalar bytes here.
double processCPU();
bool opt2MetricsEnabled();
class MSMTrace {
 std::size_t bin_;double started_;std::uint64_t allocations_;
 public:explicit MSMTrace(std::size_t);~MSMTrace();
};
}
''')
write('opt2_metrics.cpp',r'''#include "opt2_metrics.hpp"
#include <ctime>
#include <cstdlib>
#include <new>
#include <iostream>
#include <array>
namespace {thread_local std::uint64_t allocations=0;thread_local unsigned tracing=0;}
void* operator new(std::size_t n){if(void*p=std::malloc(n?n:1)){if(tracing)++allocations;return p;}throw std::bad_alloc();}
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
struct Stats {std::array<Bin,6>bins{};~Stats(){if(!opt2MetricsEnabled())return;const char*names[]={"1","2","3-8","9-64","65-1024",">1024"};double total=0;for(auto&b:bins)total+=b.cpu;std::cerr<<"OPT2_MSM {\"cpu_seconds\":"<<total<<",\"allocation_scope\":\"C++ new inside traced wrappers; excludes C malloc and caller construction\",\"bins\":[";for(unsigned i=0;i<6;++i){if(i)std::cerr<<',';auto&b=bins[i];std::cerr<<"{\"length\":\""<<names[i]<<"\",\"calls\":"<<b.calls<<",\"terms\":"<<b.terms<<",\"cpu_seconds\":"<<b.cpu<<",\"cpp_allocations\":"<<b.allocations<<'}';}std::cerr<<"]}\n";}};
Stats&stats(){static Stats s;return s;}
}
MSMTrace::MSMTrace(std::size_t n):bin_(n==1?0:n==2?1:n<=8?2:n<=64?3:n<=1024?4:5),started_(0),allocations_(0){if(opt2MetricsEnabled()){auto&b=stats().bins[bin_];++b.calls;b.terms+=n;allocations_=allocations;++tracing;started_=processCPU();}}
MSMTrace::~MSMTrace(){if(opt2MetricsEnabled()){auto&b=stats().bins[bin_];b.cpu+=processCPU()-started_;b.allocations+=allocations-allocations_;--tracing;}}
}
''')
edit('CMakeLists.txt','add_library(paper_r7 STATIC framework.cpp','add_library(paper_r7 STATIC opt2_metrics.cpp framework.cpp')
edit('crypto.hpp','#include "paper_r1/core.hpp"','#include "paper_r1/core.hpp"\n#include "opt2_metrics.hpp"')
edit('crypto.cpp','G1 out;G1::mulVec(out,const_cast<G1*>(g.data()),const_cast<Fr*>(x.data()),x.size());return out;','MSMTrace trace(x.size());G1 out;G1::mulVec(out,const_cast<G1*>(g.data()),const_cast<Fr*>(x.data()),x.size());return out;')
for n in ['hyrax_a3.cpp','oneblock_model.cpp']:
 edit(n,'G1 out;G1::mulVec(out,','MSMTrace trace(x.size());G1 out;G1::mulVec(out,')
edit('streaming_master.cpp','G1 p;G1::mulVec(p,','MSMTrace trace(n);G1 p;G1::mulVec(p,')
edit('classified_commitments.hpp','class Commitments {','struct AuthMetrics {std::uint64_t map_calls=0,map_coordinates=0,kernel_calls=0,kernel_coordinates=0,weights=0,weight_terms=0,weight_products=0,kernel_peak_bytes=0;double map_cpu=0,kernel_cpu=0,weights_cpu=0,partial_cpu=0;};\nclass Commitments {\n friend struct AuthenticationTestAccess;\n mutable AuthMetrics auth_metrics_;')
edit('classified_commitments.hpp',' void registerBase();',' ~Commitments();\n void registerBase();')
edit('classified_commitments.cpp','class Weights {std::vector<Fr>a_,b_;unsigned split_;public:explicit Weights(const std::vector<Fr>&q):split_(q.size()/2){a_=r1::weights({q.begin(),q.begin()+split_});b_=r1::weights({q.begin()+split_,q.end()});}Fr at(std::size_t i)const{return a_[i&(a_.size()-1)]*b_[i>>split_];}};',
'''class Weights {std::vector<Fr>a_,b_;unsigned split_;public:explicit Weights(const std::vector<Fr>&q,AuthMetrics*m=nullptr):split_(q.size()/2){auto t=processCPU();a_=r1::weights({q.begin(),q.begin()+split_});b_=r1::weights({q.begin()+split_,q.end()});if(m){++m->weights;m->weight_terms+=a_.size()+b_.size();m->weights_cpu+=processCPU()-t;}}Fr at(std::size_t i)const{return a_[i&(a_.size()-1)]*b_[i>>split_];}};''')
edit('classified_commitments.cpp','Weights w(c.point);','Weights w(c.point,&auth_metrics_);')
edit('classified_commitments.cpp','Digest Commitments::mapDigest(const Claim&c)const{MapHash h;','Digest Commitments::mapDigest(const Claim&c)const{auto started=processCPU();++auth_metrics_.map_calls;auth_metrics_.map_coordinates+=c.padded;MapHash h;')
edit('classified_commitments.cpp','h.append(l);}return h.finish();}','h.append(l);}auto d=h.finish();auth_metrics_.map_cpu+=processCPU()-started;return d;}')
edit('classified_commitments.cpp',' std::vector<Fr>result(roots_.at(k).spec.padded,Fr(0));',' auto started=processCPU();++auth_metrics_.kernel_calls;auth_metrics_.kernel_peak_bytes=std::max<std::uint64_t>(auth_metrics_.kernel_peak_bytes,roots_.at(k).spec.padded*sizeof(Fr));std::vector<Fr>result(roots_.at(k).spec.padded,Fr(0));')
edit('classified_commitments.cpp','Weights w(c.point,&auth_metrics_);MapHash digest;','auth_metrics_.kernel_coordinates+=c.padded;auth_metrics_.weight_products+=c.padded;Weights w(c.point,&auth_metrics_);MapHash digest;')
edit('classified_commitments.cpp','}return result;','}auth_metrics_.kernel_cpu+=processCPU()-started;if(opt2MetricsEnabled())std::cerr<<"OPT2_KERNEL {\\"root\\":"<<k<<",\\"cpu_seconds\\":"<<processCPU()-started<<"}\\n";return result;')
edit('classified_commitments.cpp','auto rootStarted=Clock::now();Fr v=0;','auto rootStarted=Clock::now();auto partialStarted=processCPU();Fr v=0;')
edit('classified_commitments.cpp','partial.push_back(exchangeHidden','auth_metrics_.partial_cpu+=processCPU()-partialStarted;partial.push_back(exchangeHidden')
edit('classified_commitments.cpp','void Commitments::authenticate(){',r'''Commitments::~Commitments(){if(opt2MetricsEnabled()){auto&m=auth_metrics_;std::cerr<<"OPT2_AUTH {\"map_calls\":"<<m.map_calls<<",\"map_coordinates\":"<<m.map_coordinates<<",\"map_cpu_seconds\":"<<m.map_cpu<<",\"kernel_calls\":"<<m.kernel_calls<<",\"kernel_coordinates\":"<<m.kernel_coordinates<<",\"kernel_cpu_seconds\":"<<m.kernel_cpu<<",\"weights_precomputations\":"<<m.weights<<",\"weights_terms\":"<<m.weight_terms<<",\"weights_cpu_seconds\":"<<m.weights_cpu<<",\"weight_products\":"<<m.weight_products<<",\"partial_cpu_seconds\":"<<m.partial_cpu<<",\"kernel_peak_bytes\":"<<m.kernel_peak_bytes<<"}\n";}}
void Commitments::authenticate(){''')
print('INSTRUMENTED BASELINE WRITTEN')
