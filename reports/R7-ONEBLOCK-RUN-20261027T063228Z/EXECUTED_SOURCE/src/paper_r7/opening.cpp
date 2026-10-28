#include "crypto.hpp"
#include <set>
namespace zkgpt::r7::crypto {
namespace {
void core(IO&io,const std::string&label,const G1&xi,const Handle&Y,const std::vector<Fr>&a,const std::vector<Fr>*x,const Fr&rho,const Fr&eta,Random*rng){
 auto p=parameters(a.size());std::vector<Fr>d,z;Fr delta=0,beta=0;Writer first;
 if(io.prover()){need(x && x->size()==a.size() && rng,"R7 opening private shape");for(std::size_t j=0;j<a.size();++j)d.push_back(rng->scalar());delta=rng->scalar();beta=rng->scalar();first.point(msm(p.columns,d)+p.hiding*delta);first.point(scalarCommit(r1::dot(a,d),beta));}
 auto bytes=io.message(label+"/mask",first.b);Reader f{bytes};auto A=f.point(),B=f.point();f.end();auto c=io.coin(label+"/c");need(!c.isZero(),"R7 zero opening challenge");
 Writer response;
 if(io.prover()){for(std::size_t j=0;j<a.size();++j)z.push_back(d[j]+c*x->at(j));response.scalar(delta+c*rho);response.scalar(beta+c*eta);response.scalar(r1::dot(a,z));}
 bytes=io.message(label+"/masked-response",response.b);Reader r{bytes};auto s=r.scalar(),u=r.scalar(),t=r.scalar();r.end();
 need(Y.point*c+B==scalarCommit(t,u),"R7 opening masked evaluation equation failed");
 auto Rz=xi*c+A-p.hiding*s;auto chi=io.coin(label+"/response-ipa/chi");need(!chi.isZero(),"R7 zero IPA challenge");
 auto U=Rz+p.scalar*(chi*t);auto g=p.columns;auto weights=a;unsigned round=0;
 while(g.size()>1){auto n=g.size()/2;Writer cross;
  if(io.prover()){std::vector<G1>gl(g.begin(),g.begin()+n),gr(g.begin()+n,g.end());std::vector<Fr>zl(z.begin(),z.begin()+n),zr(z.begin()+n,z.end()),al(weights.begin(),weights.begin()+n),ar(weights.begin()+n,weights.end());cross.point(msm(gr,zl)+p.scalar*(chi*r1::dot(zl,ar)));cross.point(msm(gl,zr)+p.scalar*(chi*r1::dot(zr,al)));}
  bytes=io.message(label+"/ipa/"+std::to_string(round)+"/LR",cross.b);Reader lr{bytes};auto L=lr.point(),R=lr.point();lr.end();auto fold=io.coin(label+"/ipa/"+std::to_string(round)+"/fold");need(!fold.isZero(),"R7 zero IPA fold");Fr inverse;Fr::inv(inverse,fold);U+=L*(fold*fold)+R*(inverse*inverse);
  for(std::size_t j=0;j<n;++j){g[j]=g[j]*inverse+g[j+n]*fold;weights[j]=weights[j]*inverse+weights[j+n]*fold;if(io.prover())z[j]=z[j]*fold+z[j+n]*inverse;}
  g.resize(n);weights.resize(n);if(io.prover())z.resize(n);++round;
 }
 Writer final;if(io.prover())final.scalar(z[0]);bytes=io.message(label+"/ipa/final-masked-response",final.b);Reader end{bytes};auto last=end.scalar();end.end();
 need(U==(g[0]+p.scalar*(chi*weights[0]))*last,"R7 response IPA terminal equation failed");
}
}
void opening(IO&io,const PC&pc,const std::vector<Query>&queries,const PCSecret*secret,const std::vector<Hidden>*evaluations,Random*rng){
 validate(pc);need(!queries.empty() && queries.size()<=4,"R7 opening K must be1..4");
 Writer statement;statement.raw(encode(pc));statement.text(io.session());const auto parent=io.prefix();statement.raw(Bytes(parent.begin(),parent.end()));
 std::map<std::string,Bytes>aliases;
 for(auto&q:queries){validate(q.evaluation,io.session(),q.evaluation.role);need(q.point.size()==r1::log2exact(pc.spec.padded),"R7 opening point dimension");statement.raw(encode(q.evaluation));statement.number(q.point.size());for(auto&r:q.point)statement.scalar(r);auto e=encode(q.evaluation);auto it=aliases.emplace(q.evaluation.role,e);need(it.second || it.first->second==e,"R7 conflicting full hidden handle alias");}
 bindPublic(io,"HPC/batch-statement",statement.b);
 if(io.prover())need(secret && evaluations && evaluations->size()==queries.size() && encode(secret->public_pc)==encode(pc),"R7 opening PC witness identity mismatch");
 auto audit=io.coin("HPC/row-audit/beta");std::vector<Fr>rowweights(pc.spec.rows,Fr(1));for(std::size_t i=1;i<rowweights.size();++i)rowweights[i]=rowweights[i-1]*audit;
 auto xi=msm(pc.rows,rowweights);std::vector<Fr>x(pc.spec.columns,Fr(0));Fr rho=0;
 if(io.prover())for(std::size_t i=0;i<pc.spec.rows;++i){rho+=rowweights[i]*secret->blinds.at(i);for(std::size_t j=0;j<pc.spec.columns;++j)x[j]+=rowweights[i]*secret->values.at(i+pc.spec.rows*j);}
 Handle zero;zero.session=io.session();zero.role="HPC/row-audit/zero";zero.point.clear();core(io,"HPC/audit",xi,zero,std::vector<Fr>(pc.spec.columns,Fr(0)),io.prover()?&x:nullptr,rho,Fr(0),rng);
 const auto rowbits=r1::log2exact(pc.spec.rows);
 for(std::size_t k=0;k<queries.size();++k){auto&q=queries[k];std::vector<Fr>rrow(q.point.begin(),q.point.begin()+rowbits),rcol(q.point.begin()+rowbits,q.point.end());auto L=r1::weights(rrow),a=r1::weights(rcol);xi=msm(pc.rows,L);x.assign(pc.spec.columns,Fr(0));rho=0;Fr eta=0;
  if(io.prover()){need(encode(evaluations->at(k).handle)==encode(q.evaluation),"R7 opening complete handle mismatch");eta=evaluations->at(k).blind;for(std::size_t i=0;i<pc.spec.rows;++i){rho+=L[i]*secret->blinds.at(i);for(std::size_t j=0;j<pc.spec.columns;++j)x[j]+=L[i]*secret->values.at(i+pc.spec.rows*j);}}
  core(io,"HPC/eval/"+std::to_string(k),xi,q.evaluation,a,io.prover()?&x:nullptr,rho,eta,rng);
 }
}
}
