#include "oneblock_sumcheck.hpp"
#include "carry_terminal.hpp"
#include <random>
#include <chrono>
#include <iostream>
using namespace zkgpt::r7;using namespace oneblock;using namespace oneblock::classified;
namespace {
Fr chi(std::size_t j,const std::vector<Fr>&q){Fr out=1;for(unsigned b=0;b<q.size();++b)out*=((j>>b)&1)?q[b]:Fr(1)-q[b];return out;}
void compare(const std::vector<Fr>&a,const std::vector<Fr>&q,std::size_t j,std::size_t z,unsigned bits,std::size_t step,std::int64_t stride){
 CarryTerminalBasis basis(a,q);Fr reference=0;for(std::size_t t=0;t<(std::size_t(1)<<bits);++t)reference+=chi(j+t*step,a)*chi(std::size_t(std::int64_t(z)+std::int64_t(t)*stride),q);
 need(basis.block(j,z,bits,step,stride)==reference,"CARRY_BASIS_DENSE_MISMATCH");
}
}
int main(){try{r1::init();std::mt19937 random(20261109);
 for(unsigned trial=0;trial<2200;++trial){
  unsigned ad=1+random()%12,qd=1+random()%12,k=random()%ad,h=random()%qd,bits=random()%(std::min({8u,ad-k,qd-h})+1);std::size_t n=std::size_t(1)<<bits,step=std::size_t(1)<<k;
  std::int64_t stride=trial%3?std::int64_t(1)<<h:0;if(trial%3==2)stride=-stride;
  std::size_t j=((random()%(std::size_t(1)<<(ad-k-bits)))<<(k+bits))+(random()%step);if(trial%11==0)j=(std::size_t(1)<<ad)-n*step+step-1;
  auto width=(n-1)*std::size_t(stride<0?-stride:stride),domain=std::size_t(1)<<qd;std::size_t z=random()%(domain-width);if(stride<0)z+=width;
  if(trial%5==0 && stride>0){z=domain-width-1;}
  if(trial%5==1 && stride<0){z=width;}
  std::vector<Fr>a(ad),q(qd);auto point=[&](){switch((trial/3)%4){case 0:return Fr(0);case 1:return Fr(1);case 2:return Fr(random()%2);default:return Fr(int(random()%23)-11);}};
  for(auto&x:a){x=point();}
  for(auto&x:q){x=point();}
  compare(a,q,j,z,bits,step,stride);
 }
 for(int point:{-3,0,1,7})for(unsigned k:{0u,15u,31u})for(unsigned h:{0u,15u,31u})for(unsigned bits=0;bits<=std::min({2u,32-k,32-h});++bits){
  std::vector<Fr>a(32,Fr(point)),q(32,Fr(point));auto n=std::size_t(1)<<bits,step=std::size_t(1)<<k,domain=std::size_t(1)<<32,j=domain-n*step+step-1,magnitude=std::size_t(1)<<h,width=(n-1)*magnitude;
  compare(a,q,j,std::min(domain-width-1,domain/2-1),bits,step,std::int64_t(magnitude));compare(a,q,j,std::max(width,domain/2),bits,step,-std::int64_t(magnitude));compare(a,q,j,domain-1,bits,step,0);
 }
 compare({}, {},0,0,0,1,0);compare(std::vector<Fr>(3,Fr(-3)),{},0,0,3,1,0);compare({},std::vector<Fr>(32,Fr(1)),0,(1ull<<32)-1,0,1,1);
 // Keep referenced point vectors alive for the invalid-input checks.
 std::vector<Fr>a2(2,Fr(3)),q2(2,Fr(-3));CarryTerminalBasis checked(a2,q2);
 for(unsigned bad=0;bad<5;++bad){bool rejected=false;try{switch(bad){case 0:checked.block(0,0,33,1,1);break;case 1:checked.block(0,0,1,3,1);break;case 2:checked.block(1,0,1,1,1);break;case 3:checked.block(0,0,1,1,-1);break;default:checked.block(0,3,1,1,1);}}catch(const std::exception&){rejected=true;}need(rejected,"carry basis invalid bounds accepted");}
 std::vector<Fr>a(18,Fr(-3)),q(20,Fr(7));CarryTerminalBasis basis(a,q);auto start=std::chrono::steady_clock::now();Fr fast=basis.block(0,12345,18,1,1);auto fast_time=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();start=std::chrono::steady_clock::now();Fr dense=0;for(std::size_t j=0;j<(1u<<18);++j)dense+=chi(j,a)*chi(j+12345,q);auto dense_time=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();need(fast==dense,"large carry basis mismatch");
 std::cout<<"CARRY_BASIS_2200_MIXED_DIMENSIONS_POWER_STEPS_SIGNED_STRIDES_BOOLEAN_32BIT_BOUNDARIES_INVALID_BOUNDS_PASS fast_seconds="<<fast_time<<" dense_seconds="<<dense_time<<"\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
