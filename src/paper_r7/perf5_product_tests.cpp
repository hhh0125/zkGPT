#define main retained_perf5_sumcheck_test_main
#include "opt2_sumcheck_tests.cpp"
#undef main
// Catches any wrong cubic coefficient, fold direction, or endpoint; the oracle
// evaluates at four integer points and interpolates, independently of the kernel.
int main(){try{r1::init();unsigned checks=0;
 for(unsigned seed=0;seed<30;++seed){auto n=std::size_t(1)<<(1+seed%10);Expression e{3,{{Fr(1),{0,1,2}}}};std::vector<std::vector<Fr>>input(3,std::vector<Fr>(n));std::vector<Value>f;
  for(unsigned k=0;k<3;++k){for(unsigned j=0;j<n;++j)input[k][j]=seed<3?Fr(int(seed)-1):randomFr(j,seed*7+k);f.push_back([&,k](std::size_t j){return input[k].at(j);});}
  auto folded=input;std::vector<Fr>point;Fr claim=0;for(std::size_t j=0;j<n;++j)claim+=input[0][j]*input[1][j]*input[2][j];
  for(auto current=n;current>1;current/=2){auto got=detail::productPolynomial(current,f,folded,current==n);auto want=reference(e,current,f,folded,current==n);need(got==want,"PERF5 cubic full coefficient mismatch");need(got==detail::expressionPolynomial(e,current,f,folded,current==n),"PERF4 coefficient mismatch");Fr recurrence=got[0];for(auto&a:got)recurrence+=a;need(recurrence==claim,"PERF5 recurrence mismatch");auto round=point.size();Fr r=round%4==0?Fr(0):round%4==1?Fr(1):round%4==2?Fr(-1):randomFr(unsigned(current),seed);point.push_back(r);Fr power=1;claim=0;for(auto&a:got){claim+=power*a;power*=r;}for(auto&v:folded){for(std::size_t j=0;j<current/2;++j)v[j]=v[2*j]+r*(v[2*j+1]-v[2*j]);v.resize(current/2);}++checks;}
  for(unsigned k=0;k<3;++k)need(folded[k][0]==r1::mle(input[k],point),"PERF5 endpoint mismatch");need(claim==folded[0][0]*folded[1][0]*folded[2][0],"PERF5 final product mismatch");
 }
 for(unsigned bits=0;bits<10;++bits)for(auto r:{Fr(0),Fr(1),Fr(-1),Fr(17)}){std::vector<Fr>x(bits);for(unsigned k=0;k<bits;++k)x[k]=Fr(int(k)-2);auto old=r1::weights(x);x.insert(x.begin(),r);auto fresh=r1::weights(x);for(std::size_t j=0;j<old.size();++j){need(fresh[2*j]==(Fr(1)-r)*old[j] && fresh[2*j+1]==r*old[j],"PERF5 interleaved weight recurrence mismatch");}}
 std::cout<<"PASS PERF5 complete cubic coefficients, recurrence, zero/one/negative challenges, endpoints and little-endian weights; rounds="<<checks<<'\n';return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
