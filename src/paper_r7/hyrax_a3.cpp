#include "hyrax_a3.hpp"
#include <algorithm>
#include <chrono>
#include <map>
namespace zkgpt::r7::hyrax_a3 {
namespace {
void dimension(std::size_t n){need(n && n<=maxDot && !(n&(n-1)),"A3 dot dimension must be power-of-two1..1024");}
G1 wideMSM(const std::vector<G1>&g,const std::vector<Fr>&x){
 need(g.size()==x.size() && !g.empty() && g.size()<=maxTrustedR6Dot+1,"A3 MSM dimension");
 auto&m=metrics();++m.msm_calls;m.msm_terms+=g.size();m.max_msm=std::max<std::uint64_t>(m.max_msm,g.size());
 G1 out;G1::mulVec(out,const_cast<G1*>(g.data()),const_cast<Fr*>(x.data()),x.size());return out;
}
void publicBinding(IO&io,const std::string&label,const Bytes&b){measurement().public_payload_bytes+=b.size();need(io.message(label,b)==b,"A3 suite/version/public input mismatch");}
Bytes exchange(IO&io,const std::string&label,const Bytes&b,bool core){auto out=io.message(label,b);auto&m=measurement();(core?m.core_payload_bytes:m.guard_payload_bytes)+=out.size();return out;}
Fr challenge(IO&io,const std::string&label){auto c=io.coin(label);need(!c.isZero(),"A3 zero verifier challenge");return c;}
// Independent generalized Schnorr proofs for the TWO input commitments.
// Not a modification of Fig7/8; without this precondition xi+tau cancellation
// must not be interpreted as knowledge of the two original openings.
void knowledge(IO&io,const G1&xi,const Handle&tau,const std::vector<G1>&g,const DotWitness*w,Random*rng){
 auto p=parameters(1);std::vector<Fr>d;Fr rv=0,dy=0,ry=0;Writer masks;
 if(io.prover()){
  for(std::size_t i=0;i<g.size();++i){d.push_back(rng->scalar());}
  rv=rng->scalar();dy=rng->scalar();ry=rng->scalar();
  masks.point(wideMSM(g,d)+p.hiding*rv);masks.point(scalarCommit(dy,ry));
 }
 auto b=exchange(io,"A3/knowledge/masks",masks.b,false);Reader mr{b};auto D=mr.point(),B=mr.point();mr.end();auto c=challenge(io,"A3/knowledge/c");
 Writer response;
 if(io.prover()){for(std::size_t i=0;i<g.size();++i)response.scalar(d[i]+c*w->values[i]);response.scalar(rv+c*w->blind);response.scalar(dy+c*w->evaluation.value);response.scalar(ry+c*w->evaluation.blind);}
 b=exchange(io,"A3/knowledge/responses",response.b,false);Reader rr{b};std::vector<Fr>z;for(std::size_t i=0;i<g.size();++i)z.push_back(rr.scalar());auto zv=rr.scalar(),zy=rr.scalar(),zr=rr.scalar();rr.end();
 if(!io.prover()){
  need(wideMSM(g,z)+p.hiding*zv==D+xi*c,"A3 vector knowledge equation failed");
  need(scalarCommit(zy,zr)==B+tau.point*c,"A3 scalar knowledge equation failed");
 }
}
// Literal Appendix A.3 Figures7–8. Public API always establishes knowledge first.
void prooflog(IO&io,const G1&xi,const Handle&tau,const std::vector<Fr>&a,const std::vector<G1>&bases,const DotWitness*w,Random*rng){
 const auto p=parameters(1);auto U=xi+tau.point;auto g=bases;auto aa=a;std::vector<Fr>x;Fr y=0,blind=0;
 if(io.prover()){x=w->values;y=r1::dot(x,a);blind=w->blind+w->evaluation.blind;}
 std::vector<Fr>folds;std::size_t width=a.size();unsigned round=0;
 while(width>1){auto half=width/2;Writer cross;Fr rl=0,rr=0,left=0,right=0;
  if(io.prover()){
   std::vector<Fr>xl(x.begin(),x.begin()+half),xr(x.begin()+half,x.end()),al(aa.begin(),aa.begin()+half),ar(aa.begin()+half,aa.end());
   std::vector<G1>gl(g.begin(),g.begin()+half),gr(g.begin()+half,g.end());
   rl=rng->scalar();rr=rng->scalar();left=r1::dot(xl,ar);right=r1::dot(xr,al);
   cross.point(wideMSM(gr,xl)+p.scalar*left+p.hiding*rl);cross.point(wideMSM(gl,xr)+p.scalar*right+p.hiding*rr);
  }
  const auto label="A3/reduce/"+std::to_string(round);auto b=exchange(io,label+"/cross",cross.b,true);Reader lr{b};auto L=lr.point(),R=lr.point();lr.end();auto c=challenge(io,label+"/c");Fr inv;Fr::inv(inv,c);folds.push_back(c);auto c2=c*c,i2=inv*inv;U+=L*c2+R*i2;
  if(io.prover()){
   for(std::size_t i=0;i<half;++i){x[i]=c*x[i]+inv*x[i+half];aa[i]=inv*aa[i]+c*aa[i+half];g[i]=g[i]*inv+g[i+half]*c;}
   x.resize(half);aa.resize(half);g.resize(half);y+=c2*left+i2*right;blind+=c2*rl+i2*rr;
  }
  width=half;++round;
 }
 G1 ghat;Fr ahat;
 if(io.prover()){ghat=g[0];ahat=aa[0];}
 else{
  // Independent verifier: direct product weights by HIGH bit (half-vector split),
  // not the prover's recursive generator fold; one n-base MSM as in A.3 text.
  std::vector<Fr>weights(a.size(),Fr(1));for(std::size_t k=0;k<folds.size();++k){Fr inv;Fr::inv(inv,folds[k]);auto bit=folds.size()-1-k;for(std::size_t i=0;i<weights.size();++i)weights[i]*=((i>>bit)&1)?folds[k]:inv;}
  ghat=wideMSM(bases,weights);ahat=r1::dot(a,weights);
 }
 need(!ahat.isZero(),"A3 zero final coefficient: INCOMPLETE degenerate public vector/challenges");
 Writer masks;Fr d=0,rd=0,rb=0;
 if(io.prover()){d=rng->scalar();rd=rng->scalar();rb=rng->scalar();masks.point(ghat*d+p.hiding*rd);masks.point(p.scalar*d+p.hiding*rb);}
 auto b=exchange(io,"A3/final/delta-beta",masks.b,true);Reader mr{b};auto delta=mr.point(),beta=mr.point();mr.end();auto c=challenge(io,"A3/final/c");Writer response;
 if(io.prover()){response.scalar(d+c*y);response.scalar(ahat*(c*blind+rb)+rd);}
 b=exchange(io,"A3/final/z1-z2",response.b,true);Reader zr{b};auto z1=zr.scalar(),z2=zr.scalar();zr.end();
 if(!io.prover())need((U*c+beta)*ahat+delta==(ghat+p.scalar*ahat)*z1+p.hiding*z2,"A3 Schnorr equation failed");
}
}
Measurement& measurement(){static Measurement m;return m;}
std::vector<G1> generators(std::size_t n){
 dimension(n);auto small=parameters(1);(void)small; // pin exact existing curve/map/encoding
 static std::map<std::size_t,std::vector<G1>>cache;auto found=cache.find(n);if(found!=cache.end())return found->second;
 auto g=parameters(std::min<std::size_t>(n,64)).columns;
 for(std::size_t i=g.size();i<n;++i){G1 point;mcl::bn::hashAndMapToG1(point,std::string(crypto::suite)+"/mcl-hashAndMapToG1/MCL_MAP_TO_MODE_ORIGINAL/column/"+std::to_string(i));need(point.isValid() && point.isValidOrder() && !point.isZero(),"A3 derived generator invalid");g.push_back(point);}
 auto all=g;all.push_back(small.scalar);all.push_back(small.hiding);for(std::size_t i=0;i<all.size();++i)for(std::size_t j=0;j<i;++j)need(all[i]!=all[j] && all[i]!=-all[j],"A3 generator collision");
 cache.emplace(n,g);return g;
}
G1 vectorCommit(const std::vector<Fr>&x,const Fr&r){auto g=generators(x.size());for(auto&v:x)need(v.isValid(),"A3 invalid witness scalar");need(r.isValid(),"A3 invalid vector blind");return wideMSM(g,x)+parameters(1).hiding*r;}
void dotProduct(IO&io,const G1&xi,const Handle&tau,const std::vector<Fr>&a,const DotWitness*w,Random*rng){
 dimension(a.size());validate(tau,io.session(),tau.role);need(std::any_of(a.begin(),a.end(),[](const Fr&f){return !f.isZero();}),"A3 zero public vector unsupported: INCOMPLETE");
 Writer binding;binding.text(suite);binding.text(messageVersion);binding.text(crypto::suite);binding.text(io.session());auto parent=io.prefix();binding.raw(Bytes(parent.begin(),parent.end()));binding.number(a.size());binding.point(xi);binding.raw(encode(tau));for(auto&f:a)binding.scalar(f);
 publicBinding(io,"A3/statement",binding.b);auto g=generators(a.size());
 if(io.prover())need(w && rng && w->values.size()==a.size() && encode(w->evaluation.handle)==encode(tau),"A3 private input/handle mismatch");
 auto start=std::chrono::steady_clock::now();knowledge(io,xi,tau,g,w,rng);auto next=std::chrono::steady_clock::now();measurement().guard_seconds+=std::chrono::duration<double>(next-start).count();prooflog(io,xi,tau,a,g,w,rng);measurement().core_seconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-next).count();
}
void dotProductTrustedR6Basis(IO&io,const r1::PCSpec&approved,const G1&xi,const Handle&tau,const std::vector<Fr>&a,const DotWitness*w,Random*rng){
 approved.validate();need(approved.layout==r1::Layout::ColumnMajor && a.size()==approved.columns && a.size() && a.size()<=maxTrustedR6Dot && !(a.size()&(a.size()-1)),"A3 trusted R6 dimension/layout");
 validate(tau,io.session(),tau.role);need(std::any_of(a.begin(),a.end(),[](const Fr&f){return !f.isZero();}),"A3 zero public vector unsupported: INCOMPLETE");
 Writer binding;binding.text(trustedR6Suite);binding.text(messageVersion);binding.text(crypto::suite);binding.text(io.session());auto parent=io.prefix();binding.raw(Bytes(parent.begin(),parent.end()));binding.text(approved.role);binding.number(approved.actual,8);binding.number(approved.padded,8);binding.number(approved.rows,8);binding.number(approved.columns,8);binding.number(static_cast<unsigned>(approved.layout));binding.point(xi);binding.raw(encode(tau));for(auto&f:a)binding.scalar(f);
 publicBinding(io,"A3/R6/statement",binding.b);
 // No generator bytes are accepted from the prover. The anchor spec is supplied
 // by the trusted-model verifier, and R1's role-specific derivation is exact.
 const auto&g=r1::generators(approved).g;
 if(io.prover())need(w && rng && w->values.size()==a.size() && encode(w->evaluation.handle)==encode(tau),"A3 private input/handle mismatch");
 auto start=std::chrono::steady_clock::now();knowledge(io,xi,tau,g,w,rng);auto next=std::chrono::steady_clock::now();measurement().guard_seconds+=std::chrono::duration<double>(next-start).count();prooflog(io,xi,tau,a,g,w,rng);measurement().core_seconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-next).count();
}
void opening(IO&io,const PC&pc,const Query&q,const PCSecret*secret,const Hidden*evaluation,Random*rng){
 validate(pc);validate(q.evaluation,io.session(),q.evaluation.role);need(q.point.size()==r1::log2exact(pc.spec.padded),"A3 PC query dimension");
 Writer b;b.text(suite);b.text(messageVersion);b.raw(encode(pc));b.raw(encode(q.evaluation));b.number(q.point.size());for(auto&f:q.point)b.scalar(f);publicBinding(io,"A3/PC-statement",b.b);
 auto rowbits=r1::log2exact(pc.spec.rows);std::vector<Fr>rl(q.point.begin(),q.point.begin()+rowbits),rc(q.point.begin()+rowbits,q.point.end());auto L=r1::weights(rl),a=r1::weights(rc);auto xi=msm(pc.rows,L);DotWitness w;
 if(io.prover()){
  need(secret && evaluation && rng && encode(secret->public_pc)==encode(pc) && encode(evaluation->handle)==encode(q.evaluation) && secret->values.size()==pc.spec.padded && secret->blinds.size()==pc.spec.rows,"A3 PC private identity/shape mismatch");
  w.values.assign(pc.spec.columns,Fr(0));w.evaluation=*evaluation;for(std::size_t i=0;i<pc.spec.rows;++i){w.blind+=L[i]*secret->blinds[i];for(std::size_t j=0;j<pc.spec.columns;++j)w.values[j]+=L[i]*secret->values[i+pc.spec.rows*j];}
 }
 dotProduct(io,xi,q.evaluation,a,io.prover()?&w:nullptr,rng);
}
}
