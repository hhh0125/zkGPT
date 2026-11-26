#include "paper_r7_source_mle.hpp"
#include <chrono>
#include "paper_r7_native_workers.hpp"
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
Fr sourceMLE(const VectorOracle&v,const std::vector<Fr>&q,bool stream,SourceMLEMetrics*metrics){
 if(!stream || (!v.contiguous && v.source_spans.empty()))
  return sourceMLE(v.padded,v.value,q,stream,metrics);
 need(q.size()<=28 && v.padded==(std::size_t(1)<<q.size()),"source MLE dimensions");
 using Clock=std::chrono::steady_clock;
 auto start=Clock::now();
 const auto split=q.size()/2;
 const auto lo=r1::weights({q.begin(),q.begin()+split});
 const auto hi=r1::weights({q.begin()+split,q.end()});
 SourceMLEMetrics m;
 m.weight_peak_bytes=(lo.capacity()+hi.capacity())*sizeof(Fr);
 m.weights_wall=std::chrono::duration<double>(Clock::now()-start).count();
 start=Clock::now();
 const VectorOracle::SourceSpan contiguous{v.padded,v.contiguous,Fr(0)};
 const auto*first_span=v.contiguous?&contiguous:v.source_spans.data();
 const auto*last_span=v.contiguous?first_span+1:first_span+v.source_spans.size();
 need((last_span-1)->end==v.padded,"source MLE span coverage");
 struct alignas(64) Partial {Fr y=0;std::uint64_t reads=0,mul=0,add=0;};
 std::array<Partial,detail::NativeWorkers::capacity> partial;
 auto&workers=detail::nativeWorkers();
 const auto count=detail::nativeRanges(hi.size(),workers,std::max<std::size_t>(1,16384/lo.size()),
  [&](std::size_t k,std::size_t first,std::size_t last){
   auto&local=partial[k];
   const auto*span=std::upper_bound(first_span,last_span,first*lo.size(),
       [](std::size_t j,const auto&s){return j<s.end;});
   for(auto h=first;h<last;++h){
    Fr y=0;
    const auto begin=h*lo.size(),end=begin+lo.size();
    for(auto at=begin;at<end;){
     const auto stop=std::min(end,span->end);
     const bool biased=!span->bias.isZero();
     const auto span_begin=span==first_span?0:(span-1)->end;
     if(span->data){
      const auto*data=span->data+(at-span_begin);
      const auto*weight=lo.data()+(at-begin);
      const auto length=stop-at;
      if(biased)for(std::size_t j=0;j<length;++j)y+=weight[j]*(data[j]+span->bias);
      else for(std::size_t j=0;j<length;++j)y+=weight[j]*data[j];
      local.reads+=length;local.mul+=length;local.add+=length*(biased?2:1);
     }else if(biased){
      Fr weight=0;
      for(auto j=at;j<stop;++j)weight+=lo[j-begin];
      y+=weight*span->bias;
      ++local.mul;local.add+=stop-at+1;
     }
     at=stop;
     if(at==span->end)++span;
    }
    local.y+=hi[h]*y;
    ++local.mul;++local.add;
   }
  });
 Fr result=0;
 for(unsigned k=0;k<count;++k){
  result+=partial[k].y;
  m.values_read+=partial[k].reads;
  m.multiplications+=partial[k].mul;
  m.additions+=partial[k].add+1;
 }
 m.dot_wall=std::chrono::duration<double>(Clock::now()-start).count();
 if(metrics)*metrics=m;
 return result;
}

}
