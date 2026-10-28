#include "classified_commitments.hpp"
#include "stage2_opening.hpp"
#include <algorithm>
#include <openssl/evp.h>
#include <chrono>
#include <iostream>
#include <sys/resource.h>
namespace zkgpt::r7::oneblock::classified {
namespace {
using Clock=std::chrono::steady_clock;
void rootTiming(const char*phase,const std::string&id,Clock::time_point started){struct rusage usage{};getrusage(RUSAGE_SELF,&usage);std::cerr<<"OPT1_ROOT {\"phase\":\""<<phase<<"\",\"id\":\""<<id<<"\",\"seconds\":"<<std::chrono::duration<double>(Clock::now()-started).count()<<",\"cumulative_peak_rss_kib\":"<<usage.ru_maxrss<<"}\n";}
class MapHash {
 std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)>ctx_{EVP_MD_CTX_new(),EVP_MD_CTX_free};
 std::array<unsigned char,32768>buffer_{};std::size_t used_=0;
 void flush(){if(used_){need(EVP_DigestUpdate(ctx_.get(),buffer_.data(),used_)==1,"classified map digest update");used_=0;}}
 public:MapHash(){need(ctx_ && EVP_DigestInit_ex(ctx_.get(),EVP_sha256(),nullptr)==1,"classified map digest init");}
 void append(const Located&l){if(used_+41>buffer_.size())flush();buffer_[used_++]=l.constant?1:0;for(auto n:{l.coordinate.root,l.coordinate.index})for(unsigned j=0;j<4;++j)buffer_[used_++]=std::uint8_t(n>>(8*j));need(l.bias.isValid() && l.bias.serialize(buffer_.data()+used_,32)==32,"classified canonical map bias");used_+=32;}
 Digest finish(){flush();Digest d{};unsigned n=0;need(EVP_DigestFinal_ex(ctx_.get(),d.data(),&n)==1 && n==32,"classified map digest end");return d;}
};
class Weights {std::vector<Fr>a_,b_;unsigned split_;public:explicit Weights(const std::vector<Fr>&q):split_(q.size()/2){a_=r1::weights({q.begin(),q.begin()+split_});b_=r1::weights({q.begin()+split_,q.end()});}Fr at(std::size_t i)const{return a_[i&(a_.size()-1)]*b_[i>>split_];}};
Fr fold(std::vector<Fr>&v,const std::vector<Fr>&q){std::size_t n=v.size();need(r1::log2exact(n)==q.size(),"classified public kernel query");for(auto&x:q){for(std::size_t j=0;j<n/2;++j)v[j]=v[2*j]+x*(v[2*j+1]-v[2*j]);n/=2;}return v[0];}
}
Commitments::Commitments(IO&io,const Layout&layout,Value base,Prepared*prepared,Random*rng,const model::Trust*trust,model::Secret*modelSecret):io_(io),layout_(layout),identity_(identity(layout)),session_(io.session()),rng_(rng){
 need(io.prover()?bool(base)&&prepared&&rng:!base&&!prepared&&!rng&&!modelSecret,"classified P/V boundary");Writer binding;binding.text(suite);binding.text(identity_);binding.text(session_);bindPublic(io,"Classified/layout",binding.b);roots_.resize(layout.roots.size());
 for(std::size_t k=0;k<roots_.size();++k){auto rootStarted=Clock::now();auto&root=roots_[k];root.spec=layout.roots[k];if(root.spec.model)continue;std::shared_ptr<std::vector<Fr>>packed;
  if(io.prover()){
   packed=std::make_shared<std::vector<Fr>>(root.spec.padded,Fr(0));
   if(root.spec.registered){for(std::size_t i=0;i<layout.base.size();++i)if(layout.base[i].root==k)packed->at(layout.base[i].index)=base(i);}
   else for(auto&[id,view]:layout.views)if(!view.mapped && view.start.root==k){auto it=prepared->find(id);need(it!=prepared->end() && it->second->size()==view.padded,"classified prepared object/shape");std::copy(it->second->begin(),it->second->end(),packed->begin()+view.start.index);prepared->erase(it);}
   root.secret=commitVectorOwned(root.spec.id,root.spec.actual,root.spec.padded,std::move(packed),*rng);root.pc=root.secret->pc;
  }
  auto bytes=io.message("Classified/root/"+std::to_string(k)+"/PC",io.prover()?encodeVector(root.pc):Bytes{});root.pc=decodeVector(bytes);need(root.pc.id==root.spec.id && root.pc.actual==root.spec.actual && root.pc.padded==root.spec.padded,"classified wrong commitment identity/shape");root.basis=hyrax_a3::generators(root.pc.columns);
  if(io.prover()){auto values=root.secret->values;root.value=[values](std::size_t i){return values->at(i);};root.blinds=root.secret->blinds;}
  rootTiming("pack-and-commit",root.spec.id,rootStarted);
 }
 for(std::size_t k=0;k<roots_.size();++k)if(roots_[k].spec.model){need(trust,"classified missing model authority");auto pub=model::registerModel(io,*trust,modelSecret,rng);auto&root=roots_[k];need(root.spec.actual==pub.spec.actual && root.spec.padded==pub.spec.padded,"classified W trusted dimensions");root.pc={root.spec.id,pub.spec.actual,pub.spec.padded,pub.spec.rows,pub.spec.columns,pub.rows};root.basis=r1::generators(trust->anchor.w.spec).g;
  if(io.prover()){root.value=[modelSecret](std::size_t i){return modelSecret->value(i);};root.blinds=modelSecret->blinds;}
  root.open=[trust,pub,modelSecret](IO&live,const Query&q,const Hidden*h,Random*r,const std::string&id){model::opening(live,*trust,pub,q,modelSecret,h,r,id);};
 }
 if(io.prover())need(prepared->empty(),"classified unused prepared object");
 sealed_=true;
}
void Commitments::registerBase(){
 need(sealed_ && !base_registered_,"classified registration lifecycle");std::set<std::string>seen;
 for(std::size_t k=0;k<roots_.size();++k){auto&r=roots_[k];if(!r.spec.registered)continue;auto chunk=std::min<std::size_t>(64,65536/r.pc.columns);
  for(std::size_t start=0;start<r.pc.rows;start+=chunk){auto count=std::min(chunk,r.pc.rows-start);auto name="Classified/register/"+std::to_string(k)+"/"+std::to_string(start);Writer header;header.text(suite);header.text(identity_);header.text(session_);header.text(r.spec.id);header.number(start);header.number(count);header.number(r.pc.columns);for(std::size_t i=0;i<count;++i)header.point(r.pc.points[start+i]);bindPublic(io_,name+"/header",header.b);std::vector<std::vector<Fr>>masks;std::vector<Fr>blinds;Writer first;
   auto active=[&](std::size_t row){return row>=r.spec.actual?std::size_t(0):std::min(r.pc.columns,1+(r.spec.actual-1-row)/r.pc.rows);};
   if(io_.prover())for(std::size_t i=0;i<count;++i){std::vector<Fr>d(active(start+i));for(auto&x:d)x=rng_->scalar();auto rho=rng_->scalar();first.point(master3::masterMSM(r.basis.data(),d.data(),d.size())+parameters(1).hiding*rho);masks.push_back(std::move(d));blinds.push_back(rho);}
   auto b=io_.message(name+"/masks",first.b);Reader mr{b};std::vector<G1>D;for(std::size_t i=0;i<count;++i){auto d=mr.point();need(seen.insert(hex(r1::hash(r1::encode(d)))).second,"classified reused row mask");D.push_back(d);}mr.end();auto c=io_.coin(name+"/challenge");need(!c.isZero(),"classified zero registration challenge");Writer response;
   if(io_.prover())for(std::size_t i=0;i<count;++i){for(std::size_t j=0;j<masks[i].size();++j)response.scalar(masks[i][j]+c*r.value(start+i+r.pc.rows*j));response.scalar(blinds[i]+c*r.blinds[start+i]);}
   b=io_.message(name+"/responses",response.b);registration_bytes+=b.size();Reader rr{b};for(std::size_t i=0;i<count;++i){auto n=active(start+i);need(n>0,"classified empty row");std::vector<Fr>z(n);for(auto&x:z)x=rr.scalar();auto rho=rr.scalar();if(!io_.prover())need(master3::masterMSM(r.basis.data(),z.data(),n)+parameters(1).hiding*rho==D[i]+r.pc.points[start+i]*c,"classified row knowledge/zero-tail equation failed");++registered_rows;}rr.end();
  }
 }
 Writer end;end.text(identity_);end.number(registered_rows,8);end.number(registration_bytes,8);bindPublic(io_,"Classified/base-registered",end.b);base_registered_=true;
}
void Commitments::rootOpening(std::size_t k,const Query&q,const Hidden*h,const std::string&id){auto&r=roots_.at(k);if(r.spec.model)r.open(io_,q,h,rng_,id);else openVector(io_,r.pc,q,io_.prover()?&*r.secret:nullptr,h,rng_,id);}
VectorOracle Commitments::view(const std::string&id){
 need(sealed_ && !authenticated_,"classified view lifecycle");const auto&v=layout_.views.at(id);VectorOracle out;out.id=id;out.actual=v.actual;out.padded=v.padded;
 if(io_.prover())out.value=[this,&v](std::size_t j){auto l=locate(layout_,v,j);return l.constant?l.bias:roots_.at(l.coordinate.root).value(l.coordinate.index)+l.bias;};
 out.open=[this,&v](IO&live,const Query&q,const Hidden*h,Random*,std::string name){need(&live==&io_,"classified foreign IO session");Hidden value;if(live.prover()){need(h && encode(h->handle)==encode(q.evaluation),"classified opening hidden identity");value=*h;}else value.handle=q.evaluation;Claim c{name,v.id,value,q.point,v.padded,[this,&v](std::size_t j){return locate(layout_,v,j);}};if(!v.mapped)c.only_root=int(v.start.root);defer(std::move(c));};return out;
}
void Commitments::defer(Claim c){
 need(sealed_ && base_registered_ && !authenticated_ && io_.session()==session_,"classified unregistered/closed session");need(c.map && c.padded && c.padded<=(1u<<24) && c.point.size()==r1::log2exact(c.padded) && names_.insert(c.id).second,"classified missing/duplicate/invalid claim");crypto::validate(c.value.handle,session_,c.value.handle.role);c.map_digest=mapDigest(c);Writer w;w.text(suite);w.text(identity_);w.text(c.id);w.text(c.map_id);w.raw(Bytes(c.map_digest.begin(),c.map_digest.end()));w.raw(encode(c.value.handle));w.number(c.padded,8);for(auto&x:c.point)w.scalar(x);bindPublic(io_,"Classified/defer/"+std::to_string(claims_.size()),w.b);claims_.push_back(std::move(c));
}
void Commitments::deferBase(const std::string&id,std::size_t actual,std::size_t padded,std::function<std::size_t(std::size_t)>index,const Hidden&h,const std::vector<Fr>&q){need(actual>0 && actual<=padded,"classified base claim shape");defer({id,"native-base/"+id,h,q,padded,[this,index=std::move(index),actual](std::size_t j)->Located{if(j>=actual)return {{},Fr(0),true};auto at=index(j);need(at<layout_.base.size(),"classified base map out of bounds");return {layout_.base[at],Fr(0),false};}});}
Digest Commitments::mapDigest(const Claim&c)const{MapHash h;for(std::size_t j=0;j<c.padded;++j){auto l=c.map(j);need(l.constant || (l.coordinate.root<roots_.size() && l.coordinate.index<roots_[l.coordinate.root].spec.actual),"classified map root/coordinate bounds");if(c.only_root>=0)need(!l.constant && l.bias.isZero() && l.coordinate.root==std::size_t(c.only_root),"classified invalid root-only map hint");h.append(l);}return h.finish();}
std::vector<Fr>Commitments::kernel(std::size_t k,const std::vector<Fr>&lambda)const{
 std::vector<Fr>result(roots_.at(k).spec.padded,Fr(0));for(std::size_t i=0;i<claims_.size();++i){auto&c=claims_[i];if(c.only_root>=0 && std::size_t(c.only_root)!=k)continue;Weights w(c.point);MapHash digest;for(std::size_t j=0;j<c.padded;++j){auto l=c.map(j);digest.append(l);if(!l.constant && l.coordinate.root==k){need(l.coordinate.index<roots_[k].spec.actual,"classified mapped coordinate outside actual root");result[l.coordinate.index]+=lambda[i]*w.at(j);}}need(digest.finish()==c.map_digest,"classified source map changed after defer");}return result;
}
void Commitments::authenticate(){
 need(sealed_ && base_registered_ && !authenticated_ && !claims_.empty() && claims_.size()<=8192 && identity(layout_)==identity_,"classified missing/closed/mutated authentication barrier");Writer summary;summary.text(suite);summary.text(identity_);summary.number(claims_.size());for(auto&c:claims_)summary.text(c.id);bindPublic(io_,"Classified/all-claims-fixed",summary.b);
 std::vector<Fr>lambda;std::vector<Hidden>values;Fr bias=0;for(std::size_t i=0;i<claims_.size();++i){auto&c=claims_[i];need(mapDigest(c)==c.map_digest,"classified source map changed after defer");lambda.push_back(io_.coin("Classified/lambda/"+std::to_string(i)));need(!lambda.back().isZero(),"classified zero aggregation challenge");values.push_back(c.value);if(c.only_root<0){Weights w(c.point);Fr local=0;MapHash digest;for(std::size_t j=0;j<c.padded;++j){auto l=c.map(j);digest.append(l);local+=w.at(j)*l.bias;}need(digest.finish()==c.map_digest,"classified bias map changed after defer");bias+=lambda.back()*local;}}
 auto initial=combine(session_,"Classified/aggregate",values,lambda);initial.value-=bias;initial.handle.point-=scalarCommit(bias,Fr(0));bindPublic(io_,"Classified/aggregate",encode(initial.handle));std::vector<Hidden>partial;
 for(std::size_t k=0;k<roots_.size();++k){auto rootStarted=Clock::now();Fr v=0;if(io_.prover()){auto K=kernel(k,lambda);for(std::size_t j=0;j<roots_[k].spec.actual;++j)v+=roots_[k].value(j)*K[j];}partial.push_back(exchangeHidden(io_,"Classified/partial/"+std::to_string(k),v,rng_));rootTiming("partial-kernel-dot",roots_[k].spec.id,rootStarted);}
 auto witnesses=partial;witnesses.push_back(initial);Relations sum;sum.role="Classified/partial-sum";for(auto&h:witnesses)sum.handles.push_back(h.handle);std::vector<Fr>coeff(witnesses.size(),Fr(1));coeff.back()=Fr(-1);sum.linear={coeff};sigma(io_,sum,io_.prover()?&witnesses:nullptr,rng_);
 for(std::size_t k=0;k<roots_.size();++k){auto rootStarted=Clock::now();auto K=kernel(k,lambda);auto name="Classified/root-proof/"+std::to_string(k);std::vector<Value>f;if(io_.prover())f={roots_[k].value,[&K](std::size_t j){return K.at(j);}};auto end=expressionSumcheck(io_,roots_[k].spec.padded,partial[k],f,Expression{2,{{Fr(1),{0,1}}}},rng_,name+"/sumcheck");auto expected=fold(K,end.point);std::vector<Fr>().swap(K);equalHidden(io_,end.endpoints[1],expected,rng_,name+"/kernel");rootOpening(k,{end.point,end.endpoints[0].handle},io_.prover()?&end.endpoints[0]:nullptr,name+"/opening");rootTiming("authentication-and-opening",roots_[k].spec.id,rootStarted);}
 authenticated_=true;io_.coin("Classified/all-claims-authenticated/ack");
}
}
