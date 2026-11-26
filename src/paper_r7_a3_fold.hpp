#pragma once
#include "bounded_workers.hpp"
#include "paper_r7_crypto.hpp"
#include "paper_r7_perf5_profile.hpp"

namespace zkgpt::r7::hyrax_a3::detail {

inline unsigned foldGeneratorWorkers(std::size_t half, unsigned capacity){
 // Point linear combinations are substantially heavier than scalar-field
 // folds. Keep small final rounds local and at least eight points per worker.
 return static_cast<unsigned>(std::min<std::size_t>({half/8,capacity,8}));
}

inline void foldGenerators(crypto::G1* points, std::size_t half,
                           const crypto::Fr& inverse, const crypto::Fr& challenge,
                           zkgpt::Workers* workers){
 const auto fold=[&](std::size_t begin,std::size_t end){
  const crypto::Fr scalars[2]{inverse,challenge};
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
