#include "oneblock_sumcheck.hpp"
#include <algorithm>
#include <array>
namespace zkgpt::r7::oneblock {
Hidden publicHidden(IO&io,const std::string&role,const Fr&value){Hidden h;h.value=value;h.handle.session=io.session();h.handle.role=role;h.handle.point=scalarCommit(value,Fr(0));return h;}
Hidden exchangeHidden(IO&io,const std::string&role,const Fr&value,Random*rng){Hidden h;Bytes b;if(io.prover()){need(rng,"Oneblock entropy required");h=hide(io.session(),role,value,*rng);b=encode(h.handle);}b=io.message(role+"/commitment",b);h.handle=decodeHandle(b);validate(h.handle,io.session(),role);return h;}
void equalHidden(IO&io,const Hidden&h,const Fr&value,Random*rng,const std::string&role){auto c=publicHidden(io,role+"/public",value);std::vector<Hidden>w{h,c};Relations r;r.role=role;r.handles={h.handle,c.handle};r.linear={{Fr(1),Fr(-1)}};sigma(io,r,io.prover()?&w:nullptr,rng);}
namespace {

Hidden alias(IO&io,const Hidden&h,const std::string&role){auto out=h;out.handle.role=role;bindPublic(io,role,encode(out.handle));return out;}
void endpoint(IO&io,const Hidden&claim,const std::vector<Hidden>&x,const Expression&e,Random*rng,const std::string&role){
 std::vector<Hidden>terms;std::vector<Fr>coeff;
 for(std::size_t j=0;j<e.terms.size();++j){auto&t=e.terms[j];auto term=publicHidden(io,role+"/term/"+std::to_string(j)+"/one",Fr(1));
  if(!t.factors.empty())term=x[t.factors[0]];
  for(std::size_t k=1;k<t.factors.size();++k){auto name=role+"/term/"+std::to_string(j)+"/mul/"+std::to_string(k);auto a=alias(io,term,name+"/left"),b=alias(io,x[t.factors[k]],name+"/right");auto z=exchangeHidden(io,name+"/output",io.prover()?a.value*b.value:Fr(0),rng);std::vector<Hidden>w{a,b,z};Relations r;r.role=name;r.handles={a.handle,b.handle,z.handle};r.products={{0,1,2}};sigma(io,r,io.prover()?&w:nullptr,rng);term=z;}
  terms.push_back(alias(io,term,role+"/sum-term/"+std::to_string(j)));coeff.push_back(t.coefficient);
 }
 terms.push_back(claim);coeff.push_back(Fr(-1));Relations r;r.role=role+"/expression";for(auto&t:terms)r.handles.push_back(t.handle);r.linear={coeff};sigma(io,r,io.prover()?&terms:nullptr,rng);
}
}
namespace detail {
std::vector<Fr>expressionPolynomial(const Expression&e,std::size_t current,const std::vector<Value>&factors,const std::vector<std::vector<Fr>>&folded,bool first){
 unsigned degree=0;for(auto&t:e.terms)degree=std::max(degree,unsigned(t.factors.size()));
 std::vector<Fr>coeff(degree+1,Fr(0));std::array<Fr,8>a,d;
 for(std::size_t j=0;j<current/2;++j){
  for(std::size_t k=0;k<e.factor_count;++k){a[k]=first?factors[k](2*j):folded[k][2*j];d[k]=(first?factors[k](2*j+1):folded[k][2*j+1])-a[k];}
  for(auto&t:e.terms){auto n=t.factors.size();const auto&c=t.coefficient;
   if(!n){coeff[0]+=c;continue;}
   auto i=t.factors[0];if(n==1){coeff[0]+=c*a[i];coeff[1]+=c*d[i];continue;}
   auto k=t.factors[1];auto ab0=a[i]*a[k],ab1=a[i]*d[k]+d[i]*a[k],ab2=d[i]*d[k];
   if(n==2){coeff[0]+=c*ab0;coeff[1]+=c*ab1;coeff[2]+=c*ab2;continue;}
   auto l=t.factors[2];coeff[0]+=c*(ab0*a[l]);coeff[1]+=c*(ab0*d[l]+ab1*a[l]);coeff[2]+=c*(ab1*d[l]+ab2*a[l]);coeff[3]+=c*(ab2*d[l]);
  }
 }
 return coeff;
}
}
Reduction expressionSumcheck(IO&io,std::size_t n,const Hidden&initial,const std::vector<Value>&factors,const Expression&e,Random*rng,const std::string&role){
 need(n && n<=(1u<<24) && !(n&(n-1)) && e.factor_count>0 && e.factor_count<=8 && !e.terms.empty() && e.terms.size()<=16,"Oneblock expression bounds");validate(initial.handle,io.session(),initial.handle.role);unsigned degree=0;Writer statement;statement.text("R7-ONEBLOCK-HIDDEN-EXPRESSION-SUMCHECK-v1");statement.number(n,8);statement.number(e.factor_count);statement.raw(encode(initial.handle));statement.number(e.terms.size());for(auto&t:e.terms){need(t.factors.size()<=3,"Oneblock expression degree");degree=std::max(degree,unsigned(t.factors.size()));statement.scalar(t.coefficient);statement.number(t.factors.size());for(auto i:t.factors){need(i<e.factor_count,"Oneblock expression factor index");statement.number(i);}}bindPublic(io,role+"/statement",statement.b);if(io.prover())need(factors.size()==e.factor_count && rng,"Oneblock expression witness shape");
 std::vector<std::vector<Fr>>folded(e.factor_count);Hidden previous=initial;std::vector<Fr>point;std::size_t current=n;unsigned round=0;
 while(current>1){auto name=role+"/round/"+std::to_string(round);std::vector<Fr>coeff(degree+1,Fr(0));if(io.prover()){
   coeff=detail::expressionPolynomial(e,current,factors,folded,!round);
  }
  std::vector<Hidden>h{previous};for(unsigned k=0;k<=degree;++k)h.push_back(exchangeHidden(io,name+"/c/"+std::to_string(k),coeff[k],rng));Relations r;r.role=name+"/recurrence";for(auto&v:h)r.handles.push_back(v.handle);std::vector<Fr>line(h.size(),Fr(1));line[0]=Fr(-1);line[1]=Fr(2);r.linear={line};sigma(io,r,io.prover()?&h:nullptr,rng);
  auto challenge=io.coin(name+"/r");point.push_back(challenge);std::vector<Hidden>polynomial(h.begin()+1,h.end());std::vector<Fr>powers;Fr power=1;for(unsigned k=0;k<=degree;++k){powers.push_back(power);power*=challenge;}previous=combine(io.session(),name+"/derived",polynomial,powers);bindPublic(io,name+"/derived",encode(previous.handle));
  if(io.prover())for(std::size_t k=0;k<e.factor_count;++k){if(!round){folded[k].resize(current/2);for(std::size_t j=0;j<current/2;++j){auto a=factors[k](2*j);folded[k][j]=a+challenge*(factors[k](2*j+1)-a);}}else{for(std::size_t j=0;j<current/2;++j)folded[k][j]=folded[k][2*j]+challenge*(folded[k][2*j+1]-folded[k][2*j]);folded[k].resize(current/2);}}
  current/=2;++round;
 }
 std::vector<Hidden>ends;for(std::size_t k=0;k<e.factor_count;++k)ends.push_back(exchangeHidden(io,role+"/endpoint/"+std::to_string(k),io.prover()?(n==1?factors[k](0):folded[k][0]):Fr(0),rng));endpoint(io,previous,ends,e,rng,role+"/terminal");return {initial,ends,point};
}
}
