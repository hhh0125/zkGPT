#pragma once
#include "paper_r7_native_workers.hpp"
#include "paper_r7_parallel_fold.hpp"
#include "paper_r7_perf5_profile.hpp"
#include "paper_r7_product_polynomial.hpp"

namespace zkgpt::r7::oneblock::detail {
inline constexpr std::size_t product_parallel_minimum=8192;

template<std::size_t N> struct alignas(64) ProductPartial {
 std::array<Fr,N> coefficients;
 ProductPartial(){coefficients.fill(Fr(0));}
};

template<std::size_t N,class Accumulate>
std::array<Fr,N> reduceProductPairs(std::size_t pairs,NativeWorkers& workers,const Accumulate& accumulate,
                                  std::uint64_t mul_per_pair,std::uint64_t add_per_pair,
                                  std::uint64_t mul_final,std::uint64_t add_final){
 const auto partitions=nativePartitions(pairs,workers,product_parallel_minimum);
 std::array<ProductPartial<N>,NativeWorkers::capacity> partial;
 nativeRanges(pairs,workers,product_parallel_minimum,[&](std::size_t k,std::size_t first,std::size_t last){
  auto& sum=partial[k].coefficients;
  for(auto j=first;j<last;++j)accumulate(sum,2*j);
 });
 auto& out=partial[0].coefficients;
 {
  perf5::Scope measure(perf5::PolynomialReduction);
  for(unsigned k=1;k<partitions;++k)for(unsigned i=0;i<N;++i)out[i]+=partial[k].coefficients[i];
 }
 if(auto* metrics=perf5::context){
  ++metrics->poly_rounds;metrics->poly_pairs+=pairs;
  metrics->poly_mul+=mul_per_pair*pairs+mul_final;
  metrics->poly_add+=add_per_pair*pairs+add_final+N*(partitions-1);
  metrics->poly_workers=std::max(metrics->poly_workers,partitions);
  if(partitions>1)++metrics->parallel_poly_rounds;
 }
 return out;
}

template<class Load>
polynomial::Cubic<Fr> singleProductCoefficients(std::size_t width,const Load& load,NativeWorkers& workers){
 return reduceProductPairs<4>(width/2,workers,[&](auto& sum,std::size_t i){
  const Fr equality=load(0,i);
  const auto child=polynomial::quadratic(load(1,i),load(1,i+1),load(2,i),load(2,i+1));
  const auto coefficients=polynomial::convolve(child,equality,load(0,i+1)-equality);
  for(unsigned k=0;k<4;++k)sum[k]+=coefficients[k];
 },9,11,0,0);
}

template<class Load>
polynomial::Cubic<Fr> batchProductCoefficients(std::size_t width,const Load& load,const std::array<Fr,2>& alpha,
                                               const Fr* rho,NativeWorkers& workers){
 if(rho){
  const auto sums=reduceProductPairs<6>(width/2,workers,[&](auto& sum,std::size_t i){
   const Fr shared=load(0,i)+load(0,i+1);
   for(unsigned b=0;b<2;++b){
    const auto k=1+2*b;
    const auto child=polynomial::quadratic(load(k,i),load(k,i+1),load(k+1,i),load(k+1,i+1));
    for(unsigned j=0;j<3;++j)sum[3*b+j]+=shared*child[j];
   }
  },12,15,12,7);
  const polynomial::Quadratic<Fr> left{sums[0],sums[1],sums[2]},right{sums[3],sums[4],sums[5]};
  const Fr constant=Fr(1)-*rho;
  return polynomial::convolve(polynomial::blend(left,right,alpha),constant,*rho-constant);
 }
 const auto sums=reduceProductPairs<8>(width/2,workers,[&](auto& sum,std::size_t i){
  const Fr equality=load(0,i),delta=load(0,i+1)-equality;
  for(unsigned b=0;b<2;++b){
   const auto k=1+2*b;
   const auto child=polynomial::quadratic(load(k,i),load(k,i+1),load(k+1,i),load(k+1,i+1));
   const auto coefficients=polynomial::convolve(child,equality,delta);
   for(unsigned j=0;j<4;++j)sum[4*b+j]+=coefficients[j];
  }
 },18,21,8,4);
 polynomial::Cubic<Fr> out;
 for(unsigned j=0;j<4;++j)out[j]=alpha[0]*sums[j]+alpha[1]*sums[j+4];
 return out;
}

template<class Load>
void foldNativeProduct(std::size_t width,const Fr& challenge,std::vector<std::vector<Fr>>& folded,
                       bool first,const Load& load,NativeWorkers& workers){
 const auto next=width/2;
 std::vector<std::size_t> counts(folded.size(),next);
 std::size_t jobs;
 if(first){
  for(auto& factor:folded)factor.resize(next);
  jobs=unified::detail::foldRanges(counts,0,next,workers,false,[&](std::size_t k,std::size_t begin,std::size_t end){
   for(auto j=begin;j<end;++j){
    const Fr x=load(k,2*j);
    folded[k][j]=x+challenge*(load(k,2*j+1)-x);
   }
  });
 }else{
  jobs=unified::detail::foldInPlace(counts,workers,[&](std::size_t k,std::size_t begin,std::size_t end){
   auto* values=folded[k].data();
   for(auto j=begin;j<end;++j)values[j]=values[2*j]+challenge*(values[2*j+1]-values[2*j]);
  });
  for(auto& factor:folded)factor.resize(next);
 }
 if(auto* metrics=perf5::context){
  metrics->parallel_fold_jobs+=workers.count()>1?jobs:0;
  metrics->fold_mul+=folded.size()*next;
  metrics->fold_add+=2*folded.size()*next;
 }
}

template<std::size_t N,class Load>
std::array<Fr,N> productLeafDots(const std::vector<Fr>& weights,const Load& load,NativeWorkers& workers){
 perf5::Scope scope(perf5::LeafDot);
 std::array<ProductPartial<N>,NativeWorkers::capacity> partial;
 const auto count=nativeRanges(weights.size(),workers,product_parallel_minimum,[&](std::size_t k,std::size_t first,std::size_t last){
  auto& sum=partial[k].coefficients;
  for(auto i=first;i<last;++i){
   const auto& weight=weights[i];
   for(unsigned j=0;j<N;++j)sum[j]+=weight*load(j,i);
  }
 });
 auto out=partial[0].coefficients;
 for(unsigned k=1;k<count;++k)for(unsigned j=0;j<N;++j)out[j]+=partial[k].coefficients[j];
 return out;
}

inline void productTreeParents(std::size_t count,const std::array<const Fr*,2>& child,
                               const std::array<Fr*,2>& parent,NativeWorkers& workers){
 const auto trees=parent[1]?2:1;
 const auto partitions=nativeRanges(count*trees,workers,product_parallel_minimum,[&](std::size_t,std::size_t first,std::size_t last){
  for(auto k=first/count;k<std::size_t(trees) && k*count<last;++k){
   const auto begin=std::max(first,k*count)-k*count,end=std::min(last,(k+1)*count)-k*count;
   const auto* in=child[k];auto* out=parent[k];
   for(auto j=begin;j<end;++j)out[j]=in[2*j]*in[2*j+1];
  }
 });
 if(auto* metrics=perf5::context){
  metrics->tree_workers=std::max(metrics->tree_workers,partitions);
  if(partitions>1)++metrics->parallel_tree_layers;
 }
}

inline Fr readProductInput(const ProductInputs& input,unsigned k,std::size_t i){
 return k?input.children[2*i+k-1]:input.equality[i];
}

inline Fr readProductInput(const std::array<ProductInputs,2>& input,unsigned k,std::size_t i){
 return k?input[(k-1)/2].children[2*i+(k-1)%2]:input[0].equality[i];
}

inline polynomial::Cubic<Fr> batchProductRound(std::size_t width,unsigned round,
    const std::vector<Value>& factors,const std::vector<std::vector<Fr>>& folded,
    const std::array<ProductInputs,2>* inputs,const std::array<Fr,2>& alpha,const Fr* rho){
 if(auto* metrics=perf5::context){
  metrics->poly_mode=rho?3:0;
  metrics->a2_calls+=2;metrics->a2_pairs+=width;
  if(!round){
   if(inputs){metrics->c2_polynomial_calls+=2;metrics->c2_polynomial_pairs+=width;}
   else metrics->poly_callbacks+=5*width;
  }
 }
 auto& workers=nativeWorkers();
 if(round)return batchProductCoefficients(width,[&](unsigned k,std::size_t i){return folded[k][i];},alpha,rho,workers);
 if(inputs)return batchProductCoefficients(width,[&](unsigned k,std::size_t i){return readProductInput(*inputs,k,i);},alpha,rho,workers);
 return batchProductCoefficients(width,[&](unsigned k,std::size_t i){return factors[k](i);},alpha,rho,workers);
}

inline void foldBatchedProductRound(std::size_t width,unsigned round,const Fr& challenge,
    const std::vector<Value>& factors,std::vector<std::vector<Fr>>& folded,const std::array<ProductInputs,2>* inputs){
 perf5::Scope folding(perf5::Fold);
 perf5::Scope round_time(round?perf5::FoldLater:perf5::FoldFirst);
 if(!round && perf5::context){
  if(inputs){perf5::context->c2_fold_calls+=2;perf5::context->c2_fold_pairs+=width;}
  else perf5::context->fold_callbacks+=5*width;
 }
 const auto load=[&](unsigned k,std::size_t i){return inputs?readProductInput(*inputs,k,i):factors[k](i);};
 foldNativeProduct(width,challenge,folded,!round,load,nativeWorkers());
}
}
