#include "oneblock_model.hpp"
#include "paper_r2/r6_model.hpp"
#include "paper_r1/wire.hpp"
#include <algorithm>
#include <iostream>
#include <set>
#include <chrono>
namespace zkgpt::r7::oneblock::model {
namespace {
struct Quiet {std::ostream&s;std::streambuf*saved;struct Sink:std::streambuf{int overflow(int c)override{return c;}}sink;explicit Quiet(std::ostream&out):s(out),saved(out.rdbuf(&sink)){}~Quiet(){s.rdbuf(saved);}};
void shape(const r1::PCSpec&s){s.validate();need(s.layout==r1::Layout::RowMajor && s.columns<=hyrax_a3::maxTrustedR6Dot && s.rows<=4096,"oneblock model approved R6 shape/layout");}
void spec(Writer&w,const r1::PCSpec&s){shape(s);w.text(s.role);w.number(s.actual,8);w.number(s.padded,8);w.number(s.rows,8);w.number(s.columns,8);w.number(static_cast<unsigned>(s.layout));}
r1::PCSpec spec(Reader&r){r1::PCSpec s;s.role=r.text();s.actual=r.number(8);s.padded=r.number(8);s.rows=r.number(8);s.columns=r.number(8);s.layout=static_cast<r1::Layout>(r.number());shape(s);return s;}
void checkTrust(const Trust&t){
 shape(t.anchor.w.spec);need(t.anchor.w.rows.size()==t.anchor.w.spec.rows,"oneblock model anchor row count");
 need(t.anchor_hash.size()==64 && t.manifest_hash.size()==64 && t.manifest_hash==t.anchor.manifest_hash,"oneblock model trusted manifest identity");
 need(r5b::digestHex(r6::encodeAnchor(t.anchor))==t.anchor_hash,"oneblock model trusted anchor bytes changed");
}
Public decode(const Bytes&b){Reader r{b};Public p;p.version=r.text();need(p.version==suite,"oneblock model unknown suite/version");p.session=r.text();p.trust_id=r.text();p.spec=spec(r);need(r.number()==p.spec.rows,"oneblock model wire row count");for(std::size_t i=0;i<p.spec.rows;++i)p.rows.push_back(r.point());r.end();return p;}
G1 modelMSM(const std::vector<G1>&g,const std::vector<Fr>&x){
 need(g.size()==x.size() && !g.empty() && g.size()<=4096,"oneblock model folded-row MSM bounds");auto&m=metrics();++m.msm_calls;m.msm_terms+=g.size();m.max_msm=std::max<std::uint64_t>(m.max_msm,g.size());G1 out;G1::mulVec(out,const_cast<G1*>(g.data()),const_cast<Fr*>(x.data()),x.size());return out;
}
}
Trust loadTrust(const std::string&anchor_path,const std::string&manifest_path){
 Trust t;t.anchor_hash=anchorPin;t.manifest_hash=manifestPin;
 auto manifest=r1::readFile(manifest_path);need(r5b::digestHex(manifest)==manifestPin,"oneblock model independently pinned manifest mismatch");t.manifest=r6::parseManifest(manifest,1);
 // Derive the original operator anchor spec from a fresh, public native R6
 // circuit and parameter registry. Stage2 fixture bytes are never consulted.
 {Quiet quiet(std::cout);LLM n(1,12,64,768,3072);r6::configure(n);auto graph=n.buildPublicCircuit();t.plan=r2::planModelQueries(graph,n.getWitnessRegistry(),n.parameterRegions(),r1::ExpProfile::LegacyClampOne);r6::auditGraph(graph,n,t.plan);t.regions=n.parameterRegions();r6::validateRegions(t.plan,t.regions);}
 t.anchor=r6::loadAnchor(anchor_path,anchorPin,manifest_path,t.plan);checkTrust(t);return t;
}
std::string trustIdentity(const Trust&t){
 checkTrust(t);Writer w;w.text(suite);w.text("operator-trust-not-publisher-certified");w.text(t.anchor_hash);w.text(t.manifest_hash);w.text(t.manifest.revision);w.text(t.manifest.checkpoint);w.text(t.manifest.exporter);w.text(t.anchor.profile);w.text(t.anchor.profile_hash);w.text(t.anchor.circuit);w.text(t.anchor.model_id);spec(w,t.anchor.w.spec);return hex(r1::hash(w.b));
}
Bytes encode(const Public&p){
 need(p.version==suite && p.session.size()==64 && p.trust_id.size()==64,"oneblock model public suite/session/trust identity");shape(p.spec);need(p.rows.size()==p.spec.rows,"oneblock model public row count");Writer w;w.text(p.version);w.text(p.session);w.text(p.trust_id);spec(w,p.spec);w.number(p.rows.size());for(auto&point:p.rows)w.point(point);return w.b;
}
void validate(const Trust&t,const Public&p,const std::string&session){
 encode(p);need(p.session==session && p.trust_id==trustIdentity(t) && p.spec==t.anchor.w.spec,"oneblock model verifier-owned trust/session/spec mismatch");
}
Fr Secret::value(std::size_t i)const{need(i<model.spec.padded,"oneblock model private coordinate bounds");return i<values.size()?values[i]:Fr(0);}
Secret rerandomize(const Trust&t,const std::string&session,std::vector<Fr>w,Random&rng){
 auto started=std::chrono::steady_clock::now();
 checkTrust(t);const auto&s=t.anchor.w.spec;need(w.size()==s.actual || w.size()==s.padded,"oneblock model complete private W shape");for(std::size_t i=0;i<w.size();++i){need(w[i].isValid(),"oneblock model invalid private W scalar");if(i>=s.actual)need(w[i].isZero(),"oneblock model nonzero padded W tail");}w.resize(s.actual);
 Secret out;out.model.session=session;out.model.trust_id=trustIdentity(t);out.model.spec=s;out.values=std::move(w);auto H=parameters(1).hiding;
 for(std::size_t i=0;i<s.rows;++i){out.blinds.push_back(rng.scalar());out.model.rows.push_back(t.anchor.w.rows[i]+H*out.blinds.back());}validate(t,out.model,session);std::cerr<<"MODEL_TIMING {\"phase\":\"W-hiding-rerandomization\",\"seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<"}\n";return out;
}
Secret prepare(const Trust&t,const std::string&session,Random&rng,const std::string&manifest_path){
 checkTrust(t);need(t.anchor_hash==anchorPin && t.manifest_hash==manifestPin && !t.plan.sources.empty() && t.plan.sources.back().spec==t.anchor.w.spec,"oneblock model production operator trust required");auto bytes=r1::readFile(manifest_path);need(r5b::digestHex(bytes)==t.manifest_hash,"oneblock model preprocessing manifest changed");auto manifest=r6::parseManifest(bytes,1);return rerandomize(t,session,r6::preprocessWeights(t.plan,t.regions,manifest,"data/gpt2_int"),rng);
}
Public registerModel(IO&io,const Trust&t,const Secret*secret,Random*rng){
 checkTrust(t);Writer input;if(io.prover()){need(secret && rng,"oneblock model missing private registration input");validate(t,secret->model,io.session());need(secret->blinds.size()==secret->model.spec.rows,"oneblock model private row blinds");input.raw(encode(secret->model));}
 auto public_bytes=io.message("oneblock/model/public",input.b);auto p=decode(public_bytes);validate(t,p,io.session());auto root=hex(r1::hash(public_bytes));auto H=parameters(1).hiding;std::set<Bytes>seen;
 for(std::size_t start=0;start<p.spec.rows;start+=chunkRows){
  const auto count=std::min(chunkRows,p.spec.rows-start);auto label="oneblock/model/register/"+std::to_string(start/chunkRows);Writer header;header.text(suite);header.text(root);header.text(io.session());header.text(trustIdentity(t));header.number(start);header.number(count);auto parent=io.prefix();header.raw(Bytes(parent.begin(),parent.end()));for(std::size_t i=0;i<count;++i){header.point(t.anchor.w.rows[start+i]);header.point(p.rows[start+i]);}bindPublic(io,label+"/header",header.b);
  Writer masks;std::vector<Fr>d;if(io.prover())for(std::size_t i=0;i<count;++i){d.push_back(rng->scalar(true));masks.point(H*d.back());}
  auto b=io.message(label+"/masks",masks.b);Reader mr{b};std::vector<G1>D;for(std::size_t i=0;i<count;++i){auto point=mr.point();need(!point.isZero(),"oneblock model zero rerandomization mask");need(seen.insert(r1::encode(point)).second,"oneblock model reused rerandomization mask");D.push_back(point);}mr.end();auto c=io.coin(label+"/challenge");need(!c.isZero(),"oneblock model zero verifier challenge");Writer responses;if(io.prover())for(std::size_t i=0;i<count;++i)responses.scalar(d[i]+c*secret->blinds[start+i]);
  b=io.message(label+"/responses",responses.b);Reader rr{b};for(std::size_t i=0;i<count;++i){auto z=rr.scalar();if(!io.prover())need(H*z==D[i]+(p.rows[start+i]-t.anchor.w.rows[start+i])*c,"oneblock model rerandomization row response equation");}rr.end();
 }
 Writer end;end.text(suite);end.text(root);end.number(p.spec.rows);bindPublic(io,"oneblock/model/registered",end.b);return p;
}
void opening(IO&io,const Trust&t,const Public&p,const Query&q,const Secret*secret,const Hidden*evaluation,Random*rng,const std::string&id){
 validate(t,p,io.session());need(!id.empty() && id.size()<=160,"oneblock model opening id");crypto::validate(q.evaluation,io.session(),q.evaluation.role);need(q.point.size()==r1::log2exact(p.spec.padded),"oneblock model opening point dimension");Writer statement;statement.text(suite);statement.text(id);statement.raw(encode(p));statement.raw(crypto::encode(q.evaluation));statement.number(q.point.size());for(auto&f:q.point)statement.scalar(f);bindPublic(io,"oneblock/model/open/"+id,statement.b);
 const auto colbits=r1::log2exact(p.spec.columns);auto a=r1::weights({q.point.begin(),q.point.begin()+colbits});auto L=r1::weights({q.point.begin()+colbits,q.point.end()});auto xi=modelMSM(p.rows,L);hyrax_a3::DotWitness w;
 if(io.prover()){
  need(secret && evaluation && rng && encode(secret->model)==encode(p) && crypto::encode(evaluation->handle)==crypto::encode(q.evaluation) && secret->values.size()==p.spec.actual && secret->blinds.size()==p.spec.rows,"oneblock model opening private identity/shape");w.values.assign(p.spec.columns,Fr(0));w.evaluation=*evaluation;
  for(std::size_t row=0;row<p.spec.rows;++row){w.blind+=L[row]*secret->blinds[row];for(std::size_t col=0;col<p.spec.columns;++col)w.values[col]+=L[row]*secret->value(row*p.spec.columns+col);}
 }
 hyrax_a3::dotProductTrustedR6Basis(io,t.anchor.w.spec,xi,q.evaluation,a,io.prover()?&w:nullptr,rng);
}
}
