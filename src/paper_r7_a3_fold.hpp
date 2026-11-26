#pragma once
#include "bounded_workers.hpp"
#include "paper_r7_crypto.hpp"
#include "paper_r7_perf5_profile.hpp"
#include <optional>

namespace zkgpt::r7::hyrax_a3::detail {

// These digits are valid only for public Fiat-Shamir challenges. Reuse
// MCL's BN254 GLV split and signed window algorithm, then normalize each
// bounded table batch together before applying the shared digits.
struct PreparedPublicScalars {
 using G1=crypto::G1;
 using Fr=crypto::Fr;
 using GLV=mcl::bn::local::GLV1;
 using Digits=mcl::FixedArray<int8_t,sizeof(Fr)*4+2>;
 static constexpr unsigned table_size=8;
 Digits digits[2][2];
 std::size_t width=0;
 explicit PreparedPublicScalars(const Fr (&values)[2]){
  for(unsigned i=0;i<2;++i){
   mpz_class scalar,split[2];
   mcl::fp::getMpzAtT<Fr>(scalar,values,i);
   GLV::split(split,scalar);
   for(unsigned j=0;j<2;++j){
    bool ok;
    mcl::gmp::getNAFwidth(&ok,digits[i][j],split[j],5);
    crypto::need(ok,"public IPA scalar window");
    width=std::max(width,digits[i][j].size());
   }
  }
 }
 static constexpr std::size_t batch_size=8;
 void apply(G1* output,const G1* left,const G1* right,std::size_t count)const{
  std::array<G1,2*batch_size*table_size> points,images;
  for(std::size_t i=0;i<count;++i)for(unsigned side=0;side<2;++side){
   auto* table=points.data()+(2*i+side)*table_size;
   table[0]=(side?right:left)[i];
   G1 step;G1::dbl(step,table[0]);
   for(unsigned j=1;j<table_size;++j)G1::add(table[j],table[j-1],step);
  }
  const auto entries=2*count*table_size;
  G1::normalizeVec(points.data(),points.data(),entries);
  for(std::size_t i=0;i<entries;++i)GLV::mulLambda(images[i],points[i]);
  for(std::size_t i=0;i<count;++i){
   G1 result;result.clear();
   for(auto bit=width;bit-->0;){
    G1::dbl(result,result);
    for(unsigned side=0;side<2;++side){
     const auto at=(2*i+side)*table_size;
     mcl::ec::local::addTbl(result,points.data()+at,digits[side][0],bit);
     mcl::ec::local::addTbl(result,images.data()+at,digits[side][1],bit);
    }
   }
   output[i]=result;
  }
 }
};

inline unsigned foldGeneratorWorkers(std::size_t half, unsigned capacity){
 // Point linear combinations are substantially heavier than scalar-field
 // folds. Keep small final rounds local and at least eight points per worker.
 const unsigned limit=half>=512?16:8;
 return static_cast<unsigned>(std::min<std::size_t>({half/8,capacity,limit}));
}

inline void foldGenerators(crypto::G1* points, std::size_t half,
                           const crypto::Fr& inverse, const crypto::Fr& challenge,
                           zkgpt::Workers* workers){
 const crypto::Fr scalars[2]{inverse,challenge};
 std::optional<PreparedPublicScalars> prepared;
 if(half>=512)prepared.emplace(scalars);
 const auto fold=[&](std::size_t begin,std::size_t end){
  if(prepared){
   for(auto i=begin;i<end;i+=PreparedPublicScalars::batch_size)
    prepared->apply(points+i,points+i,points+half+i,std::min(end-i,PreparedPublicScalars::batch_size));
   return;
  }
  for(auto i=begin;i<end;++i){
   crypto::G1 pair[2]{points[i],points[i+half]};
   crypto::G1::mulVec(points[i],pair,scalars,2);
  }
 };
 const auto count=workers?std::max(1u,foldGeneratorWorkers(half,workers->count())):1u;
 perf5::WorkerDispatch profile(perf5::Kernel::A3GeneratorFold,half,count);
 if(!workers || count==1){
  profile.participant(0,[&]{fold(0,half);});
  return;
 }
 workers->run(count,[&](std::size_t worker){
  profile.participant(worker,[&]{fold(half*worker/count,half*(worker+1)/count);});
 },true);
}

}
