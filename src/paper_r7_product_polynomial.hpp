#pragma once
#include "paper_r7_oneblock_sumcheck.hpp"
#include <array>
#include <cstdlib>
namespace zkgpt::r7::oneblock::polynomial {
// Experimental prover arithmetic only. Default retains the original implementation.
inline unsigned mode(){const char*v=std::getenv("ZKGPT_PRODUCT_POLY");if(!v)return 0;need(v[0]>='0' && v[0]<='3' && !v[1],"ZKGPT_PRODUCT_POLY must be 0..3");return unsigned(v[0]-'0');}
template<class F>using Cubic=std::array<F,4>;
template<class F>using Quadratic=std::array<F,3>;
template<class F>Quadratic<F> quadratic(const F&a,const F&b,const F&c,const F&d){
 const F p=a*c,q=(b-a)*(d-c);return {p,b*d-p-q,q};
}
template<class F>Cubic<F> convolve(const Quadratic<F>&q,const F&u,const F&v){return {u*q[0],u*q[1]+v*q[0],u*q[2]+v*q[1],v*q[2]};}
template<class F>Quadratic<F> blend(const Quadratic<F>&a,const Quadratic<F>&b,const std::array<F,2>&alpha){return {alpha[0]*a[0]+alpha[1]*b[0],alpha[0]*a[1]+alpha[1]*b[1],alpha[0]*a[2]+alpha[1]*b[2]};}
template<class F,class Load>Cubic<F> bespoke(std::size_t n,Load load,const std::array<F,2>&alpha){
 Cubic<F>out{F(0),F(0),F(0),F(0)};
 for(std::size_t j=0;j<n/2;++j){
  const F e=load(0,2*j),delta=load(0,2*j+1)-e;
  const auto a=quadratic(alpha[0]*load(1,2*j),alpha[0]*load(1,2*j+1),load(2,2*j),load(2,2*j+1));
  const auto b=quadratic(alpha[1]*load(3,2*j),alpha[1]*load(3,2*j+1),load(4,2*j),load(4,2*j+1));
  const Quadratic<F>q{a[0]+b[0],a[1]+b[1],a[2]+b[2]};
  const auto c=convolve(q,e,delta);for(unsigned k=0;k<4;++k)out[k]+=c[k];
 }return out;
}
template<class F,class Load>Cubic<F> equality(std::size_t n,Load load,const std::array<F,2>&alpha,const F&rho,bool combined){
 std::array<Quadratic<F>,2>sum{{{F(0),F(0),F(0)},{F(0),F(0),F(0)}}};
 for(std::size_t j=0;j<n/2;++j){
  const F w=load(0,2*j)+load(0,2*j+1);
  for(unsigned b=0;b<2;++b){const unsigned l=1+2*b;
   const F a=load(l,2*j),endL=load(l,2*j+1),c=load(l+1,2*j),endR=load(l+1,2*j+1);
   Quadratic<F>q;
   if(combined)q=quadratic(a,endL,c,endR);else{const F dl=endL-a,dr=endR-c;q={a*c,a*dr+dl*c,dl*dr};}
   for(unsigned k=0;k<3;++k)sum[b][k]+=w*q[k];
  }
 }
 const F u=F(1)-rho,v=rho-u;
 if(combined)return convolve(blend(sum[0],sum[1],alpha),u,v);
 const auto a=convolve(sum[0],u,v),b=convolve(sum[1],u,v);Cubic<F>out;
 for(unsigned k=0;k<4;++k){out[k]=alpha[0]*a[k]+alpha[1]*b[k];}return out;
}
template<class F>std::vector<F> weights(const std::vector<F>&r){
 std::vector<F>w(std::size_t(1)<<r.size());w[0]=F(1);std::size_t n=1;
 for(const auto&x:r){for(std::size_t i=0;i<n;++i){w[i+n]=w[i]*x;w[i]=w[i]-w[i+n];}n*=2;}return w;
}
}
