#include "oneblock_sumcheck.hpp"
#include <array>
#include <iostream>
#include <chrono>
using namespace zkgpt::r7::oneblock;
Fr evaluate(const Expression&e,const std::vector<Fr>&x){Fr v=0;for(auto&t:e.terms){auto a=t.coefficient;for(auto k:t.factors)a*=x[k];v+=a;}return v;}
unsigned degree(const Expression&e){unsigned d=0;for(auto&t:e.terms)d=std::max(d,unsigned(t.factors.size()));return d;}
std::vector<Fr>reference(const Expression&e,std::size_t n,const std::vector<Value>&f,const std::vector<std::vector<Fr>>&folded,bool first){
 auto d=degree(e);std::vector<Fr>ys(d+1,Fr(0)),x0(e.factor_count),dx(e.factor_count),x(e.factor_count);
 for(std::size_t j=0;j<n/2;++j){for(std::size_t k=0;k<e.factor_count;++k){x0[k]=first?f[k](2*j):folded[k][2*j];dx[k]=(first?f[k](2*j+1):folded[k][2*j+1])-x0[k];}for(unsigned t=0;t<=d;++t){for(std::size_t k=0;k<e.factor_count;++k)x[k]=x0[k]+Fr(t)*dx[k];ys[t]+=evaluate(e,x);}}
 Fr half,sixth;Fr::inv(half,Fr(2));Fr::inv(sixth,Fr(6));Fr a3=d==3?(ys[3]-Fr(3)*ys[2]+Fr(3)*ys[1]-ys[0])*sixth:Fr(0);Fr a2=d>=2?(ys[2]-Fr(2)*ys[1]+ys[0])*half-Fr(3)*a3:Fr(0);std::vector<Fr>c(d+1,Fr(0));c[0]=ys[0];if(d>=1)c[1]=ys[1]-c[0]-a2-a3;if(d>=2)c[2]=a2;if(d==3)c[3]=a3;return c;
}
Fr randomFr(unsigned i,unsigned seed){Bytes b;for(auto v:{i,seed})for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));auto d=r1::hash(b);Fr x;x.setLittleEndianMod(d.data(),d.size());return x;}
Expression spec(unsigned d){if(d==0)return {1,{{Fr(7),{}}}};if(d==1)return {2,{{Fr(3),{0}},{Fr(-5),{1}},{Fr(7),{}}}};if(d==2)return {4,{{Fr(1),{0,1}},{Fr(-3),{2,3}},{Fr(7),{0,0}},{Fr(5),{}},{Fr(-1),{3}}}};return {5,{{Fr(1),{0,1,2}},{Fr(-3),{0,3,4}},{Fr(4),{1,1,1}},{Fr(5),{}},{Fr(-1),{3}}}};}
int main(int argc,char**argv){try{r1::init();bool bench=argc>1;bool optimized=argc<=1 || std::string(argv[1])!="reference";unsigned rounds=0;double polynomialCPU=0;auto start=std::chrono::steady_clock::now();auto cpu=processCPU();
 for(unsigned d:(bench?std::vector<unsigned>{2,3}:std::vector<unsigned>{0,1,2,3}))for(unsigned seed=0;seed<(bench?1u:24u);++seed){auto e=spec(d);auto n=bench?65536u:(1u<<(seed%9));std::vector<std::vector<Fr>>input(e.factor_count,std::vector<Fr>(n));std::vector<Value>f;for(unsigned k=0;k<e.factor_count;++k){for(unsigned j=0;j<n;++j)input[k][j]=randomFr(j,seed*13+k);f.push_back([&,k](std::size_t j){return input[k][j];});}
  std::vector<std::vector<Fr>>folded=input;std::vector<Fr>point;Fr claim=0;std::vector<Fr>x(e.factor_count);for(unsigned j=0;j<n;++j){for(unsigned k=0;k<e.factor_count;++k)x[k]=input[k][j];claim+=evaluate(e,x);}
  for(std::size_t current=n;current>1;current/=2){auto polyStarted=processCPU();auto got=optimized?detail::expressionPolynomial(e,current,f,folded,current==n):reference(e,current,f,folded,current==n);polynomialCPU+=processCPU()-polyStarted;if(!bench){auto expected=reference(e,current,f,folded,current==n);need(got==expected,"degree2/3 round coefficients mismatch");Fr recurrence=got[0];for(auto&a:got)recurrence+=a;need(recurrence==claim,"sumcheck recurrence mismatch");}
   auto r=randomFr(unsigned(current),seed+91);point.push_back(r);Fr power=1;claim=0;for(auto&a:got){claim+=a*power;power*=r;}for(unsigned k=0;k<e.factor_count;++k){for(std::size_t j=0;j<current/2;++j)folded[k][j]=folded[k][2*j]+r*(folded[k][2*j+1]-folded[k][2*j]);folded[k].resize(current/2);}++rounds;
  }
  if(!bench){for(unsigned k=0;k<e.factor_count;++k){need(folded[k][0]==r1::mle(input[k],point),"final factor MLE mismatch");x[k]=folded[k][0];}need(claim==evaluate(e,x),"terminal expression mismatch");}
 }
 std::cout<<"{\"test\":\"sumcheck\",\"mode\":\""<<(optimized?"optimized":"reference")<<"\",\"rounds\":"<<rounds<<",\"wall_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<",\"polynomial_cpu_seconds\":"<<polynomialCPU<<",\"cpu_seconds\":"<<processCPU()-cpu<<"}\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
