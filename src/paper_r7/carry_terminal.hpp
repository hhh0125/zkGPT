#pragma once
#include "crypto.hpp"
namespace zkgpt::r7::oneblock::classified {
// Public one-terminal joint basis. Free source bits are contiguous; adding or
// subtracting a power-of-two global stride uses a two-state carry/borrow DP.
// No division, challenge cache, witness values, or alignment assumption on z.
class CarryTerminalBasis {
 using Fr=crypto::Fr;
 const std::vector<Fr>&a_;const std::vector<Fr>&q_;
 static bool power(std::size_t n){return n && !(n&(n-1));}
 static unsigned shift(std::size_t n){unsigned out=0;while(n>1){n>>=1;++out;}return out;}
 static Fr high(const std::vector<Fr>&q,std::size_t at,unsigned low){Fr out=1;for(unsigned b=low;b<q.size();++b)out*=((at>>b)&1)?q[b]:Fr(1)-q[b];return out;}
 public:
 CarryTerminalBasis(const std::vector<Fr>&a,const std::vector<Fr>&q):a_(a),q_(q){crypto::need(a.size()<=32 && q.size()<=32,"carry basis dimensions");}
 Fr block(std::size_t j,std::size_t z,unsigned bits,std::size_t step,std::int64_t stride)const{
  crypto::need(power(step) && step<=(1ull<<32) && stride>=-4294967296LL && stride<=4294967296LL && bits<=32,"carry basis affine dimensions");
  const auto k=shift(step);const auto n=std::size_t(1)<<bits;const auto magnitude=std::size_t(stride<0?-stride:stride);
  crypto::need(k+bits<=a_.size() && !((j>>k)&(n-1)) && j+(n-1)*step<(1ull<<a_.size()) && z<(1ull<<q_.size()),"carry source block bounds/alignment");
  Fr fixed=1;for(unsigned b=0;b<a_.size();++b)if(b<k || b>=k+bits)fixed*=((j>>b)&1)?a_[b]:Fr(1)-a_[b];
  if(!stride)return fixed*high(q_,z,0);
  crypto::need(power(magnitude),"carry basis power stride");const auto h=shift(magnitude);crypto::need(h+bits<=q_.size(),"carry global dimensions");
  const auto width=(n-1)*magnitude;crypto::need(stride>0?width<=(1ull<<q_.size())-1-z:width<=z,"carry global block bounds");
  for(unsigned b=0;b<h;++b)fixed*=((z>>b)&1)?q_[b]:Fr(1)-q_[b];
  std::array<Fr,2>dp{Fr(1),Fr(0)};
  for(unsigned b=0;b<bits;++b){std::array<Fr,2>next{Fr(0),Fr(0)};unsigned base=(z>>(h+b))&1;
   for(unsigned carry=0;carry<2;++carry)for(unsigned bit=0;bit<2;++bit){unsigned out=0,to=0;
    if(stride>0){auto sum=base+bit+carry;out=sum&1;to=sum>>1;}
    else{out=(base+2-bit-carry)&1;to=bit+carry>base;}
    next[to]+=dp[carry]*(bit?a_[k+b]:Fr(1)-a_[k+b])*(out?q_[h+b]:Fr(1)-q_[h+b]);
   }dp=next;
  }
  const auto low=h+bits;const auto upper=z>>low;const auto limit=std::size_t(1)<<(q_.size()-low);Fr result=dp[0]*high(q_,upper<<low,low);
  if(stride>0){if(upper+1<limit)result+=dp[1]*high(q_,(upper+1)<<low,low);}
  else if(upper)result+=dp[1]*high(q_,(upper-1)<<low,low);
  return fixed*result;
 }
};
}
