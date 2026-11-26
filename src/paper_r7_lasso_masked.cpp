#include "paper_r7_lasso_masked.hpp"
#include "paper_r7_hyrax_a3.hpp"
#include "paper_r7_stage2_opening.hpp"
#include "paper_r7_perf5_profile.hpp"
#include "paper_r7_product_kernels.hpp"
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
 std::string role;Measure&measurement;Fr known_prefix=0,future_sum=0;
 Mask(IO&io,unsigned variables,const Hidden&initial,Random*rng,const std::string&name,Measure&measure):m(variables),role(name),measurement(measure){
  while(padded<1+3*m)padded*=2;
  std::vector<Fr>t(padded,Fr(0));t[0]=Fr(std::uint32_t(1)<<m);
  if(m)std::fill(t.begin()+1,t.begin()+1+3*m,Fr(std::uint32_t(1)<<(m-1)));
  if(io.prover()){Timer timer(measure.seconds[0]);perf5::Scope generation(perf5::MaskGeneration);
   need(rng,"mask entropy required");a.assign(padded,Fr(0));
   for(unsigned i=0;i<=3*m;++i)a[i]=rng->scalar();
   blind=rng->scalar();
   known_prefix=a[0];
   for(unsigned i=1;i<=3*m;++i)future_sum+=a[i];}
  {Timer timer(measure.seconds[1]);perf5::Scope commit(perf5::MaskCommitment);Writer w;if(io.prover()){commitment=hyrax_a3::vectorCommit(a,blind);w.point(commitment);}auto bytes=io.message(role+"/mask-vector",w.b);Reader r{bytes};commitment=r.point();r.end();sum=exchangeHidden(io,role+"/sum",io.prover()?r1::dot(a,t):Fr(0),rng);}
  {Timer timer(measure.seconds[2]);perf5::Scope opening(perf5::MaskSumOpening);open(io,sum,t,rng,role+"/sum-opening");}
  rho=io.coin(role+"/rho");
  {Timer timer(measure.seconds[3]);Writer w;if(io.prover())w.scalar(initial.value+rho*sum.value);auto bytes=io.message(role+"/initial-claim",w.b);Reader r{bytes};claim=r.scalar();r.end();
   linear(io,{initial,sum,publicHidden(io,role+"/public-initial",claim)},{Fr(1),rho,Fr(-1)},rng,role+"/initial-boundary");
  }
 }
 void open(IO&io,const Hidden&h,const std::vector<Fr>&weights,Random*rng,const std::string&name){
  hyrax_a3::DotWitness witness;if(io.prover()){witness.values=a;witness.blind=blind;witness.evaluation=h;}
  perf5::Scope a3(perf5::A3);
  stage2::ScopedIO scoped(io,name);
  auto* workers=io.prover() && padded>=32?&detail::nativeWorkers():nullptr;
  hyrax_a3::dotProduct(scoped,commitment,h.handle,weights,io.prover()?&witness:nullptr,rng,workers);
 }
 std::array<Fr,4>round(unsigned i){
  need(i<m,"mask round index");
  const auto remaining=m-i-1;
  const Fr scale=Fr(std::uint32_t(1)<<remaining);
  const auto* local=a.data()+1+3*i;
  future_sum-=local[0]+local[1]+local[2];
  std::array<Fr,4> out{scale*known_prefix,scale*local[0],scale*local[1],scale*local[2]};
  if(remaining)out[0]+=Fr(std::uint32_t(1)<<(remaining-1))*future_sum;
  return out;
 }
 Fr exchangeRound(IO&io,std::vector<Fr>coeff,unsigned i){
  Writer w;if(io.prover()){auto q=round(i);for(unsigned k=0;k<4;++k)w.scalar(coeff[k]+rho*q[k]);}
  auto name=role+"/round/"+std::to_string(i);
  {
   perf5::Scope transcript(perf5::Transcript);
   auto bytes=io.message(name+"/coefficients",w.b);Reader reader{bytes};for(auto&x:coeff)x=reader.scalar();reader.end();
  }
  {perf5::Scope recurrence(perf5::Recurrence);need(claim==Fr(2)*coeff[0]+coeff[1]+coeff[2]+coeff[3],"masked sumcheck recurrence equation failed");}
  Fr challenge;
  {perf5::Scope transcript(perf5::Transcript);challenge=io.coin(name+"/r");}
  claim=coeff[0]+challenge*(coeff[1]+challenge*(coeff[2]+challenge*coeff[3]));
  if(io.prover()){
   const auto* local=a.data()+1+3*i;
   known_prefix+=challenge*(local[0]+challenge*(local[1]+challenge*local[2]));
  }
  return challenge;
 }
 Hidden endpoint(IO&io,const std::vector<Fr>&r,Random*rng){
  Timer timer(measurement.seconds[5]);perf5::Scope opening(perf5::MaskEndpointOpening);need(r.size()==m,"mask endpoint dimension");std::vector<Fr>weights(padded,Fr(0));weights[0]=Fr(1);
  for(unsigned j=0;j<m;++j){Fr power=r[j];for(unsigned k=0;k<3;++k){weights[1+3*j+k]=power;power*=r[j];}}
  auto value=exchangeHidden(io,role+"/endpoint-mask",io.prover()?r1::dot(a,weights):Fr(0),rng);
  open(io,value,weights,rng,role+"/endpoint-opening");
  auto out=combine(io.session(),role+"/original-terminal",{publicHidden(io,role+"/public-terminal",claim),value},{Fr(1),-rho});
  bindPublic(io,role+"/original-terminal",encode(out.handle));return out;
 }
};
}
std::array<Reduction,2> batch(IO&io,std::size_t n,const std::array<Hidden,2>&initial,const std::vector<Value>&factors,Random*rng,const std::string&role,const std::array<ProductInputs,2>*inputs,const std::vector<Fr>*equality_point){
 need(n && n<=(1u<<24) && !(n&(n-1)),"masked Product01 domain");
 if(equality_point)need(equality_point->size()==r1::log2exact(n),"masked equality metadata");
 need(!io.prover() || (rng && factors.size()==5),"masked Product01 witness boundary");
 perf6::ScopedMessages messages(perf6::enabled("ZKGPT_PERF6_B2"));
 Measure measurement{{},role,io.prover()};Writer statement;statement.text("R7-LASSO-MASKED-PRODUCT01-HIDDEN-A3-RESEARCH-v1");statement.number(n,8);statement.number(3);statement.text("a0,ai1,ai2,ai3;zero-query-padding");statement.text(hyrax_a3::suite);
 for(auto&h:initial){need(h.handle.session==io.session(),"R7 HiddenScalar session/role identity mismatch");statement.raw(encode(h.handle));}bindPublic(io,role+"/statement",statement.b);
 std::vector<Fr>alpha{io.coin(role+"/batch-alpha/0"),io.coin(role+"/batch-alpha/1")};
 auto aggregate=combine(io.session(),role+"/aggregate-initial",{initial[0],initial[1]},alpha);bindPublic(io,role+"/aggregate-initial",encode(aggregate.handle));
 Mask mask(io,r1::log2exact(n),aggregate,rng,role+"/masked",measurement);
 std::vector<std::vector<Fr>>folded(5);std::vector<Fr>point;Fr equality_prefix=1;std::size_t current=n;unsigned round=0;
 while(current>1){
  Timer timer(measurement.seconds[4]);std::vector<Fr>coeff(4,Fr(0));
  if(io.prover()){
   perf5::Scope polynomial_time(perf5::Polynomial);
   perf5::Scope round_time(round?perf5::PolynomialLater:perf5::PolynomialFirst);
   const auto values=detail::batchProductRound(current,round,factors,folded,inputs,{alpha[0],alpha[1]},
                                              equality_point?&(*equality_point)[round]:nullptr,equality_prefix);
   std::copy(values.begin(),values.end(),coeff.begin());
  }
  auto challenge=mask.exchangeRound(io,coeff,round);
  if(io.prover())detail::foldBatchedProductRound(current,round,challenge,factors,folded,inputs,
                                            equality_point?&(*equality_point)[round]:nullptr,equality_prefix);
  point.push_back(challenge);current/=2;++round;
 }
 auto previous=mask.endpoint(io,point,rng);
 Timer terminal_time(measurement.seconds[6]);perf5::Scope terminal_scope(perf5::Endpoint);
 std::vector<Hidden>ends;for(unsigned k=0;k<5;++k)ends.push_back(exchangeHidden(io,role+"/endpoint/"+std::to_string(k),io.prover()?(n==1?factors[k](0):k==0 && equality_point?equality_prefix:folded[k][0]):Fr(0),rng));
 std::vector<Hidden>w=ends;
 for(unsigned b=0;b<2;++b)w.push_back(exchangeHidden(io,role+"/terminal/product/"+std::to_string(b),io.prover()?ends[1+2*b].value*ends[2+2*b].value:Fr(0),rng));
 for(unsigned b=0;b<2;++b)w.push_back(exchangeHidden(io,role+"/terminal/weighted-input/"+std::to_string(b),io.prover()?ends[0].value*w[5+b].value:Fr(0),rng));
 w.push_back(previous);Relations terminal;terminal.role=role+"/terminal";for(auto&h:w)terminal.handles.push_back(h.handle);terminal.products={{1,2,5},{0,5,7},{3,4,6},{0,6,8}};terminal.linear={{Fr(0),Fr(0),Fr(0),Fr(0),Fr(0),Fr(0),Fr(0),alpha[0],alpha[1],Fr(-1)}};
 sigma(io,terminal,io.prover()?&w:nullptr,rng);
 return {Reduction{initial[0],{ends[0],ends[1],ends[2]},point},Reduction{initial[1],{ends[0],ends[3],ends[4]},point}};
}
Reduction single(IO&io,std::size_t n,const Hidden&initial,const std::vector<Value>&factors,Random*rng,const std::string&role,const ProductInputs*inputs,bool){
 need(n && n<=(1u<<24) && !(n&(n-1)),"masked Product2 domain");need(!io.prover() || (rng && factors.size()==3),"masked Product2 witness boundary");
 perf6::ScopedMessages messages(perf6::enabled("ZKGPT_PERF6_B2"));
 Measure measurement{{},role,io.prover()};Writer statement;statement.text("R7-LASSO-MASKED-PRODUCT2-HIDDEN-A3-RESEARCH-v1");statement.number(n,8);statement.number(3);statement.text(hyrax_a3::suite);need(initial.handle.session==io.session(),"R7 HiddenScalar session/role identity mismatch");statement.raw(encode(initial.handle));bindPublic(io,role+"/statement",statement.b);
 Mask mask(io,r1::log2exact(n),initial,rng,role+"/masked",measurement);
 std::vector<std::vector<Fr>>folded(3);std::vector<Fr>point;std::size_t current=n;unsigned round=0;
 while(current>1){
  Timer timer(measurement.seconds[4]);std::vector<Fr>coeff(4,Fr(0));
  if(io.prover()){
   perf5::Scope polynomial_time(perf5::Polynomial);
   perf5::Scope round_time(round?perf5::PolynomialLater:perf5::PolynomialFirst);
   coeff=inputs && !round?detail::productPolynomialContiguous(*inputs):detail::productPolynomial(current,factors,folded,!round);
   if(auto*c=perf5::context){
    ++c->a2_calls;c->a2_pairs+=current/2;
    if(!round){if(inputs){++c->c2_polynomial_calls;c->c2_polynomial_pairs+=current/2;}else c->poly_callbacks+=3*current;}
   }
  }
  auto challenge=mask.exchangeRound(io,coeff,round);
  if(io.prover()){
   perf5::Scope folding(perf5::Fold);
   perf5::Scope round_time(round?perf5::FoldLater:perf5::FoldFirst);
   if(!round && perf5::context){
    if(inputs){++perf5::context->c2_fold_calls;perf5::context->c2_fold_pairs+=current/2;}
    else perf5::context->fold_callbacks+=3*current;
   }
   auto load=[&](unsigned k,std::size_t i){return inputs?detail::readProductInput(*inputs,k,i):factors[k](i);};
   detail::foldNativeProduct(current,challenge,folded,!round,load,detail::nativeWorkers());
  }
  point.push_back(challenge);current/=2;++round;
 }
 auto previous=mask.endpoint(io,point,rng);
 Timer terminal_time(measurement.seconds[6]);perf5::Scope terminal_scope(perf5::Endpoint);
 std::vector<Hidden>ends;for(unsigned k=0;k<3;++k)ends.push_back(exchangeHidden(io,role+"/endpoint/"+std::to_string(k),io.prover()?(n==1?factors[k](0):folded[k][0]):Fr(0),rng));
 originalSingleTerminal(io,previous,ends,rng,role+"/terminal");return {initial,ends,point};
}

}
