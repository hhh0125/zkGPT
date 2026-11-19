#include "paper_r7_native_masked.hpp"
#include "paper_r7_hyrax_a3.hpp"
#include "paper_r7_stage2_opening.hpp"
#include <cstdlib>
#include <iostream>
#include <iomanip>
namespace zkgpt::r7::oneblock::native_masked {
namespace {
using Clock=std::chrono::steady_clock;
struct Timer {double&value;Clock::time_point start=Clock::now();explicit Timer(double&v):value(v){}~Timer(){value+=std::chrono::duration<double>(Clock::now()-start).count();}};
}
Mode configuredMode(){
 const auto*p=std::getenv("ZKGPT_NATIVE_GKR_SUMCHECK");if(!p || std::string(p)=="sigma")return Mode::Sigma;
 need(std::string(p)=="masked-research-v1","unknown Native GKR sumcheck version");return Mode::MaskedResearch;
}
PhaseMeasure::~PhaseMeasure(){
 std::cerr<<std::setprecision(12)<<"NATIVE_SUMCHECK {\"role\":"<<std::quoted(role)<<",\"prover\":"<<(prover?"true":"false")<<",\"masked\":"<<(masked?"true":"false")<<",\"rounds\":"<<rounds<<",\"total_seconds\":"<<std::chrono::duration<double>(Clock::now()-started).count()<<",\"seconds\":{";
 const char*names[]={"mask_generation","mask_commitment","sum_opening","initial_boundary","recurrence","endpoint_opening"};
 for(unsigned i=0;i<6;++i){if(i)std::cerr<<',';std::cerr<<std::quoted(names[i])<<':'<<seconds[i];}std::cerr<<"}}\n";
}
Mask::Mask(IO&io,unsigned m,const Hidden&initial,Random*rng,std::string role,PhaseMeasure&measure):variables_(m),role_(std::move(role)),measure_(measure){
 need(m<=24 && (!io.prover() || rng),"Native mask dimensions/entropy");validate(initial.handle,io.session(),initial.handle.role);
 while(padded_<1+2*m)padded_*=2;
 Writer statement;statement.text(suite);statement.number(m);statement.number(2);statement.number(padded_,8);statement.text(hyrax_a3::suite);statement.raw(encode(initial.handle));bindPublic(io,role_+"/statement",statement.b);
 std::vector<Fr>weights(padded_,Fr(0));Fr cube=1;for(unsigned j=0;j<m;++j)cube+=cube;weights[0]=cube;
 if(m){Fr half;Fr::div(half,cube,Fr(2));for(unsigned j=1;j<=2*m;++j)weights[j]=half;}
 {Timer t(measure_.seconds[0]);if(io.prover()){coefficients_.assign(padded_,Fr(0));for(unsigned j=0;j<=2*m;++j)coefficients_[j]=rng->scalar();blind_=rng->scalar();}}
 Hidden sum;
 {Timer t(measure_.seconds[1]);Writer w;if(io.prover()){commitment_=hyrax_a3::vectorCommit(coefficients_,blind_);w.point(commitment_);}auto b=io.message(role_+"/mask-vector",w.b);Reader r{b};commitment_=r.point();r.end();sum=exchangeHidden(io,role_+"/sum",io.prover()?r1::dot(coefficients_,weights):Fr(0),rng);}
 {Timer t(measure_.seconds[2]);open(io,sum,weights,rng,role_+"/sum-opening");}
 rho_=io.coin(role_+"/rho");need(!rho_.isZero(),"Native mask rho must be nonzero");
 {Timer t(measure_.seconds[3]);Writer w;if(io.prover())w.scalar(initial.value+rho_*sum.value);auto b=io.message(role_+"/initial-claim",w.b);Reader r{b};claim_=r.scalar();r.end();
  std::vector<Hidden>h{initial,sum,publicHidden(io,role_+"/public-initial",claim_)};Relations relation;relation.role=role_+"/initial-boundary";for(auto&v:h)relation.handles.push_back(v.handle);relation.linear={{Fr(1),rho_,Fr(-1)}};sigma(io,relation,io.prover()?&h:nullptr,rng);
 }
}
void Mask::open(IO&io,const Hidden&h,const std::vector<Fr>&weights,Random*rng,const std::string&name){
 hyrax_a3::DotWitness witness;if(io.prover()){witness.values=coefficients_;witness.blind=blind_;witness.evaluation=h;}
 stage2::ScopedIO scoped(io,name);hyrax_a3::dotProduct(scoped,commitment_,h.handle,weights,io.prover()?&witness:nullptr,rng);
}
Fr Mask::round(IO&io,const std::array<Fr,3>&original){
 need(!finished_ && point_.size()<variables_,"Native mask round state");unsigned i=point_.size();Writer w;
 if(io.prover()){
  Fr scale=1;for(unsigned j=i+1;j<variables_;++j)scale+=scale;
  Fr constant=coefficients_[0];for(unsigned j=0;j<i;++j)constant+=coefficients_[1+2*j]*point_[j]+coefficients_[2+2*j]*point_[j]*point_[j];constant*=scale;
  if(i+1<variables_){Fr half;Fr::div(half,scale,Fr(2));for(unsigned j=i+1;j<variables_;++j)constant+=half*(coefficients_[1+2*j]+coefficients_[2+2*j]);}
  w.scalar(original[0]+rho_*constant);w.scalar(original[1]+rho_*scale*coefficients_[1+2*i]);w.scalar(original[2]+rho_*scale*coefficients_[2+2*i]);
 }
 auto role=role_+"/round/"+std::to_string(i);auto b=io.message(role+"/coefficients",w.b);Reader read{b};std::array<Fr,3>c;for(auto&v:c)v=read.scalar();read.end();
 {Timer t(measure_.seconds[4]);need(claim_==Fr(2)*c[0]+c[1]+c[2],"Native masked recurrence equation failed");}
 auto r=io.coin(role+"/r");claim_=c[0]+r*(c[1]+r*c[2]);point_.push_back(r);++measure_.rounds;return r;
}
Hidden Mask::finish(IO&io,Random*rng){
 need(!finished_ && point_.size()==variables_,"Native mask endpoint state");finished_=true;Timer t(measure_.seconds[5]);
 std::vector<Fr>weights(padded_,Fr(0));weights[0]=Fr(1);for(unsigned j=0;j<variables_;++j){weights[1+2*j]=point_[j];weights[2+2*j]=point_[j]*point_[j];}
 auto evaluation=exchangeHidden(io,role_+"/endpoint-mask",io.prover()?r1::dot(coefficients_,weights):Fr(0),rng);open(io,evaluation,weights,rng,role_+"/endpoint-opening");
 auto result=combine(io.session(),role_+"/original-terminal",{publicHidden(io,role_+"/public-terminal",claim_),evaluation},{Fr(1),-rho_});
 bindPublic(io,role_+"/original-terminal",encode(result.handle));return result;
}
Reduction matrix(IO&io,std::size_t n,const Hidden&initial,const std::vector<Value>&factors,Random*rng,const std::string&role){
 need(n && n<=(1u<<24) && !(n&(n-1)),"Native Matrix domain");need(!io.prover() || (rng && factors.size()==2),"Native Matrix private shape");
 PhaseMeasure measure{role,io.prover(),true};Mask mask(io,r1::log2exact(n),initial,rng,role+"/masked",measure);
 Expression expression{2,{{Fr(1),{0,1}}}};std::vector<std::vector<Fr>>folded(2);std::vector<Fr>point;std::size_t current=n;
 while(current>1){
  std::vector<Fr>c(3,Fr(0));if(io.prover())c=detail::expressionPolynomial(expression,current,factors,folded,point.empty());
  auto r=mask.round(io,{c[0],c[1],c[2]});
  if(io.prover())for(unsigned k=0;k<2;++k){if(point.empty()){folded[k].resize(current/2);for(std::size_t j=0;j<current/2;++j){auto a=factors[k](2*j);folded[k][j]=a+r*(factors[k](2*j+1)-a);}}else{for(std::size_t j=0;j<current/2;++j)folded[k][j]=folded[k][2*j]+r*(folded[k][2*j+1]-folded[k][2*j]);folded[k].resize(current/2);}}
  point.push_back(r);current/=2;
 }
 auto terminal=mask.finish(io,rng);std::vector<Hidden>ends;
 for(unsigned k=0;k<2;++k)ends.push_back(exchangeHidden(io,role+"/endpoint/"+std::to_string(k),io.prover()?(n==1?factors[k](0):folded[k][0]):Fr(0),rng));
 detail::nativeMatrixTerminal(io,terminal,ends,rng,role+"/terminal");
 return {initial,ends,point};
}
}
