#pragma once
#include "paper_r7_native_workers.hpp"
#include "paper_r7_parallel_fold.hpp"
#include "paper_r7_perf5_profile.hpp"
#include "paper_r7_product_polynomial.hpp"

namespace zkgpt::r7::oneblock::detail {
// Work units differ by kernel. With a configured 32-thread pool, small
// dispatches activate only their participating partitions.
inline constexpr std::size_t product_polynomial_minimum=1024;
inline constexpr std::size_t product_single_polynomial_minimum=8192;
inline constexpr std::size_t product_tree_minimum=2048;
inline constexpr std::size_t product_first_fold_minimum=1024;
inline constexpr std::size_t product_later_fold_minimum=1024;
inline constexpr std::size_t product_leaf_minimum=1024;
inline constexpr std::size_t product_weight_minimum=8192;
inline constexpr std::size_t product_fingerprint_minimum=2048;

inline unsigned productPartitions(std::size_t count, const NativeWorkers& workers, std::size_t minimum){
 const auto usable=nativePartitions(count,workers,minimum);
 unsigned partitions=1;
 while(partitions<=usable/2)partitions*=2;
 return partitions;
}

template<class Work>
unsigned productDispatch(std::size_t units,unsigned partitions,NativeWorkers& workers,
                         perf5::Kernel kernel,const Work& work){
 perf5::WorkerDispatch profile(kernel,units,partitions);
 workers.run(partitions,[&](std::size_t i){
  profile.participant(i,[&]{work(i);});
 },partitions>1);
 return partitions;
}

template<class Work>
unsigned productRanges(std::size_t count,NativeWorkers& workers,std::size_t minimum,
                       perf5::Kernel kernel,const Work& work){
 const auto partitions=productPartitions(count,workers,minimum);
 return productDispatch(count,partitions,workers,kernel,[&](std::size_t i){
  work(i,count*i/partitions,count*(i+1)/partitions);
 });
}

inline std::vector<Fr> productEqualityWeights(const std::vector<Fr>& point,NativeWorkers& workers){
 const auto n=std::size_t(1)<<point.size();
 const auto blocks=productPartitions(n,workers,product_weight_minimum);
 if(blocks==1)return polynomial::weights(point);
 unsigned low_bits=point.size();
 for(auto high=blocks;high>1;high/=2)--low_bits;
 const auto seeds=polynomial::weights(std::vector<Fr>(point.begin()+low_bits,point.end()));
 std::vector<Fr> weights(n);
 const auto block_size=n/blocks;
 productRanges(n,workers,product_weight_minimum,perf5::Kernel::Weights,[&](std::size_t k,std::size_t first,std::size_t){
  auto* out=weights.data()+first;
  out[0]=seeds[k];
  for(std::size_t width=1,bit=0;width<block_size;width*=2,++bit){
   for(std::size_t i=0;i<width;++i){
    out[width+i]=out[i]*point[bit];
    out[i]-=out[width+i];
   }
  }
 });
 return weights;
}


template<std::size_t N> struct alignas(64) ProductPartial {
 std::array<Fr,N> coefficients;
 ProductPartial(){coefficients.fill(Fr(0));}
};

template<std::size_t N,class Accumulate>
std::array<Fr,N> reduceProductPairs(std::size_t pairs,NativeWorkers& workers,const Accumulate& accumulate,
                                  std::uint64_t mul_per_pair,std::uint64_t add_per_pair,
                                  std::uint64_t mul_final,std::uint64_t add_final,std::size_t minimum){
 std::array<ProductPartial<N>,NativeWorkers::capacity> partial;
 const auto partitions=productRanges(pairs,workers,minimum,
   N==4?perf5::Kernel::SinglePolynomial:perf5::Kernel::BatchPolynomial,[&](std::size_t k,std::size_t first,std::size_t last){
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
 },9,11,0,0,product_single_polynomial_minimum);
}

template<class Load>
polynomial::Cubic<Fr> batchProductCoefficients(std::size_t width,const Load& load,const std::array<Fr,2>& alpha,
                                               const Fr* rho,NativeWorkers& workers,const Fr& equality_prefix=Fr(1)){
 if(rho){
  const auto sums=reduceProductPairs<6>(width/2,workers,[&](auto& sum,std::size_t i){
   const Fr shared=load(0,i)+load(0,i+1);
   for(unsigned b=0;b<2;++b){
    const auto k=1+2*b;
    const auto child=polynomial::quadratic(load(k,i),load(k,i+1),load(k+1,i),load(k+1,i+1));
    for(unsigned j=0;j<3;++j)sum[3*b+j]+=shared*child[j];
   }
  },12,15,14,7,product_polynomial_minimum);
  const polynomial::Quadratic<Fr> left{sums[0],sums[1],sums[2]},right{sums[3],sums[4],sums[5]};
  const Fr constant=Fr(1)-*rho;
  return polynomial::convolve(polynomial::blend(left,right,alpha),
                              equality_prefix*constant,equality_prefix*(*rho-constant));
 }
 const auto sums=reduceProductPairs<8>(width/2,workers,[&](auto& sum,std::size_t i){
  const Fr equality=load(0,i),delta=load(0,i+1)-equality;
  for(unsigned b=0;b<2;++b){
   const auto k=1+2*b;
   const auto child=polynomial::quadratic(load(k,i),load(k,i+1),load(k+1,i),load(k+1,i+1));
   const auto coefficients=polynomial::convolve(child,equality,delta);
   for(unsigned j=0;j<4;++j)sum[4*b+j]+=coefficients[j];
  }
 },18,21,8,4,product_polynomial_minimum);
 polynomial::Cubic<Fr> out;
 for(unsigned j=0;j<4;++j)out[j]=alpha[0]*sums[j]+alpha[1]*sums[j+4];
 return out;
}

template<class Fold>
unsigned productFoldRanges(std::size_t factors,std::size_t begin,std::size_t end,NativeWorkers& workers,
                           std::size_t minimum,perf5::Kernel kernel,const Fold& fold){
 const auto width=end-begin;
 return productRanges(factors*width,workers,minimum,kernel,
   [&](std::size_t,std::size_t first,std::size_t last){
    for(auto k=first/width;k<factors && k*width<last;++k){
     const auto lo=std::max(first,k*width)-k*width;
     const auto hi=std::min(last,(k+1)*width)-k*width;
     fold(k,begin+lo,begin+hi);
    }
   });
}

template<class Load>
void foldNativeProduct(std::size_t width,const Fr& challenge,std::vector<std::vector<Fr>>& folded,
                       bool first,const Load& load,NativeWorkers& workers,bool sum_first_factor=false){
 const auto next=width/2;
 const auto factors=folded.size();
 std::size_t jobs=0;
 if(!next || !factors)return;
 if(first){
  for(auto& factor:folded)factor.resize(next);
  const auto count=productFoldRanges(factors,0,next,workers,product_first_fold_minimum,perf5::Kernel::FirstFold,
   [&](std::size_t k,std::size_t begin,std::size_t end){
    if(sum_first_factor && k==0){
     for(auto j=begin;j<end;++j)folded[k][j]=load(k,2*j)+load(k,2*j+1);
    }else for(auto j=begin;j<end;++j){
     const Fr x=load(k,2*j);
     folded[k][j]=x+challenge*(load(k,2*j+1)-x);
    }
   });
  if(count>1)jobs+=count;
 }else{
  const auto fold=[&](std::size_t k,std::size_t begin,std::size_t end){
   auto* values=folded[k].data();
   if(sum_first_factor && k==0){
    for(auto j=begin;j<end;++j)values[j]=values[2*j]+values[2*j+1];
   }else for(auto j=begin;j<end;++j)values[j]=values[2*j]+challenge*(values[2*j+1]-values[2*j]);
  };
  if(productPartitions(factors*next,workers,product_later_fold_minimum)==1){
   productDispatch(factors*next,1,workers,perf5::Kernel::LaterFold,[&](std::size_t){
    for(std::size_t k=0;k<factors;++k)fold(k,0,next);
   });
  }else{
   unified::detail::foldDependencyWaves(next,unified::detail::fold_serial_prefix,
    [&](std::size_t begin,std::size_t end,bool per_factor){
     unsigned count;
     if(per_factor){
      const auto factor_grain=1+(product_later_fold_minimum-1)/(end-begin);
      count=productPartitions(factors,workers,factor_grain);
      productDispatch(factors*(end-begin),count,workers,perf5::Kernel::LaterFold,[&](std::size_t i){
       for(auto k=factors*i/count;k<factors*(i+1)/count;++k)fold(k,begin,end);
      });
     }else count=productFoldRanges(factors,begin,end,workers,product_later_fold_minimum,perf5::Kernel::LaterFold,fold);
     if(count>1)jobs+=count;
    });
  }
  for(auto& factor:folded)factor.resize(next);
 }
 if(auto* metrics=perf5::context){
  metrics->parallel_fold_jobs+=jobs;
  const auto ordinary=factors-(sum_first_factor?1:0);
  metrics->fold_mul+=ordinary*next;
  metrics->fold_add+=(factors+ordinary)*next;
 }
}

template<std::size_t N,class Load>
std::array<Fr,N> productLeafDots(const std::vector<Fr>& weights,const Load& load,NativeWorkers& workers){
 perf5::Scope scope(perf5::LeafDot);
 std::array<ProductPartial<N>,NativeWorkers::capacity> partial;
 const auto count=productRanges(weights.size(),workers,product_leaf_minimum,perf5::Kernel::LeafDot,[&](std::size_t k,std::size_t first,std::size_t last){
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
 const auto partitions=productRanges(count*trees,workers,product_tree_minimum,perf5::Kernel::Tree,[&](std::size_t,std::size_t first,std::size_t last){
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

template<class Load>
std::array<Fr,2> productFingerprintPair(std::vector<Fr>& leaves,const Load& fingerprint,NativeWorkers& workers){
 struct alignas(64) Roots {Fr write=1,read=1;};
 std::array<Roots,NativeWorkers::capacity> partial;
 const auto count=productRanges(leaves.size(),workers,product_fingerprint_minimum,perf5::Kernel::Fingerprint,
   [&](std::size_t worker,std::size_t first,std::size_t last){
    auto& roots=partial[worker];
    const Fr one=1;
    for(auto i=first;i<last;++i){
     const Fr value=fingerprint(i);
     leaves[i]=value;
     roots.write*=value+one;
     roots.read*=value;
    }
   });
 std::array<Fr,2> roots{Fr(1),Fr(1)};
 for(unsigned i=0;i<count;++i){roots[0]*=partial[i].write;roots[1]*=partial[i].read;}
 return roots;
}

template<class Load>
std::array<Fr,2> productFinalRoots(const std::vector<Fr>& table,std::vector<Fr>* leaves,
                                  const Fr& gamma,const Fr& gamma2,const Fr& tau,const Load& count,
                                  NativeWorkers& workers){
 struct alignas(64) Roots {Fr initial=1,final=1;};
 std::array<Roots,NativeWorkers::capacity> partial;
 const auto partitions=productRanges(table.size(),workers,product_fingerprint_minimum,perf5::Kernel::Fingerprint,
   [&](std::size_t worker,std::size_t first,std::size_t last){
    auto& product=partial[worker];
    for(auto i=first;i<last;++i){
     const Fr value=gamma2*Fr(i)+gamma*table[i]-tau;
     product.initial*=value;
     if(leaves){
      const Fr final=value+count(i);
      (*leaves)[i]=final;
      product.final*=final;
     }
    }
   });
 std::array<Fr,2> products{Fr(1),Fr(1)};
 for(unsigned i=0;i<partitions;++i){products[0]*=partial[i].initial;products[1]*=partial[i].final;}
 return products;
}

inline unsigned productPairedLeaves(const std::vector<Fr>& read,std::vector<Fr>& write,
                                    Fr* write_parent,Fr* read_parent,NativeWorkers& workers){
 const Fr one=1;
 if(!write_parent){
  for(std::size_t i=0;i<read.size();++i)write[i]=read[i]+one;
  return 1;
 }
 return productRanges(read.size()/2,workers,product_tree_minimum,perf5::Kernel::PairedLeafParent,
   [&](std::size_t,std::size_t first,std::size_t last){
    for(auto j=first;j<last;++j){
     const Fr a=read[2*j],b=read[2*j+1];
     const Fr wa=a+one,wb=b+one;
     write[2*j]=wa;write[2*j+1]=wb;
     write_parent[j]=wa*wb;
     read_parent[j]=a*b;
    }
   });
}

inline Fr readProductInput(const ProductInputs& input,unsigned k,std::size_t i){
 return k?input.children[2*i+k-1]:input.equality[i];
}

inline Fr readProductInput(const std::array<ProductInputs,2>& input,unsigned k,std::size_t i){
 return k?input[(k-1)/2].children[2*i+(k-1)%2]:input[0].equality[i];
}

inline polynomial::Cubic<Fr> batchProductRound(std::size_t width,unsigned round,
    const std::vector<Value>& factors,const std::vector<std::vector<Fr>>& folded,
    const std::array<ProductInputs,2>* inputs,const std::array<Fr,2>& alpha,const Fr* rho,
    const Fr& equality_prefix){
 if(auto* metrics=perf5::context){
  metrics->poly_mode=rho?3:0;
  metrics->a2_calls+=2;metrics->a2_pairs+=width;
  if(!round){
   if(inputs){metrics->c2_polynomial_calls+=2;metrics->c2_polynomial_pairs+=width;}
   else metrics->poly_callbacks+=5*width;
  }
 }
 auto& workers=nativeWorkers();
 if(round)return batchProductCoefficients(width,[&](unsigned k,std::size_t i){return folded[k][i];},alpha,rho,workers,equality_prefix);
 if(inputs)return batchProductCoefficients(width,[&](unsigned k,std::size_t i){return readProductInput(*inputs,k,i);},alpha,rho,workers,equality_prefix);
 return batchProductCoefficients(width,[&](unsigned k,std::size_t i){return factors[k](i);},alpha,rho,workers,equality_prefix);
}

inline void foldBatchedProductRound(std::size_t width,unsigned round,const Fr& challenge,
    const std::vector<Value>& factors,std::vector<std::vector<Fr>>& folded,const std::array<ProductInputs,2>* inputs,
    const Fr* rho,Fr& equality_prefix){
 perf5::Scope folding(perf5::Fold);
 perf5::Scope round_time(round?perf5::FoldLater:perf5::FoldFirst);
 if(!round && perf5::context){
  if(inputs){perf5::context->c2_fold_calls+=2;perf5::context->c2_fold_pairs+=width;}
  else perf5::context->fold_callbacks+=5*width;
 }
 const auto load=[&](unsigned k,std::size_t i){return inputs?readProductInput(*inputs,k,i):factors[k](i);};
 // The suffix factors sum to one along the folded bit. Apply its challenge
 // factor once in the prefix, avoiding a field multiplication per coordinate.
 foldNativeProduct(width,challenge,folded,!round,load,nativeWorkers(),rho!=nullptr);
 if(rho){
  const Fr low=Fr(1)-*rho;
  equality_prefix*=low+challenge*(*rho-low);
  if(auto* metrics=perf5::context){metrics->fold_mul+=2;metrics->fold_add+=3;}
 }
}
}
