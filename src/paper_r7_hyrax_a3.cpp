#include "paper_r7_hyrax_a3.hpp"
#include "paper_r7_a3_fold.hpp"
#include "paper_r7_perf4_profile.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <mutex>
#include <set>
namespace zkgpt::r7::hyrax_a3 {
namespace {
using Clock=std::chrono::steady_clock;
double seconds(Clock::time_point start){return std::chrono::duration<double>(Clock::now()-start).count();}
struct DimensionStats {
 std::size_t generator_requests=0,prefix_hits=0,derived=0,copy_bytes=0,weight_calls=0,weight_muls=0;
 double generator_seconds=0,weights_seconds=0,terminal_seconds=0;
};
std::array<DimensionStats,17>&optimizationStats(){static std::array<DimensionStats,17>stats;return stats;}
DimensionStats&statsFor(std::size_t n){unsigned bits=0;while(n>1){n>>=1;++bits;}return optimizationStats()[bits];}
std::mutex&generatorMutex(){static std::mutex mutex;return mutex;}
void dimension(std::size_t n){need(n && n<=maxDot && !(n&(n-1)),"A3 dot dimension must be power-of-two1..65536");}
G1 wideMSM(const G1*g,const Fr*x,std::size_t n){
 perf4::Scope phase("IPA/MSM","wideMSM",n);
 auto&m=metrics();++m.msm_calls;m.msm_terms+=n;m.max_msm=std::max<std::uint64_t>(m.max_msm,n);
 // In the bundled MCL, both GLV paths build private tables. The fallback
 // mulVecN also takes const point/scalar inputs despite mulVec's signature.
 MSMTrace trace(n);G1 out;G1::mulVec(out,const_cast<G1*>(g),x,n);return out;
}
void publicBinding(IO&io,const std::string&label,const Bytes&b){measurement().public_payload_bytes+=b.size();need(io.message(label,b)==b,"A3 suite/version/public input mismatch");}
Bytes exchange(IO&io,const std::string&label,const Bytes&b,bool core){auto out=io.message(label,b);auto&m=measurement();(core?m.core_payload_bytes:m.guard_payload_bytes)+=out.size();return out;}
Fr challenge(IO&io,const std::string&label){auto c=io.coin(label);need(!c.isZero(),"A3 zero verifier challenge");return c;}
// Independent generalized Schnorr proofs for the TWO input commitments.
// Not a modification of Fig7/8; without this precondition xi+tau cancellation
// must not be interpreted as knowledge of the two original openings.
void knowledge(IO&io,const G1&xi,const Handle&tau,const G1*g,std::size_t n,const DotWitness*w,Random*rng){
 perf4::Scope guard("IPA/guard",io.prover()?"P":"V",n);
 auto p=parameters(1);std::vector<Fr>d;Fr rv=0,dy=0,ry=0;Writer masks;
 if(io.prover()){
  for(std::size_t i=0;i<n;++i){d.push_back(rng->scalar());}
  rv=rng->scalar();dy=rng->scalar();ry=rng->scalar();
  masks.point(wideMSM(g,d.data(),n)+p.hiding*rv);masks.point(scalarCommit(dy,ry));
 }
 auto b=exchange(io,"A3/knowledge/masks",masks.b,false);
 G1 D,B;if(!io.prover()){Reader mr{b};D=mr.point();B=mr.point();mr.end();}
 auto c=challenge(io,"A3/knowledge/c");
 Writer response;
 if(io.prover()){for(std::size_t i=0;i<n;++i)response.scalar(d[i]+c*w->values[i]);response.scalar(rv+c*w->blind);response.scalar(dy+c*w->evaluation.value);response.scalar(ry+c*w->evaluation.blind);}
 b=exchange(io,"A3/knowledge/responses",response.b,false);
 if(!io.prover()){
  Reader rr{b};std::vector<Fr>z;for(std::size_t i=0;i<n;++i)z.push_back(rr.scalar());auto zv=rr.scalar(),zy=rr.scalar(),zr=rr.scalar();rr.end();
  need(wideMSM(g,z.data(),n)+p.hiding*zv==D+xi*c,"A3 vector knowledge equation failed");
  need(scalarCommit(zy,zr)==B+tau.point*c,"A3 scalar knowledge equation failed");
 }
}
// Literal Appendix A.3 Figures7–8. Public API always establishes knowledge first.
void prooflog(IO&io,const G1&xi,const Handle&tau,const std::vector<Fr>&a,const G1*bases,const DotWitness*w,Random*rng,zkgpt::Workers* workers=nullptr){
 perf4::Scope core("IPA/core",io.prover()?"P":"V",a.size());
 const auto p=parameters(1);G1 U;if(!io.prover())U=xi+tau.point;std::vector<G1>g;std::vector<Fr>aa,x;Fr y=0,blind=0;
 if(io.prover()){perf4::Scope copy("IPA/input-copy","P",a.size(),a.size()*(sizeof(G1)+2*sizeof(Fr)));g.assign(bases,bases+a.size());aa=a;x=w->values;}
 if(io.prover()){{perf4::Scope dot("IPA/inner-product","initial",a.size());y=r1::dot(x,a);}blind=w->blind+w->evaluation.blind;}
 std::vector<Fr>folds,inverses;std::size_t width=a.size();unsigned round=0;
 while(width>1){auto half=width/2;Writer cross;Fr rl=0,rr=0,left=0,right=0;
  if(io.prover()){
   rl=rng->scalar();rr=rng->scalar();
   {
    perf4::Scope dot("IPA/inner-product","cross",width);
    for(std::size_t i=0;i<half;++i){left+=x[i]*aa[i+half];right+=x[i+half]*aa[i];}
   }
   cross.point(wideMSM(g.data()+half,x.data(),half)+p.scalar*left+p.hiding*rl);
   cross.point(wideMSM(g.data(),x.data()+half,half)+p.scalar*right+p.hiding*rr);
  }
  const auto label="A3/reduce/"+std::to_string(round);auto b=exchange(io,label+"/cross",cross.b,true);G1 L,R;if(!io.prover()){Reader lr{b};L=lr.point();R=lr.point();lr.end();}Fr c,inv,c2,i2;{perf4::Scope powers("IPA/challenge-inverse",label,width);c=challenge(io,label+"/c");Fr::inv(inv,c);if(!io.prover()){folds.push_back(c);inverses.push_back(inv);}c2=c*c;i2=inv*inv;}if(!io.prover())U+=L*c2+R*i2;
  if(io.prover()){
   {perf4::Scope phase("IPA/witness-fold",label,half);for(std::size_t i=0;i<half;++i)x[i]=c*x[i]+inv*x[i+half];}
   {perf4::Scope phase("IPA/coefficient-fold",label,half);for(std::size_t i=0;i<half;++i)aa[i]=inv*aa[i]+c*aa[i+half];}
   {perf4::Scope phase("IPA/generator-fold",label,half);detail::foldGenerators(g.data(),half,inv,c,workers);}
   x.resize(half);aa.resize(half);g.resize(half);y+=c2*left+i2*right;blind+=c2*rl+i2*rr;
  }
  width=half;++round;
 }
 G1 ghat;Fr ahat;
 if(io.prover()){ghat=g[0];ahat=aa[0];}
 else{
  // Independent verifier: expand the high-bit product weights and use a
  // single MSM over the original bases.
  auto start=Clock::now();
  auto weights=verifierWeights(folds,inverses);
  const auto weight_seconds=seconds(start);
  start=Clock::now();
  ghat=wideMSM(bases,weights.data(),a.size());ahat=r1::dot(a,weights);
  const auto terminal_seconds=seconds(start);
  const std::lock_guard<std::mutex>lock(generatorMutex());
  auto&stats=statsFor(a.size());
  ++stats.weight_calls;stats.weight_muls+=2*(a.size()-1);
  stats.weights_seconds+=weight_seconds;stats.terminal_seconds+=terminal_seconds;
 }
 need(!ahat.isZero(),"A3 zero final coefficient: INCOMPLETE degenerate public vector/challenges");
 Writer masks;Fr d=0,rd=0,rb=0;
 if(io.prover()){d=rng->scalar();rd=rng->scalar();rb=rng->scalar();masks.point(ghat*d+p.hiding*rd);masks.point(p.scalar*d+p.hiding*rb);}
 auto b=exchange(io,"A3/final/delta-beta",masks.b,true);G1 delta,beta;if(!io.prover()){Reader mr{b};delta=mr.point();beta=mr.point();mr.end();}auto c=challenge(io,"A3/final/c");Writer response;
 if(io.prover()){response.scalar(d+c*y);response.scalar(ahat*(c*blind+rb)+rd);}
 b=exchange(io,"A3/final/z1-z2",response.b,true);
 if(!io.prover()){Reader zr{b};auto z1=zr.scalar(),z2=zr.scalar();zr.end();need((U*c+beta)*ahat+delta==(ghat+p.scalar*ahat)*z1+p.hiding*z2,"A3 Schnorr equation failed");}
}
}
Measurement& measurement(){static Measurement m;return m;}
std::vector<Fr> verifierWeights(const std::vector<Fr>&challenges,const std::vector<Fr>&inverses){
 need(challenges.size()==inverses.size() && challenges.size()<=16,"A3 weight challenge dimensions");
 std::vector<Fr>weights(std::size_t(1)<<challenges.size());
 weights[0]=Fr(1);
 std::size_t width=1;
 for(std::size_t k=0;k<challenges.size();++k){
  for(std::size_t i=width;i-- >0;){
   const Fr prefix=weights[i];
   weights[2*i]=prefix*inverses[k];
   weights[2*i+1]=prefix*challenges[k];
  }
  width*=2;
 }
 return weights;
}
GeneratorView generatorView(std::size_t n){
 dimension(n);
 const auto start=Clock::now();
 const std::lock_guard<std::mutex>lock(generatorMutex());
 // The exact original BN254 suite, mode and index identify every generator.
 // A shorter dimension binds less of the same sequence.
 const auto small=parameters(1);
 static std::shared_ptr<const std::vector<G1>>cache;
 static std::set<Bytes>seen;
 auto&stats=statsFor(n);
 ++stats.generator_requests;
 if(cache && n<=cache->size()){
  ++stats.prefix_hits;stats.generator_seconds+=seconds(start);
  return GeneratorView(cache,n);
 }
 auto points=std::make_shared<std::vector<G1>>();
 auto encodings=seen;
 points->reserve(n);
 if(cache)points->assign(cache->begin(),cache->end());
 auto addIdentity=[&](const G1&point){
  need(!encodings.count(r1::encode(-point)) && encodings.insert(r1::encode(point)).second,"A3 generator collision");
 };
 if(!cache){addIdentity(small.scalar);addIdentity(small.hiding);}
 auto existing=points->size();
 for(std::size_t i=existing;i<n;++i){
  G1 point;
  if(i==0)point=small.columns[0];
  else mcl::bn::hashAndMapToG1(point,std::string(crypto::suite)+"/mcl-hashAndMapToG1/MCL_MAP_TO_MODE_ORIGINAL/column/"+std::to_string(i));
  need(point.isValid() && point.isValidOrder() && !point.isZero(),"A3 derived generator invalid");
  addIdentity(point);
  points->push_back(point);
 }
 G1::normalizeVec(points->data()+existing,points->data()+existing,n-existing);
 seen.swap(encodings);
 cache=std::move(points);
 stats.derived+=n-existing;
 stats.generator_seconds+=seconds(start);
 return GeneratorView(cache,n);
}
std::vector<G1> generators(std::size_t n){
 auto view=generatorView(n);
 const std::lock_guard<std::mutex>lock(generatorMutex());
 statsFor(n).copy_bytes+=n*sizeof(G1);
 return {view.data(),view.data()+n};
}
void writeOptimizationStats(std::ostream&out){
 const std::lock_guard<std::mutex>lock(generatorMutex());
 const auto&stats=optimizationStats();
 for(std::size_t i=0;i<stats.size();++i){
  const auto&s=stats[i];
  if(!s.generator_requests && !s.weight_calls)continue;
  out<<"A3_OPT_STATS {\"dimension\":"<<(std::size_t(1)<<i)<<",\"generator_requests\":"<<s.generator_requests
     <<",\"prefix_hits\":"<<s.prefix_hits<<",\"generators_derived\":"<<s.derived
     <<",\"generator_api_copy_bytes\":"<<s.copy_bytes<<",\"generator_seconds\":"<<s.generator_seconds
     <<",\"verifier_weights_calls\":"<<s.weight_calls<<",\"verifier_weights_muls\":"<<s.weight_muls
     <<",\"verifier_weights_seconds\":"<<s.weights_seconds<<",\"verifier_terminal_seconds\":"<<s.terminal_seconds<<"}\n";
 }
}
G1 vectorCommit(const std::vector<Fr>&x,const Fr&r){auto g=generatorView(x.size());return wideMSM(g.data(),x.data(),g.size())+parameters(1).hiding*r;}
void dotProduct(IO&io,const G1&xi,const Handle&tau,const std::vector<Fr>&a,const DotWitness*w,Random*rng,zkgpt::Workers* workers){
 ++measurement().dot_product_calls;
 dimension(a.size());need(tau.session==io.session(),"R7 HiddenScalar session/role identity mismatch");need(std::any_of(a.begin(),a.end(),[](const Fr&f){return !f.isZero();}),"A3 zero public vector unsupported: INCOMPLETE");
 Writer binding;binding.text(suite);binding.text(messageVersion);binding.text(crypto::suite);binding.text(io.session());auto parent=io.prefix();binding.raw(Bytes(parent.begin(),parent.end()));binding.number(a.size());binding.point(xi);binding.raw(encode(tau));for(auto&f:a)binding.scalar(f);
 publicBinding(io,"A3/statement",binding.b);auto g=generatorView(a.size());
 if(io.prover())need(w && rng && w->values.size()==a.size(),"A3 private input shape");
 auto start=std::chrono::steady_clock::now();knowledge(io,xi,tau,g.data(),g.size(),w,rng);auto next=std::chrono::steady_clock::now();measurement().guard_seconds+=std::chrono::duration<double>(next-start).count();prooflog(io,xi,tau,a,g.data(),w,rng,workers);measurement().core_seconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-next).count();
}
void dotProductTrustedR6Basis(IO&io,const r1::PCSpec&approved,const G1&xi,const Handle&tau,const std::vector<Fr>&a,const DotWitness*w,Random*rng){
 approved.validate();need(approved.layout==r1::Layout::RowMajor && a.size()==approved.columns && a.size() && a.size()<=maxTrustedR6Dot && !(a.size()&(a.size()-1)),"A3 trusted R6 RowMajor dimension/layout");
 need(tau.session==io.session(),"R7 HiddenScalar session/role identity mismatch");need(std::any_of(a.begin(),a.end(),[](const Fr&f){return !f.isZero();}),"A3 zero public vector unsupported: INCOMPLETE");
 Writer binding;binding.text(trustedR6Suite);binding.text(messageVersion);binding.text(crypto::suite);binding.text(io.session());auto parent=io.prefix();binding.raw(Bytes(parent.begin(),parent.end()));binding.text(approved.role);binding.number(approved.actual,8);binding.number(approved.padded,8);binding.number(approved.rows,8);binding.number(approved.columns,8);binding.number(static_cast<unsigned>(approved.layout));binding.point(xi);binding.raw(encode(tau));for(auto&f:a)binding.scalar(f);
 publicBinding(io,"A3/R6-ROWMAJOR/statement",binding.b);
 // No generator bytes are accepted from the prover. The anchor spec is supplied
 // by the trusted-model verifier, and R1's role-specific derivation is exact.
 const auto&g=r1::generators(approved).g;
 if(io.prover())need(w && rng && w->values.size()==a.size(),"A3 private input shape");
 auto start=std::chrono::steady_clock::now();knowledge(io,xi,tau,g.data(),g.size(),w,rng);auto next=std::chrono::steady_clock::now();measurement().guard_seconds+=std::chrono::duration<double>(next-start).count();prooflog(io,xi,tau,a,g.data(),w,rng);measurement().core_seconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-next).count();
}
// Legacy bounded ColumnMajor PC API. Current RowMajor Classified FS and
// Interactive controllers call dotProduct / dotProductTrustedR6Basis through
// openVector and model::opening. crypto::validate rejects any other layout tag.
void opening(IO&io,const PC&pc,const Query&q,const PCSecret*secret,const Hidden*evaluation,Random*rng){
 validate(pc);validate(q.evaluation,io.session(),q.evaluation.role);need(q.point.size()==r1::log2exact(pc.spec.padded),"A3 PC query dimension");
 Writer b;b.text(suite);b.text(messageVersion);b.raw(encode(pc));b.raw(encode(q.evaluation));b.number(q.point.size());for(auto&f:q.point)b.scalar(f);publicBinding(io,"A3/PC-statement",b.b);
 auto rowbits=r1::log2exact(pc.spec.rows);std::vector<Fr>rl(q.point.begin(),q.point.begin()+rowbits),rc(q.point.begin()+rowbits,q.point.end());auto L=r1::weights(rl),a=r1::weights(rc);auto xi=msm(pc.rows,L);DotWitness w;
 if(io.prover()){
  need(secret && evaluation && rng && encode(secret->public_pc)==encode(pc) && secret->values.size()==pc.spec.padded && secret->blinds.size()==pc.spec.rows,"A3 PC private identity/shape mismatch");
  w.values.assign(pc.spec.columns,Fr(0));w.evaluation=*evaluation;for(std::size_t i=0;i<pc.spec.rows;++i){w.blind+=L[i]*secret->blinds[i];for(std::size_t j=0;j<pc.spec.columns;++j)w.values[j]+=L[i]*secret->values[i+pc.spec.rows*j];}
 }
 dotProduct(io,xi,q.evaluation,a,io.prover()?&w:nullptr,rng);
}
}
