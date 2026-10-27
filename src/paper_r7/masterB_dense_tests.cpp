#include "global_bridge.hpp"
#include <iostream>
using namespace zkgpt::r7;using namespace masterB;
namespace {
struct CountRandom:Random {unsigned count=0;void fill(std::uint8_t*p,std::size_t n)override{auto b=r1::encode(Fr(++count));need(n==b.size(),"B1 reference entropy width");std::copy(b.begin(),b.end(),p);}};
struct ReferenceIO:IO {
 bool p;CountRandom&random;std::vector<Fr>x,k;std::vector<std::pair<std::string,Bytes>>records;std::size_t pos=0;unsigned comparisons=0;
 ReferenceIO(bool isP,CountRandom&r,std::vector<Fr>values,std::vector<Fr>kernel):p(isP),random(r),x(std::move(values)),k(std::move(kernel)){}
 bool prover()const override{return p;}std::string session()const override{return std::string(64,'b');}Digest prefix()const override{return {};}
 Bytes message(const std::string&label,const Bytes&b={})override{
  if(!p){need(pos<records.size() && records[pos].first==label,"B1 dense independent V sequence");return records[pos++].second;}
  if(label.find("/c/")!=std::string::npos && label.size()>11 && label.substr(label.size()-11)=="/commitment"){
   // Independent oracle: evaluate the dense round polynomial at0,1,2 and
   // interpolate its coefficients, rather than repeat the sparse formula.
   std::array<Fr,3>v{Fr(0),Fr(0),Fr(0)};
   for(unsigned t=0;t<3;++t)for(std::size_t j=0;j<x.size()/2;++j){auto X=x[2*j]*(Fr(1)-Fr(t))+x[2*j+1]*Fr(t);auto K=k[2*j]*(Fr(1)-Fr(t))+k[2*j+1]*Fr(t);v[t]+=X*K;}
   Fr half;Fr::inv(half,Fr(2));auto c2=(v[2]-Fr(2)*v[1]+v[0])*half;std::array<Fr,3>c{v[0],v[1]-v[0]-c2,c2};
   auto at=label.find("/c/");unsigned j=unsigned(label.at(at+3)-'0');need(j<3,"B1 oracle coefficient index");auto h=decodeHandle(b);need(h.point==scalarCommit(c[j],Fr(random.count)),"B1 sparse polynomial differs from dense interpolation");++comparisons;
  }
  if(label.find("/master-terminal/commitment")!=std::string::npos){need(x.size()==1,"B1 oracle terminal size");need(decodeHandle(b).point==scalarCommit(x[0],Fr(random.count)),"B1 sparse source fold differs from dense endpoint");++comparisons;}
  records.push_back({label,b});return b;
 }
 Fr coin(const std::string&label)override{bool round=label.size()>=2 && label.substr(label.size()-2)=="/r";Fr r(round?3+unsigned(std::stoul(label.substr(label.find("/round/")+7))):7);if(p && round){
  for(std::size_t j=0;j<x.size()/2;++j){x[j]=(Fr(1)-r)*x[2*j]+r*x[2*j+1];k[j]=(Fr(1)-r)*k[2*j]+r*k[2*j+1];}x.resize(x.size()/2);k.resize(k.size()/2);
 }return r;}
};
}
int main(){try{r1::init();unsigned checks=0;
 for(unsigned scenario=0;scenario<3;++scenario){std::vector<Fr>x(4096,Fr(0)),k(4096,Fr(0));SparsePublicKernel sparse;
  for(unsigned j=0;j<4096;++j)x[j]=Fr(int(j%13)-6);
  if(scenario!=2)for(unsigned j=0;j<2050;++j){auto at=scenario==0?(j*17)%4096:unsigned(j%3);auto w=Fr(int(j%7)-3);k[at]+=w;sparse.try_emplace(at,Fr(0)).first->second+=w;}
  Fr value=0;for(unsigned j=0;j<4096;++j)value+=x[j]*k[j];CountRandom rng;ReferenceIO p(true,rng,x,k);auto h=hide(p.session(),"reference/initial",value,rng);
  auto result=committedLinearSumcheck(p,4096,h,[&](std::size_t i){return x.at(i);},sparse,&rng,"reference/linear");need(p.comparisons==37,"B1 oracle covers all12 round polynomials and terminal");checks+=p.comparisons;
  need(result.first.value==p.x[0] && sparseMLE(sparse,result.second)==p.k[0],"B1 sparse public folds differ from dense");++checks;
  CountRandom unused;ReferenceIO v(false,unused,{},{});v.records=p.records;Hidden publicH;publicH.handle=h.handle;auto verified=committedLinearSumcheck(v,4096,publicH,{},sparse,nullptr,"reference/linear");need(v.pos==v.records.size() && encode(verified.first.handle)==encode(result.first.handle),"B1 dense-reference independent V");++checks;
 }
 std::cout<<"B1_DENSE_REFERENCE_PASSED "<<checks<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"B1_DENSE_REFERENCE_FAILED "<<e.what()<<'\n';return 1;}}
