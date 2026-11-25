#pragma once
#include "paper_r7_native_workers.hpp"
#include "paper_r7_parallel_fold.hpp"
#include "paper_r7_product_polynomial.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>

namespace zkgpt::r7::oneblock::detail {
// Owns only the two contracted one-dimensional factors. Their storage is
// reused in place once each round's challenge has been generated.
struct MatrixArithmetic {
 using Clock=std::chrono::steady_clock;
 std::array<std::vector<Fr>,2>values;
 std::array<double,2>contraction{};
 std::uint64_t source_terms=0,direct_terms=0;
 std::array<std::size_t,2>contraction_workers{};
 double polynomial_seconds=0,first_fold_seconds=0,fold_seconds=0;
 std::size_t polynomial_workers=1,fold_jobs=0,rounds=0;

 static double seconds(Clock::time_point start){return std::chrono::duration<double>(Clock::now()-start).count();}

 void materialize(std::size_t width,const std::vector<Fr>&source,
                  const std::vector<std::uint32_t>&a,const std::vector<Fr>&rows,
                  const std::vector<std::uint32_t>&b,const std::vector<Fr>&columns){
  contract(0,width,source,a,rows);
  contract(1,width,source,b,columns);
 }

 std::array<Fr,3> polynomial(){
  const auto start=Clock::now();
  auto&workers=nativeWorkers();
  struct alignas(64) Accumulator {std::array<Fr,3>coefficients{Fr(0),Fr(0),Fr(0)};};
  std::array<Accumulator,16>sums;
  const auto*left=values[0].data();
  const auto*right=values[1].data();
  const auto used=nativeRanges(values[0].size()/2,workers,1024,[&](std::size_t worker,std::size_t first,std::size_t last){
   auto&coefficients=sums[worker].coefficients;
   for(std::size_t j=first;j<last;++j){
    const auto i=2*j;
    auto product=oneblock::polynomial::quadratic(left[i],left[i+1],right[i],right[i+1]);
    for(unsigned k=0;k<3;++k)coefficients[k]+=product[k];
   }
  });
  auto result=sums[0].coefficients;
  for(std::size_t w=1;w<used;++w)for(unsigned k=0;k<3;++k)result[k]+=sums[w].coefficients[k];
  polynomial_workers=std::max(polynomial_workers,std::size_t(used));
  polynomial_seconds+=seconds(start);
  return result;
 }

 void fold(const Fr&challenge){
  const auto start=Clock::now();
  const auto half=values[0].size()/2;
  fold_jobs+=unified::detail::foldInPlace({half,half},nativeWorkers(),[&](std::size_t factor,std::size_t first,std::size_t last){
   auto*data=values[factor].data();
   for(std::size_t j=first;j<last;++j)data[j]=data[2*j]+challenge*(data[2*j+1]-data[2*j]);
  });
  for(auto&factor:values)factor.resize(half);
  if(rounds++==0)first_fold_seconds+=seconds(start);
  else fold_seconds+=seconds(start);
 }

 void report(const std::string&role,double wall)const{
  std::cerr<<std::setprecision(12)<<"GKR_MATRIX_ARITHMETIC {\"role\":"<<std::quoted(role)
   <<",\"factor_a_seconds\":"<<contraction[0]<<",\"factor_b_seconds\":"<<contraction[1]
   <<",\"materialization_including_witness_reads_seconds\":"<<contraction[0]+contraction[1]
   <<",\"source_terms\":"<<source_terms<<",\"direct_source_terms\":"<<direct_terms
   <<",\"factor_a_workers\":"<<contraction_workers[0]<<",\"factor_b_workers\":"<<contraction_workers[1]
   <<",\"polynomial_workers\":"<<polynomial_workers<<",\"polynomial_seconds\":"<<polynomial_seconds
   <<",\"first_fold_seconds\":"<<first_fold_seconds<<",\"later_fold_seconds\":"<<fold_seconds
   <<",\"parallel_fold_jobs\":"<<fold_jobs<<",\"factor_coordinates\":"<<(std::uint64_t(2)<<rounds)
   <<",\"eliminated_contraction_multiplications\":"<<(rounds?source_terms:0)
   <<",\"total_seconds\":"<<wall<<"}\n";
 }

 private:
 void contract(std::size_t factor,std::size_t width,const std::vector<Fr>&source,
               const std::vector<std::uint32_t>&indices,const std::vector<Fr>&weights){
  need(width && indices.size()%width==0 && indices.size()/width==weights.size(),"Native Matrix contraction dimensions");
  const auto started=Clock::now();
  auto&output=values[factor];output.assign(width,Fr(0));
  constexpr std::size_t tile=128;
  const auto tiles=(width+tile-1)/tile;
  struct alignas(64) Counter {std::uint64_t direct=0;};
  std::array<Counter,16>counts{};
  auto&workers=nativeWorkers();
  const auto used=nativeRanges(tiles,workers,2,[&](std::size_t worker,std::size_t first,std::size_t last){
   auto&direct=counts[worker].direct;
   for(auto block=first;block<last;++block){
    const auto begin=block*tile,end=std::min(width,begin+tile),count=end-begin;
    auto*dest=output.data()+begin;
    for(std::size_t row=0;row<weights.size();++row){
     const auto weight=weights[row];
     const auto*positions=indices.data()+row*width+begin;
     const std::size_t offset=positions[0];
     bool contiguous=offset<=source.size() && count<=source.size()-offset;
     for(std::size_t j=1;j<count && contiguous;++j)contiguous=positions[j]==offset+j;
     if(contiguous){
      const auto*input=source.data()+offset;
      for(std::size_t j=0;j<count;++j)dest[j]+=weight*input[j];
      direct+=count;
     }else{
      for(std::size_t j=0;j<count;++j)dest[j]+=weight*source.at(positions[j]);
     }
    }
   }
  });
  for(std::size_t w=0;w<used;++w)direct_terms+=counts[w].direct;
  source_terms+=indices.size();
  contraction_workers[factor]=used;
  contraction[factor]=seconds(started);
 }
};
}
