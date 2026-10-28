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
