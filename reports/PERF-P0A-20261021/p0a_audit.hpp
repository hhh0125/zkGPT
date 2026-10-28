#pragma once
#include <chrono>
#include <map>
#include <string>
#include <cstdio>
#include <vector>
namespace perf_audit {
using Clock=std::chrono::steady_clock;
struct Stat {unsigned long long calls=0; double inc=0,exc=0;};
struct Store {std::map<std::string,Stat> rows; ~Store(){for(auto &p:rows)std::fprintf(stderr,"AUDIT %s calls=%llu inclusive=%.9f exclusive=%.9f\n",p.first.c_str(),p.second.calls,p.second.inc,p.second.exc);}};
inline Store &store(){static Store x;return x;}
struct Scope;
inline std::vector<Scope*> &stack(){static thread_local std::vector<Scope*> s;return s;}
struct Scope {std::string name; Clock::time_point begin;double child=0;
 explicit Scope(std::string n):name(std::move(n)),begin(Clock::now()){(void)store();stack().push_back(this);}
 ~Scope(){double dt=std::chrono::duration<double>(Clock::now()-begin).count();stack().pop_back();if(!stack().empty())stack().back()->child+=dt;auto &v=store().rows[name];++v.calls;v.inc+=dt;v.exc+=dt-child;}
};
}

#include <atomic>
#include <mcl/bn256.hpp>
namespace p0a_count {
using G1=mcl::bn::G1;
struct Counts {std::atomic<unsigned long long> actual{0},skipped{0},fast_points{0},fast_decodes{0};
 ~Counts(){std::fprintf(stderr,"P0A_CHECKS actual_order=%llu logical_order_skipped=%llu fast_points=%llu fast_decodes=%llu\n",actual.load(),skipped.load(),fast_points.load(),fast_decodes.load());}};
inline Counts &counts(){static Counts c;return c;}
inline decltype(G1::isValidOrderFast) &prior(){static decltype(G1::isValidOrderFast) p=nullptr;return p;}
inline bool countedOrder(const G1 &p){counts().actual.fetch_add(1,std::memory_order_relaxed);if(prior())return prior()(p);G1 q;G1::mulGeneric(q,p,G1::order_);return q.isZero();}
inline void install(){(void)counts();if(G1::isValidOrderFast!=countedOrder){prior()=G1::isValidOrderFast;G1::isValidOrderFast=countedOrder;}}
inline void pointSkip(){counts().fast_points.fetch_add(1,std::memory_order_relaxed);counts().skipped.fetch_add(1+(G1::verifyOrder_?1:0),std::memory_order_relaxed);}
inline void decodeSkip(bool zero){counts().fast_decodes.fetch_add(1,std::memory_order_relaxed);if(!zero&&G1::verifyOrder_)counts().skipped.fetch_add(1,std::memory_order_relaxed);}
}
