#pragma once
#include "crypto.hpp"
namespace zkgpt::r7::oneblock::classified {
// Public, one-terminal-only bilinear MLE basis. No division or cached challenge.
class TerminalBasis {
 const std::vector<crypto::Fr>&a_;const std::vector<crypto::Fr>&r_;
 std::array<crypto::Fr,33>joint_;
 static crypto::Fr high(const std::vector<crypto::Fr>&q,std::size_t at,unsigned low){
  crypto::Fr result=1;for(unsigned t=low;t<q.size();++t)result*=((at>>t)&1)?q[t]:crypto::Fr(1)-q[t];return result;
 }
 public:
 TerminalBasis(const std::vector<crypto::Fr>&a,const std::vector<crypto::Fr>&r):a_(a),r_(r){
  crypto::need(a.size()<=32 && r.size()<=32,"K terminal basis dimension");joint_[0]=1;
  for(unsigned t=0;t<std::min(a.size(),r.size());++t)joint_[t+1]=joint_[t]*((crypto::Fr(1)-a[t])*(crypto::Fr(1)-r[t])+a[t]*r[t]);
 }
 crypto::Fr block(std::size_t j,std::size_t z,unsigned bits,bool repeated)const{
  crypto::need(bits<=a_.size() && (repeated || bits<=r_.size()),"K terminal block dimension");
  const auto mask=(std::size_t(1)<<bits)-1;
  crypto::need(bits<=a_.size() && (repeated || bits<=r_.size()) && !(j&mask) && (repeated || !(z&mask)),"K terminal block alignment");
  return high(a_,j,bits)*high(r_,z,repeated?0:bits)*(repeated?crypto::Fr(1):joint_[bits]);
 }
};
}
