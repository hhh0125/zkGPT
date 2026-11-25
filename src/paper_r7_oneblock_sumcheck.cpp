#include "paper_r7_lasso_masked.hpp"
#include "paper_r7_oneblock_sumcheck.hpp"
#include "paper_r7_matrix_arithmetic.hpp"
#include "paper_r7_perf5_profile.hpp"
#include "paper_r7_product_kernels.hpp"
#include <algorithm>
#include <array>
namespace zkgpt::r7::oneblock {
Hidden publicHidden(IO&io,const std::string&role,const Fr&value){Hidden h;h.value=value;h.handle.session=io.session();h.handle.role=role;h.handle.point=scalarCommit(value,Fr(0));return h;}
Hidden exchangeHidden(IO&io,const std::string&role,const Fr&value,Random*rng){Hidden h;
 if(io.prover()){
  need(rng,"Oneblock entropy required");
  h=hide(io.session(),role,value,*rng);
  io.message(role+"/commitment",encode(h.handle));
 }else{
  h.handle=decodeHandle(io.message(role+"/commitment"));
  validate(h.handle,io.session(),role);
 }
 return h;}
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
void masked::originalSingleTerminal(IO&io,const Hidden&claim,const std::vector<Hidden>&ends,Random*rng,const std::string&role){endpoint(io,claim,ends,Expression{3,{{Fr(1),{0,1,2}}}},rng,role);}
namespace detail {
void nativeMatrixTerminal(IO&io,const Hidden&claim,const std::vector<Hidden>&ends,Random*rng,const std::string&role){endpoint(io,claim,ends,Expression{2,{{Fr(1),{0,1}}}},rng,role);}
namespace {
void inputShape(const ProductInputs&input){auto n=input.equality.size();need(n>=2 && n<=(1u<<24) && !(n&(n-1)) && input.children.size()==2*n,"Product contiguous input shape");}
}
std::vector<Fr>productPolynomialContiguous(const ProductInputs&input){
 inputShape(input);
 const auto coefficients=singleProductCoefficients(input.equality.size(),[&](unsigned k,std::size_t i){
  return readProductInput(input,k,i);
 },nativeWorkers());
 return {coefficients.begin(),coefficients.end()};
}
void foldProductInputs(const ProductInputs&input,const Fr&r,std::vector<std::vector<Fr>>&folded){
 foldNativeProduct(input.equality.size(),r,folded,true,[&](unsigned k,std::size_t i){
  return readProductInput(input,k,i);
 },nativeWorkers());
}
std::vector<Fr>productPolynomial(std::size_t current,const std::vector<Value>&factors,const std::vector<std::vector<Fr>>&folded,bool first){
 const auto coefficients=first?
  singleProductCoefficients(current,[&](unsigned k,std::size_t i){return factors[k](i);},nativeWorkers()):
  singleProductCoefficients(current,[&](unsigned k,std::size_t i){return folded[k][i];},nativeWorkers());
 return {coefficients.begin(),coefficients.end()};
}
template<class Load>static std::vector<Fr>expressionPolynomialLoaded(const Expression&e,std::size_t current,Load load){
 unsigned degree=0;for(auto&t:e.terms)degree=std::max(degree,unsigned(t.factors.size()));
 std::vector<Fr>coeff(degree+1,Fr(0));std::array<Fr,8>a,d;
 for(std::size_t j=0;j<current/2;++j){
  for(std::size_t k=0;k<e.factor_count;++k){a[k]=load(k,2*j);d[k]=load(k,2*j+1)-a[k];}
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
std::vector<Fr>expressionPolynomial(const Expression&e,std::size_t current,const std::vector<Value>&factors,const std::vector<std::vector<Fr>>&folded,bool first){
 return expressionPolynomialLoaded(e,current,[&](std::size_t k,std::size_t i)->Fr{return first?factors[k](i):folded[k][i];});
}
}
static Reduction expressionSumcheckImpl(IO&io,std::size_t n,const Hidden&initial,const std::vector<Value>&factors,const Expression&e,Random*rng,const std::string&role,bool product,const ProductInputs*inputs=nullptr,detail::MatrixArithmetic*matrix=nullptr){
 const bool b2=perf6::enabled("ZKGPT_PERF6_B2");perf6::ScopedMessages message_options(b2);
 unsigned degree=0;{perf5::Scope t(perf5::ExpressionInit);need(n && n<=(1u<<24) && !(n&(n-1)) && e.factor_count>0 && e.factor_count<=8 && !e.terms.empty() && e.terms.size()<=16,"Oneblock expression bounds");need(initial.handle.session==io.session(),"R7 HiddenScalar session/role identity mismatch");Writer statement;statement.text("R7-ONEBLOCK-HIDDEN-EXPRESSION-SUMCHECK-v1");statement.number(n,8);statement.number(e.factor_count);statement.raw(encode(initial.handle));statement.number(e.terms.size());for(auto&t:e.terms){need(t.factors.size()<=3,"Oneblock expression degree");degree=std::max(degree,unsigned(t.factors.size()));statement.scalar(t.coefficient);statement.number(t.factors.size());for(auto i:t.factors){need(i<e.factor_count,"Oneblock expression factor index");statement.number(i);}}bindPublic(io,role+"/statement",statement.b);if(io.prover())need((matrix || factors.size()==e.factor_count) && rng,"Oneblock expression witness shape");}
 std::vector<std::vector<Fr>>folded(e.factor_count);Hidden previous=initial;std::vector<Fr>point;std::size_t current=n;unsigned round=0;
 while(current>1){auto name=role+"/round/"+std::to_string(round);std::vector<Fr>coeff(degree+1,Fr(0));if(io.prover()){
   perf5::Scope t(perf5::Polynomial);
   perf5::Scope round_time(round?perf5::PolynomialLater:perf5::PolynomialFirst,product);
   if(auto*c=perf5::context){if(product){++c->a2_calls;c->a2_pairs+=current/2;if(!inputs && !round)c->poly_callbacks+=3*current;}if(inputs && !round){++c->c2_polynomial_calls;c->c2_polynomial_pairs+=current/2;}}
   perf5::Scope a2_time(perf5::A2Polynomial,product);perf5::Scope c2_time(perf5::C2Input,inputs && !round);
   if(matrix){auto c=matrix->polynomial();coeff.assign(c.begin(),c.end());}
   else coeff=product?(inputs && !round?detail::productPolynomialContiguous(*inputs):detail::productPolynomial(current,factors,folded,!round)):detail::expressionPolynomial(e,current,factors,folded,!round);
  }
  std::vector<Hidden>h;{perf5::Scope t(perf5::Messages);perf5::Scope b2_time(perf5::B2Message,b2);h={previous};for(unsigned k=0;k<=degree;++k)h.push_back(exchangeHidden(io,name+"/c/"+std::to_string(k),coeff[k],rng));{perf5::Scope recurrence(perf5::Recurrence);Relations r;r.role=name+"/recurrence";for(auto&v:h)r.handles.push_back(v.handle);std::vector<Fr>line(h.size(),Fr(1));line[0]=Fr(-1);line[1]=Fr(2);r.linear={line};sigma(io,r,io.prover()?&h:nullptr,rng);}}
  Fr challenge;{perf5::Scope t(perf5::Challenge);challenge=io.coin(name+"/r");point.push_back(challenge);std::vector<Hidden>polynomial(h.begin()+1,h.end());std::vector<Fr>powers;Fr power=1;for(unsigned k=0;k<=degree;++k){powers.push_back(power);power*=challenge;}previous=combine(io.session(),name+"/derived",polynomial,powers);bindPublic(io,name+"/derived",encode(previous.handle));}
  {perf5::Scope t(perf5::Fold);if(io.prover()){perf5::Scope round_time(round?perf5::FoldLater:perf5::FoldFirst,product);
   if(matrix)matrix->fold(challenge);
   else if(product && (!inputs || round)){
    if(!round && perf5::context)perf5::context->fold_callbacks+=3*current;
    detail::foldNativeProduct(current,challenge,folded,!round,[&](unsigned k,std::size_t i){return factors[k](i);},detail::nativeWorkers());
   }else if(inputs && !round){perf5::Scope c2_time(perf5::C2Fold);if(auto*c=perf5::context){++c->c2_fold_calls;c->c2_fold_pairs+=current/2;}detail::foldProductInputs(*inputs,challenge,folded);}else for(std::size_t k=0;k<e.factor_count;++k){if(!round){folded[k].resize(current/2);for(std::size_t j=0;j<current/2;++j){auto a=factors[k](2*j);folded[k][j]=a+challenge*(factors[k](2*j+1)-a);}}else{for(std::size_t j=0;j<current/2;++j)folded[k][j]=folded[k][2*j]+challenge*(folded[k][2*j+1]-folded[k][2*j]);folded[k].resize(current/2);}}}
  }current/=2;++round;
 }
 perf5::Scope endpoint_scope(perf5::Endpoint);perf5::Scope b2_time(perf5::B2Message,b2);std::vector<Hidden>ends;for(std::size_t k=0;k<e.factor_count;++k)ends.push_back(exchangeHidden(io,role+"/endpoint/"+std::to_string(k),io.prover()?(matrix?matrix->values[k][0]:n==1?factors[k](0):folded[k][0]):Fr(0),rng));endpoint(io,previous,ends,e,rng,role+"/terminal");return {initial,ends,point};
}
Reduction expressionSumcheck(IO&io,std::size_t n,const Hidden&initial,const std::vector<Value>&factors,const Expression&e,Random*rng,const std::string&role){return expressionSumcheckImpl(io,n,initial,factors,e,rng,role,false);}
Reduction detail::matrixExpressionSumcheck(IO&io,std::size_t n,const Hidden&initial,detail::MatrixArithmetic*matrix,Random*rng,const std::string&role){
 return expressionSumcheckImpl(io,n,initial,{},Expression{2,{{Fr(1),{0,1}}}},rng,role,false,nullptr,matrix);
}
Reduction productSumcheck(IO&io,std::size_t n,const Hidden&initial,const std::vector<Value>&factors,Random*rng,const std::string&role,const ProductInputs*inputs,bool){
 if(inputs)need(io.prover() && n && n<=(1u<<24) && inputs->equality.size()==n && inputs->children.size()==2*n,"Product native input binding");
 perf5::Scope independent_time(perf5::IndependentProduct);
 if(auto*c=perf5::context)c->independent_calls+=io.prover();
 if(masked::singleEnabled())return masked::single(io,n,initial,factors,rng,role,inputs,true);
 return expressionSumcheckImpl(io,n,initial,factors,Expression{3,{{Fr(1),{0,1,2}}}},rng,role,true,inputs);
}
static std::array<Reduction,2> batchedProductSumcheckImpl(IO&io,std::size_t n,const std::array<Hidden,2>&initial,const std::vector<Value>&factors,Random*rng,const std::string&role,const std::array<ProductInputs,2>*inputs,const std::vector<Fr>*equality_point){
 if(equality_point)need(equality_point->size()==r1::log2exact(n),"Product01 equality point dimensions");
 const bool b2=perf6::enabled("ZKGPT_PERF6_B2");perf6::ScopedMessages message_options(b2);
 perf5::Scope batched_time(perf5::BatchedProduct);
 if(auto*c=perf5::context)c->batched_calls+=io.prover();
 if(inputs){need(io.prover(),"Product01 continuous prover only");for(const auto&input:*inputs)need(input.equality.size()==n && input.children.size()==2*n,"Product01 continuous dimensions");need(&(*inputs)[0].equality==&(*inputs)[1].equality,"Product01 shared equality");}
 need(n && n<=(1u<<24) && !(n&(n-1)),"Product01 sumcheck domain");
 if(masked::batchEnabled())return masked::batch(io,n,initial,factors,rng,role,inputs,equality_point);
 Writer statement;statement.text("R7-PRODUCT01-LASSO-BATCH-v2");statement.number(n,8);
 for(const auto&h:initial){need(h.handle.session==io.session(),"R7 HiddenScalar session/role identity mismatch");statement.raw(encode(h.handle));}
 bindPublic(io,role+"/statement",statement.b);
 if(io.prover())need(factors.size()==5 && rng,"Product01 witness shape");
 // Both original carries are bound above. Fresh weights once per layer,
 // not once per inner round. IO coins use the existing nonzero-field sampler.
 const std::vector<Fr>alpha{io.coin(role+"/batch-alpha/0"),io.coin(role+"/batch-alpha/1")};
 Hidden previous=combine(io.session(),role+"/aggregate-initial",{initial[0],initial[1]},alpha);
 bindPublic(io,role+"/aggregate-initial",encode(previous.handle));
 std::vector<std::vector<Fr>>folded(5);std::vector<Fr>point;
 auto current=n;unsigned round=0;
 while(current>1){
  std::vector<Fr>aggregate(4,Fr(0));
  auto name=role+"/round/"+std::to_string(round);
  if(io.prover()){
   perf5::Scope t(perf5::Polynomial);
   perf5::Scope round_time(round?perf5::PolynomialLater:perf5::PolynomialFirst);
   const auto coeff=detail::batchProductRound(current,round,factors,folded,inputs,{alpha[0],alpha[1]},
                                             equality_point?&(*equality_point)[round]:nullptr);
   std::copy(coeff.begin(),coeff.end(),aggregate.begin());
  }
  std::vector<Hidden>messages{previous};
  {perf5::Scope t(perf5::Messages);perf5::Scope b2_time(perf5::B2Message,b2);
   for(unsigned k=0;k<4;++k)messages.push_back(exchangeHidden(io,name+"/c/"+std::to_string(k),aggregate[k],rng));
   Relations recurrence;recurrence.role=name+"/recurrence";for(auto&h:messages)recurrence.handles.push_back(h.handle);
   recurrence.linear={{Fr(-1),Fr(2),Fr(1),Fr(1),Fr(1)}};
   {perf5::Scope check(perf5::BatchSigma);sigma(io,recurrence,io.prover()?&messages:nullptr,rng);}
  }
  Fr challenge;{perf5::Scope t(perf5::Challenge);challenge=io.coin(name+"/r");point.push_back(challenge);
   std::vector<Fr>powers{Fr(1),challenge,challenge*challenge,challenge*challenge*challenge};
   std::vector<Hidden>polynomial(messages.begin()+1,messages.end());auto label=name+"/derived";
   previous=combine(io.session(),label,polynomial,powers);bindPublic(io,label,encode(previous.handle));
  }
  if(io.prover())detail::foldBatchedProductRound(current,round,challenge,factors,folded,inputs);
  current/=2;++round;
 }
 perf5::Scope t(perf5::Endpoint);perf5::Scope b2_time(perf5::B2Message,b2);std::vector<Hidden>ends;
 for(unsigned k=0;k<5;++k)ends.push_back(exchangeHidden(io,role+"/endpoint/"+std::to_string(k),io.prover()?(n==1?factors[k](0):folded[k][0]):Fr(0),rng));
 // Preserve all five independent endpoint handles. Four multiplication
 // equations certify z_b=L_b*R_b and t_b=E*z_b; one explicit linear row binds
 // the final aggregate claim to alpha0*t0+alpha1*t1.
 std::vector<Hidden>w=ends;
 for(unsigned b=0;b<2;++b)w.push_back(exchangeHidden(io,role+"/terminal/product/"+std::to_string(b),io.prover()?ends[1+2*b].value*ends[2+2*b].value:Fr(0),rng));
 for(unsigned b=0;b<2;++b)w.push_back(exchangeHidden(io,role+"/terminal/weighted-input/"+std::to_string(b),io.prover()?ends[0].value*w[5+b].value:Fr(0),rng));
 w.push_back(previous);Relations terminal;terminal.role=role+"/terminal";for(auto&h:w)terminal.handles.push_back(h.handle);
 terminal.products={{1,2,5},{0,5,7},{3,4,6},{0,6,8}};
 terminal.linear={{Fr(0),Fr(0),Fr(0),Fr(0),Fr(0),Fr(0),Fr(0),alpha[0],alpha[1],Fr(-1)}};
 {perf5::Scope check(perf5::BatchSigma);sigma(io,terminal,io.prover()?&w:nullptr,rng);}
 return {Reduction{initial[0],{ends[0],ends[1],ends[2]},point},Reduction{initial[1],{ends[0],ends[3],ends[4]},point}};
}
std::array<Reduction,2> batchedProductSumcheck(IO&io,std::size_t n,const std::array<Hidden,2>&initial,const std::vector<Value>&factors,Random*rng,const std::string&role,const std::vector<Fr>*equality_point){
 return batchedProductSumcheckImpl(io,n,initial,factors,rng,role,nullptr,equality_point);
}
std::array<Reduction,2> detail::batchedProductSumcheckContiguous(IO&io,std::size_t n,const std::array<Hidden,2>&initial,const std::vector<Value>&factors,Random*rng,const std::string&role,const std::array<ProductInputs,2>&inputs,const std::vector<Fr>*equality_point){
 return batchedProductSumcheckImpl(io,n,initial,factors,rng,role,&inputs,equality_point);
}

}
