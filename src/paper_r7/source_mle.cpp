#include "source_mle.hpp"
#include <chrono>
namespace zkgpt::r7::oneblock {
Fr sourceMLE(std::size_t padded,const Value&value,const std::vector<Fr>&q,bool stream,SourceMLEMetrics*metrics){
 need(value && q.size()<=28 && padded==(std::size_t(1)<<q.size()),"source MLE dimensions");SourceMLEMetrics m;using Clock=std::chrono::steady_clock;auto start=Clock::now();Fr y=0;
 if(!stream){auto w=r1::weights(q);m.weights_wall=std::chrono::duration<double>(Clock::now()-start).count();m.weight_peak_bytes=w.capacity()*sizeof(Fr);start=Clock::now();for(std::size_t j=0;j<padded;++j)y+=w[j]*value(j);m.values_read=padded;m.multiplications=padded;m.additions=padded;}
 else{auto split=q.size()/2;auto lo=r1::weights({q.begin(),q.begin()+split}),hi=r1::weights({q.begin()+split,q.end()});m.weights_wall=std::chrono::duration<double>(Clock::now()-start).count();m.weight_peak_bytes=(lo.capacity()+hi.capacity())*sizeof(Fr);need(m.weight_peak_bytes<=1ull<<20,"source split weights 1 MiB cap");start=Clock::now();
  // Iterate every padded position, including materialized nonzero helper
  // tails. Global/source indices, point and random blinding are unchanged.
  for(std::size_t h=0;h<hi.size();++h){Fr partial=0;for(std::size_t j=0;j<lo.size();++j)partial+=lo[j]*value(h*lo.size()+j);y+=hi[h]*partial;}
  m.values_read=padded;m.multiplications=padded+hi.size();m.additions=padded+hi.size();
 }
 m.dot_wall=std::chrono::duration<double>(Clock::now()-start).count();if(metrics){*metrics=m;}return y;
}
}
