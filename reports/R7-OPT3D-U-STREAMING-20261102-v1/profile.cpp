#include "unified_input.hpp"
#include "opt2_metrics.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <sys/resource.h>
using namespace zkgpt::r7::oneblock;
namespace ui=zkgpt::r7::oneblock::unified;
int main(){r1::init();const std::size_t n=1<<20;std::vector<Fr>u(n),v(n);for(std::size_t j=0;j<n;++j)u[j]=Fr(j%71+1);Value oracle=[&](std::size_t j){need(j<n,"profile public root bounds");return u.at(j);};
 for(unsigned rep=0;rep<3;++rep)for(unsigned t:{0u,8u,16u,24u}){std::vector<Fr>q(t,Fr(3));auto lo=r1::weights({q.begin(),q.begin()+t/2}),hi=r1::weights({q.begin()+t/2,q.end()});
  for(unsigned phase=0;phase<6;++phase){auto wall=std::chrono::steady_clock::now();auto cpu=zkgpt::r7::crypto::processCPU();Fr total=0;std::uint64_t address=0;
   if(phase==0)for(std::size_t j=0;j<n;++j)address+=(j/(std::size_t(1)<<t))*(std::size_t(1)<<t)+(j&((std::size_t(1)<<t)-1));
   if(phase==1)for(std::size_t j=0;j<n;++j)v[j]=oracle(j);
   if(phase==2)for(std::size_t j=0;j<n;++j)v[j]=u[j];
   if(phase==3)for(std::size_t j=0;j<n;++j)v[j]=u[j]*lo[j&(lo.size()-1)];
   if(phase==4)for(std::size_t j=0;j<n;++j)total+=u[j];
   if(phase==5)for(std::size_t j=0;j<n;++j){auto i=j&((std::size_t(1)<<t)-1);total+=(lo[i&(lo.size()-1)]*hi[i>>(t/2)])*oracle(j);}
   Writer w;w.scalar(total);w.scalar(v[rep]);w.number(address,8);struct rusage usage{};getrusage(RUSAGE_SELF,&usage);
   std::cout<<std::setprecision(14)<<"{\"repeat\":"<<rep<<",\"prefix_bits\":"<<t<<",\"phase\":"<<phase<<",\"coordinates\":"<<n<<",\"wall_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-wall).count()<<",\"cpu_seconds\":"<<zkgpt::r7::crypto::processCPU()-cpu<<",\"rss_kib\":"<<usage.ru_maxrss<<",\"digest\":\""<<hex(r1::hash(w.b))<<"\"}\n";
  }
 }
}
