#pragma once
#include "host_safety.hpp"
#include "circuit.h"
namespace zkgpt::r5b {
using I=__int128;
// Exact nearest integer with ties to +infinity, including negative ties.
inline I round(I n,I d){
 if(d<=0)throw std::range_error("R5B nonpositive rounding denominator");
 I q=host_safety::divide(n,d),r=n%d;
 if(r<0){q=host_safety::sub(q,1);r=host_safety::add(r,d);}
 return r>=d/2+d%2?host_safety::add(q,1):q;
}
inline I root(I n){
 if(n<0)throw std::range_error("R5B negative radicand");
 // Division-based integer binary search never squares a large midpoint.
 __uint128_t lo=0,hi=__uint128_t(1)<<64,nn=__uint128_t(n);
 while(lo+1<hi){auto m=lo+(hi-lo)/2;if(m<=nn/m)lo=m;else hi=m;}
 return I(lo);
}
inline I remainder(I n,I d,I q){return host_safety::sub(host_safety::add(host_safety::mul(2,n),d),host_safety::mul(host_safety::mul(2,d),q));}
// Shared with the R4 attained-max constraints. All selector/gap coordinates
// enter the existing registry, production Lasso and common-input proof.
inline void attained(layer &c,int select_check,int gap_check,int selector,int gap){
 c.uni_gates.emplace_back(select_check,selector,0,1);
 c.bin_gates.emplace_back(gap_check,selector,gap,1,0);
}
struct Gates {
 layer &c;int one,row=0;
 int next(){return row++;}
 void u(int g,int index,I coefficient,int source=0){c.uni_gates.emplace_back(g,index,source,coefficient);}
 void b(int g,int a,int b,I coefficient=1){c.bin_gates.emplace_back(g,a,b,coefficient,0);}
 void constant(int g,I coefficient){u(g,one,coefficient);}
};
}
