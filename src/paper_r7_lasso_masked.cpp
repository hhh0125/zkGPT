#include "paper_r7_lasso_masked.hpp"
#include "paper_r7_hyrax_a3.hpp"
#include "paper_r7_stage2_opening.hpp"
#include "paper_r7_perf5_profile.hpp"
#include "paper_r7_product_polynomial.hpp"
#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
namespace zkgpt::r7::oneblock::masked {
namespace {
using Clock=std::chrono::steady_clock;
struct Measure {
 std::array<double,7>seconds{};
 std::string role;bool prover;
 ~Measure(){std::cerr<<std::setprecision(12)<<"MASKED_PRODUCT {\"research\":true,\"role\":"<<std::quoted(role)<<",\"prover\":"<<(prover?"true":"false")<<",\"seconds\":{";const char*names[]={"generation","commitment","sum_opening","initial_boundary","rounds","endpoint_opening","terminal"};for(unsigned i=0;i<7;++i){if(i)std::cerr<<',';std::cerr<<std::quoted(names[i])<<':'<<seconds[i];}std::cerr<<"}}\n";}
};
struct Timer {
 double&value;Clock::time_point start=Clock::now();
 explicit Timer(double&v):value(v){}
 ~Timer(){value+=std::chrono::duration<double>(Clock::now()-start).count();}
};
void linear(IO&io,const std::vector<Hidden>&w,const std::vector<Fr>&a,Random*rng,const std::string&role){
 Relations rel;rel.role=role;for(auto&h:w)rel.handles.push_back(h.handle);rel.linear={a};sigma(io,rel,io.prover()?&w:nullptr,rng);
}
struct Mask {
 unsigned m;std::size_t padded=1;std::vector<Fr>a;Fr blind=0,rho=0,claim=0;G1 commitment;Hidden sum;
 std::string role;Measure&measurement;
 Mask(IO&io,unsigned variables,const Hidden&initial,Random*rng,const std::string&name,Measure&measure):m(variables),role(name),measurement(measure){
  while(padded<1+3*m)padded*=2;
  std::vector<Fr>t(padded,Fr(0));Fr cube=1;for(unsigned i=0;i<m;++i)cube+=cube;t[0]=cube;
  if(m){Fr half;Fr::div(half,cube,Fr(2));for(unsigned i=1;i<=3*m;++i)t[i]=half;}
  if(io.prover()){Timer timer(measure.seconds[0]);need(rng,"mask entropy required");a.assign(padded,Fr(0));for(unsigned i=0;i<=3*m;++i)a[i]=rng->scalar();blind=rng->scalar();}
  {Timer timer(measure.seconds[1]);Writer w;if(io.prover()){commitment=hyrax_a3::vectorCommit(a,blind);w.point(commitment);}auto bytes=io.message(role+"/mask-vector",w.b);Reader r{bytes};commitment=r.point();r.end();sum=exchangeHidden(io,role+"/sum",io.prover()?r1::dot(a,t):Fr(0),rng);}
  {Timer timer(measure.seconds[2]);open(io,sum,t,rng,role+"/sum-opening");}
  rho=io.coin(role+"/rho");
  {Timer timer(measure.seconds[3]);Writer w;if(io.prover())w.scalar(initial.value+rho*sum.value);auto bytes=io.message(role+"/initial-claim",w.b);Reader r{bytes};claim=r.scalar();r.end();
   linear(io,{initial,sum,publicHidden(io,role+"/public-initial",claim)},{Fr(1),rho,Fr(-1)},rng,role+"/initial-boundary");
  }
 }
 void open(IO&io,const Hidden&h,const std::vector<Fr>&weights,Random*rng,const std::string&name){
  hyrax_a3::DotWitness witness;if(io.prover()){witness.values=a;witness.blind=blind;witness.evaluation=h;}
  stage2::ScopedIO scoped(io,name);hyrax_a3::dotProduct(scoped,commitment,h.handle,weights,io.prover()?&witness:nullptr,rng);
 }
 std::array<Fr,4>round(unsigned i,const std::vector<Fr>&r)const{
  need(i<m && r.size()==i,"mask round index");Fr scale=1;for(unsigned j=i+1;j<m;++j)scale+=scale;
  std::array<Fr,4>out{a[0],Fr(0),Fr(0),Fr(0)};
  for(unsigned j=0;j<i;++j){Fr power=r[j];for(unsigned k=0;k<3;++k){out[0]+=a[1+3*j+k]*power;power*=r[j];}}
  out[0]*=scale;
  for(unsigned k=1;k<=3;++k)out[k]=scale*a[1+3*i+k-1];
  if(i+1<m){Fr future=0;for(unsigned j=i+1;j<m;++j)for(unsigned k=0;k<3;++k)future+=a[1+3*j+k];Fr half;Fr::div(half,scale,Fr(2));out[0]+=half*future;}
  return out;
 }
 Fr exchangeRound(IO&io,std::vector<Fr>coeff,unsigned i,const std::vector<Fr>&r){
  Writer w;if(io.prover()){auto q=round(i,r);for(unsigned k=0;k<4;++k)w.scalar(coeff[k]+rho*q[k]);}
  auto name=role+"/round/"+std::to_string(i);auto bytes=io.message(name+"/coefficients",w.b);Reader reader{bytes};for(auto&x:coeff)x=reader.scalar();reader.end();
  {perf5::Scope recurrence(perf5::Recurrence);need(claim==Fr(2)*coeff[0]+coeff[1]+coeff[2]+coeff[3],"masked sumcheck recurrence equation failed");}
  auto challenge=io.coin(name+"/r");claim=coeff[0]+challenge*(coeff[1]+challenge*(coeff[2]+challenge*coeff[3]));return challenge;
 }
 Hidden endpoint(IO&io,const std::vector<Fr>&r,Random*rng){
  Timer timer(measurement.seconds[5]);need(r.size()==m,"mask endpoint dimension");std::vector<Fr>weights(padded,Fr(0));weights[0]=Fr(1);
  for(unsigned j=0;j<m;++j){Fr power=r[j];for(unsigned k=0;k<3;++k){weights[1+3*j+k]=power;power*=r[j];}}
  auto value=exchangeHidden(io,role+"/endpoint-mask",io.prover()?r1::dot(a,weights):Fr(0),rng);
  open(io,value,weights,rng,role+"/endpoint-opening");
  auto out=combine(io.session(),role+"/original-terminal",{publicHidden(io,role+"/public-terminal",claim),value},{Fr(1),-rho});
  bindPublic(io,role+"/original-terminal",encode(out.handle));return out;
 }
};
}
std::array<Reduction,2> batch(IO&io,std::size_t n,const std::array<Hidden,2>&initial,const std::vector<Value>&factors,Random*rng,const std::string&role,const std::array<ProductInputs,2>*inputs,const std::vector<Fr>*equality_point){
 need(n && n<=(1u<<24) && !(n&(n-1)),"masked Product01 domain");need(!io.prover() || (rng && factors.size()==5),"masked Product01 witness boundary");
 const auto options=perf6::options();perf6::ScopedMessages messages(options.b2);
 Measure measurement{{},role,io.prover()};Writer statement;statement.text("R7-LASSO-MASKED-PRODUCT01-HIDDEN-A3-RESEARCH-v1");statement.number(n,8);statement.number(3);statement.text("a0,ai1,ai2,ai3;zero-query-padding");statement.text(hyrax_a3::suite);
 for(auto&h:initial){validate(h.handle,io.session(),h.handle.role);statement.raw(encode(h.handle));}bindPublic(io,role+"/statement",statement.b);
 std::vector<Fr>alpha{io.coin(role+"/batch-alpha/0"),io.coin(role+"/batch-alpha/1")};
 auto aggregate=combine(io.session(),role+"/aggregate-initial",{initial[0],initial[1]},alpha);bindPublic(io,role+"/aggregate-initial",encode(aggregate.handle));
 Mask mask(io,r1::log2exact(n),aggregate,rng,role+"/masked",measurement);
 std::vector<std::vector<Fr>>folded(5);std::vector<Fr>point;std::size_t current=n;unsigned round=0;
 while(current>1){
  Timer timer(measurement.seconds[4]);std::vector<Fr>coeff(4,Fr(0));
  if(io.prover()){
   perf5::Scope polynomial_time(perf5::Polynomial);
   auto load=[&](unsigned k,std::size_t j)->Fr{if(round)return folded[k][j];if(!inputs)return factors[k](j);if(!k)return (*inputs)[0].equality[j];return (*inputs)[(k-1)/2].children[2*j+(k-1)%2];};
   const std::array<Fr,2>weights{alpha[0],alpha[1]};auto mode=polynomial::mode();
   if(mode>=2){need(equality_point && equality_point->size()==r1::log2exact(n),"masked equality metadata");auto c=polynomial::equality<Fr>(current,load,weights,(*equality_point)[round],mode==3);std::copy(c.begin(),c.end(),coeff.begin());}
   else if(mode==1){auto c=polynomial::bespoke<Fr>(current,load,weights);std::copy(c.begin(),c.end(),coeff.begin());}
   else{
    std::array<std::array<Fr,4>,2>partial{{{Fr(0),Fr(0),Fr(0),Fr(0)},{Fr(0),Fr(0),Fr(0),Fr(0)}}};
    for(std::size_t j=0;j<current/2;++j){std::array<Fr,5>a,d;for(unsigned k=0;k<5;++k){a[k]=load(k,2*j);d[k]=load(k,2*j+1)-a[k];}
     for(unsigned b=0;b<2;++b){auto l=1+2*b,r=l+1;auto x0=a[0]*a[l],x1=a[0]*d[l]+d[0]*a[l],x2=d[0]*d[l];auto&c=partial[b];
      if(options.a2){c[0]+=x0*a[r];c[1]+=x0*d[r]+x1*a[r];c[2]+=x1*d[r]+x2*a[r];c[3]+=x2*d[r];}
      else{Fr coefficient=1;c[0]+=coefficient*(x0*a[r]);c[1]+=coefficient*(x0*d[r]+x1*a[r]);c[2]+=coefficient*(x1*d[r]+x2*a[r]);c[3]+=coefficient*(x2*d[r]);}
     }
    }
    {perf5::Scope aggregation_time(perf5::BatchAggregation);for(unsigned k=0;k<4;++k)coeff[k]=alpha[0]*partial[0][k]+alpha[1]*partial[1][k];}
    if(auto*c=perf5::context){if(options.a2){c->a2_calls+=2;c->a2_pairs+=current;}else c->generic_product_calls+=2;}
   }
  }
  auto challenge=mask.exchangeRound(io,coeff,round,point);
  {perf5::Scope folding(perf5::Fold);if(io.prover())for(unsigned k=0;k<5;++k){
   if(!round){folded[k].resize(current/2);auto load=[&](std::size_t j)->Fr{if(!inputs)return factors[k](j);return !k?(*inputs)[0].equality[j]:(*inputs)[(k-1)/2].children[2*j+(k-1)%2];};for(std::size_t j=0;j<current/2;++j){auto a=load(2*j);folded[k][j]=a+challenge*(load(2*j+1)-a);}}
   else{for(std::size_t j=0;j<current/2;++j)folded[k][j]=folded[k][2*j]+challenge*(folded[k][2*j+1]-folded[k][2*j]);folded[k].resize(current/2);}
  }}
  point.push_back(challenge);current/=2;++round;
 }
 auto previous=mask.endpoint(io,point,rng);
 Timer terminal_time(measurement.seconds[6]);perf5::Scope terminal_scope(perf5::Endpoint);
 std::vector<Hidden>ends;for(unsigned k=0;k<5;++k)ends.push_back(exchangeHidden(io,role+"/endpoint/"+std::to_string(k),io.prover()?(n==1?factors[k](0):folded[k][0]):Fr(0),rng));
 std::vector<Hidden>w=ends;
 for(unsigned b=0;b<2;++b)w.push_back(exchangeHidden(io,role+"/terminal/product/"+std::to_string(b),io.prover()?ends[1+2*b].value*ends[2+2*b].value:Fr(0),rng));
 for(unsigned b=0;b<2;++b)w.push_back(exchangeHidden(io,role+"/terminal/weighted-input/"+std::to_string(b),io.prover()?ends[0].value*w[5+b].value:Fr(0),rng));
 w.push_back(previous);Relations terminal;terminal.role=role+"/terminal";for(auto&h:w)terminal.handles.push_back(h.handle);terminal.products={{1,2,5},{0,5,7},{3,4,6},{0,6,8}};terminal.linear={{Fr(0),Fr(0),Fr(0),Fr(0),Fr(0),Fr(0),Fr(0),alpha[0],alpha[1],Fr(-1)}};
 sigma(io,terminal,io.prover()?&w:nullptr,rng);
 return {Reduction{initial[0],{ends[0],ends[1],ends[2]},point},Reduction{initial[1],{ends[0],ends[3],ends[4]},point}};
}
Reduction single(IO&io,std::size_t n,const Hidden&initial,const std::vector<Value>&factors,Random*rng,const std::string&role,const ProductInputs*inputs,bool specialized){
 need(n && n<=(1u<<24) && !(n&(n-1)),"masked Product2 domain");need(!io.prover() || (rng && factors.size()==3),"masked Product2 witness boundary");
 const auto options=perf6::options();perf6::ScopedMessages messages(options.b2);
 Measure measurement{{},role,io.prover()};Writer statement;statement.text("R7-LASSO-MASKED-PRODUCT2-HIDDEN-A3-RESEARCH-v1");statement.number(n,8);statement.number(3);statement.text(hyrax_a3::suite);validate(initial.handle,io.session(),initial.handle.role);statement.raw(encode(initial.handle));bindPublic(io,role+"/statement",statement.b);
 Mask mask(io,r1::log2exact(n),initial,rng,role+"/masked",measurement);
 std::vector<std::vector<Fr>>folded(3);std::vector<Fr>point;std::size_t current=n;unsigned round=0;
 while(current>1){
  Timer timer(measurement.seconds[4]);std::vector<Fr>coeff(4,Fr(0));
  if(io.prover()){
   perf5::Scope polynomial_time(perf5::Polynomial);
   if(specialized)coeff=inputs && !round?detail::productPolynomialContiguous(*inputs):detail::productPolynomial(current,factors,folded,!round);
   else{
    auto reads=factors;
    if(inputs && !round)reads={[inputs](std::size_t i){return inputs->equality[i];},[inputs](std::size_t i){return inputs->children[2*i];},[inputs](std::size_t i){return inputs->children[2*i+1];}};
    coeff=detail::expressionPolynomial(Expression{3,{{Fr(1),{0,1,2}}}},current,reads,folded,!round);
   }
   if(auto*c=perf5::context){if(specialized){++c->a2_calls;c->a2_pairs+=current/2;}else ++c->generic_product_calls;}
  }
  auto challenge=mask.exchangeRound(io,coeff,round,point);
  {perf5::Scope folding(perf5::Fold);if(io.prover()){
   if(inputs && !round)detail::foldProductInputs(*inputs,challenge,folded);
   else for(unsigned k=0;k<3;++k){if(!round){folded[k].resize(current/2);for(std::size_t j=0;j<current/2;++j){auto a=factors[k](2*j);folded[k][j]=a+challenge*(factors[k](2*j+1)-a);}}
    else{for(std::size_t j=0;j<current/2;++j)folded[k][j]=folded[k][2*j]+challenge*(folded[k][2*j+1]-folded[k][2*j]);folded[k].resize(current/2);}
   }
  }}
  point.push_back(challenge);current/=2;++round;
 }
 auto previous=mask.endpoint(io,point,rng);
 Timer terminal_time(measurement.seconds[6]);perf5::Scope terminal_scope(perf5::Endpoint);
 std::vector<Hidden>ends;for(unsigned k=0;k<3;++k)ends.push_back(exchangeHidden(io,role+"/endpoint/"+std::to_string(k),io.prover()?(n==1?factors[k](0):folded[k][0]):Fr(0),rng));
 originalSingleTerminal(io,previous,ends,rng,role+"/terminal");return {initial,ends,point};
}

}
