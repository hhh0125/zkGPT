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

Fr fold(std::vector<Fr>&v,const std::vector<Fr>&q){std::size_t n=v.size();need(r1::log2exact(n)==q.size(),"classified public kernel query");for(auto&x:q){for(std::size_t j=0;j<n/2;++j)v[j]=v[2*j]+x*(v[2*j+1]-v[2*j]);n/=2;}return v[0];}
}
// All cached state is owned by one stack-scoped authentication session.
// Neither a caller callback nor Layout storage is retained in a frozen map.
Commitments::Weights::Weights(const std::vector<Fr>&q,AuthMetrics*m):split_(q.size()/2){auto t=processCPU();a_=r1::weights({q.begin(),q.begin()+split_});b_=r1::weights({q.begin()+split_,q.end()});if(m){++m->weights;m->weight_terms+=a_.size()+b_.size();m->weights_cpu+=processCPU()-t;}}
std::size_t Commitments::Weights::bytes()const{return (a_.capacity()+b_.capacity())*sizeof(Fr);}
Fr Commitments::Weights::at(std::size_t i)const{return a_[i&(a_.size()-1)]*b_[i>>split_];}

Fr Commitments::prepareClaim(const Claim&c,Authentication&auth,std::size_t i)const{
 auto started=processCPU();++auth_metrics_.map_calls;auth_metrics_.map_coordinates+=c.padded;
 auto&p=auth.claims[i];auto maxRuns=(auth.map_budget-auth.map_bytes)/sizeof(Authentication::Run);bool fits=maxRuns>0;
 auto weights=std::make_unique<Weights>(c.point,&auth_metrics_);Fr bias=0;MapHash digest;
 for(std::size_t j=0;j<c.padded;++j){auto l=c.map(j);
  need(l.constant || (l.coordinate.root<roots_.size() && l.coordinate.index<roots_[l.coordinate.root].spec.actual),"classified map root/coordinate bounds");
  if(c.only_root>=0)need(!l.constant && l.bias.isZero() && l.coordinate.root==std::size_t(c.only_root),"classified invalid root-only map hint");
  digest.append(l);if(c.only_root<0 && !l.bias.isZero()){bias+=weights->at(j)*l.bias;++auth_metrics_.weight_products;}
  if(!fits || l.constant)continue;
  if(!p.runs.empty()){auto&last=p.runs.back();if(last.begin+last.count==j && last.first.root==l.coordinate.root && std::uint64_t(last.first.index)+last.count==l.coordinate.index){++last.count;continue;}}
  if(p.runs.size()==maxRuns){fits=false;std::vector<Authentication::Run>().swap(p.runs);continue;}
  if(p.runs.size()==p.runs.capacity())p.runs.reserve(std::min(maxRuns,std::max<std::size_t>(8,p.runs.capacity()*2)));
  p.runs.push_back({j,1,l.coordinate});
 }
 need(digest.finish()==c.map_digest,"classified source map changed after defer");
 p.frozen=fits;auth.map_bytes+=p.runs.capacity()*sizeof(Authentication::Run);
 if(weights->bytes()<=auth.weight_budget-auth.weight_bytes){auth.weight_bytes+=weights->bytes();p.weights=std::move(weights);}
 auth_metrics_.map_cpu+=processCPU()-started;return bias;
}
std::vector<Fr>Commitments::kernel(std::size_t k,const std::vector<Fr>&lambda,const Authentication&auth)const{
 auto started=processCPU();++auth_metrics_.kernel_calls;auto n=roots_.at(k).spec.padded;auth_metrics_.kernel_peak_bytes=std::max<std::uint64_t>(auth_metrics_.kernel_peak_bytes,n*sizeof(Fr));std::vector<Fr>result(n,Fr(0));std::uint64_t visited=0;
 for(std::size_t i=0;i<claims_.size();++i){auto&c=claims_[i];if(c.only_root>=0 && std::size_t(c.only_root)!=k)continue;auto&p=auth.claims[i];std::unique_ptr<Weights>scratch;if(!p.weights)scratch=std::make_unique<Weights>(c.point,&auth_metrics_);auto&w=p.weights?*p.weights:*scratch;
  if(p.frozen){for(auto&run:p.runs)if(run.first.root==k)for(std::size_t j=0;j<run.count;++j){result[run.first.index+j]+=lambda[i]*w.at(run.begin+j);++visited;}}
  else{MapHash digest;for(std::size_t j=0;j<c.padded;++j){auto l=c.map(j);digest.append(l);++visited;if(!l.constant && l.coordinate.root==k){need(l.coordinate.index<roots_[k].spec.actual,"classified mapped coordinate outside actual root");result[l.coordinate.index]+=lambda[i]*w.at(j);}}need(digest.finish()==c.map_digest,"classified source map changed after defer");}
 }
 auth_metrics_.kernel_coordinates+=visited;auth_metrics_.weight_products+=visited;auth_metrics_.kernel_cpu+=processCPU()-started;
 if(opt2MetricsEnabled())std::cerr<<"OPT2_KERNEL {\"root\":"<<k<<",\"coordinates\":"<<visited<<",\"cpu_seconds\":"<<processCPU()-started<<"}\n";
 return result;
}
std::vector<Fr>Commitments::partialDot(const std::vector<Fr>&lambda,const Authentication&auth)const{
 auto started=processCPU();std::vector<Fr>values(roots_.size(),Fr(0));std::uint64_t visited=0;
 for(std::size_t i=0;i<claims_.size();++i){auto&c=claims_[i];auto&p=auth.claims[i];std::unique_ptr<Weights>scratch;if(!p.weights)scratch=std::make_unique<Weights>(c.point,&auth_metrics_);auto&w=p.weights?*p.weights:*scratch;
  if(p.frozen){for(auto&run:p.runs)for(std::size_t j=0;j<run.count;++j){values[run.first.root]+=lambda[i]*w.at(run.begin+j)*roots_[run.first.root].value(run.first.index+j);++visited;}}
  else{MapHash digest;for(std::size_t j=0;j<c.padded;++j){auto l=c.map(j);digest.append(l);++visited;if(!l.constant){need(l.coordinate.root<roots_.size() && l.coordinate.index<roots_[l.coordinate.root].spec.actual,"classified partial map bounds");values[l.coordinate.root]+=lambda[i]*w.at(j)*roots_[l.coordinate.root].value(l.coordinate.index);}}need(digest.finish()==c.map_digest,"classified source map changed after defer");}
 }
 auth_metrics_.partial_cpu+=processCPU()-started;auth_metrics_.weight_products+=visited;
 if(opt2MetricsEnabled())std::cerr<<"OPT2_PARTIAL {\"coordinates\":"<<visited<<",\"cpu_seconds\":"<<processCPU()-started<<",\"map_cache_bytes\":"<<auth.map_bytes<<",\"weight_cache_bytes\":"<<auth.weight_bytes<<"}\n";
 return values;
}
Commitments::Commitments(IO&io,const Layout&layout,Value base,Prepared*prepared,Random*rng,const model::Trust*trust,model::Secret*modelSecret,const std::vector<model::Trust>*chunks,std::vector<model::Secret>*chunkSecrets):io_(io),layout_(layout),identity_(identity(layout)),session_(io.session()),rng_(rng){
 auto modelCount=std::count_if(layout.roots.begin(),layout.roots.end(),[](auto&r){return r.model;});need(layout.master_actual?(chunks && chunks->size()==std::size_t(modelCount) && !trust && !modelSecret && (io.prover()?(chunkSecrets && chunkSecrets->size()==chunks->size()):!chunkSecrets)):(!chunks && !chunkSecrets),"classified Full12 independently loaded chunk trust boundary");
 need(io.prover()?bool(base)&&prepared&&rng:!base&&!prepared&&!rng&&!modelSecret,"classified P/V boundary");Writer binding;binding.text(suite);binding.text(identity_);binding.text(session_);bindPublic(io,"Classified/layout",binding.b);roots_.resize(layout.roots.size());std::set<std::string>usedPrepared;
 for(std::size_t k=0;k<roots_.size();++k){auto rootStarted=Clock::now();auto&root=roots_[k];root.spec=layout.roots[k];if(root.spec.model)continue;std::shared_ptr<std::vector<Fr>>packed;
  if(io.prover()){
   packed=std::make_shared<std::vector<Fr>>(root.spec.padded,Fr(0));
   if(root.spec.registered){if(layout.master_actual){for(auto&run:layout.base_runs)if(run.start.root==k)for(std::size_t j=0;j<run.count;++j)packed->at(run.start.index+j)=base(run.begin+j);}else for(std::size_t i=0;i<layout.base.size();++i)if(layout.base[i].root==k)packed->at(layout.base[i].index)=base(i);}
   else if(layout.master_actual){for(auto&[id,view]:layout.views)if(!view.mapped)for(auto&piece:view.pieces)if(piece.start.root==k){auto it=prepared->find(id);need(it!=prepared->end() && it->second->size()==view.padded && piece.begin<=view.padded && piece.count<=view.padded-piece.begin && piece.start.index<=packed->size() && piece.count<=packed->size()-piece.start.index,"classified prepared partition shape/bounds");std::copy_n(it->second->begin()+piece.begin,piece.count,packed->begin()+piece.start.index);usedPrepared.insert(id);}}
   else for(auto&[id,view]:layout.views)if(!view.mapped && view.start.root==k){auto it=prepared->find(id);need(it!=prepared->end() && it->second->size()==view.padded,"classified prepared object/shape");std::copy(it->second->begin(),it->second->end(),packed->begin()+view.start.index);prepared->erase(it);}
   root.secret=commitVectorOwned(root.spec.id,root.spec.actual,root.spec.padded,std::move(packed),*rng);root.pc=root.secret->pc;
  }
  auto bytes=io.message("Classified/root/"+std::to_string(k)+"/PC",io.prover()?encodeVector(root.pc):Bytes{});root.pc=decodeVector(bytes);need(root.pc.id==root.spec.id && root.pc.actual==root.spec.actual && root.pc.padded==root.spec.padded && root.pc.layout==r1::Layout::RowMajor,"classified wrong commitment identity/shape");root.basis=hyrax_a3::generators(root.pc.columns);
  if(io.prover()){auto values=root.secret->values;root.value=[values](std::size_t i){return values->at(i);};root.blinds=root.secret->blinds;}
  rootTiming("pack-and-commit",root.spec.id,rootStarted);
 }
 if(io.prover() && layout.master_actual){need(usedPrepared.size()==prepared->size(),"classified unused/missing partitioned helper");prepared->clear();}
 std::size_t chunkIndex=0;
 for(std::size_t k=0;k<roots_.size();++k)if(roots_[k].spec.model){
  const model::Trust*activeTrust=layout.master_actual?&chunks->at(chunkIndex):trust;model::Secret*activeSecret=layout.master_actual?(io.prover()?&chunkSecrets->at(chunkIndex):nullptr):modelSecret;need(activeTrust,"classified missing model authority");
  std::string scope=layout.master_actual?"Full12/model/chunk/"+std::to_string(chunkIndex):"";std::optional<stage2::ScopedIO>scoped;if(!scope.empty())scoped.emplace(io,scope);IO&activeIO=scoped?static_cast<IO&>(*scoped):io;
  auto pub=model::registerModel(activeIO,*activeTrust,activeSecret,rng);auto&root=roots_[k];need(root.spec.actual==pub.spec.actual && root.spec.padded==pub.spec.padded,"classified W trusted dimensions");root.pc={root.spec.id,pub.spec.actual,pub.spec.padded,pub.spec.rows,pub.spec.columns,pub.rows,r1::Layout::RowMajor};root.basis=r1::generators(activeTrust->anchor.w.spec).g;
  if(io.prover()){root.value=[activeSecret](std::size_t i){return activeSecret->value(i);};root.blinds=activeSecret->blinds;}
  root.open=[activeTrust,pub,activeSecret,scope](IO&live,const Query&q,const Hidden*h,Random*r,const std::string&id){if(scope.empty())model::opening(live,*activeTrust,pub,q,activeSecret,h,r,id);else{stage2::ScopedIO scoped(live,scope);model::opening(scoped,*activeTrust,pub,q,activeSecret,h,r,id);}};
  ++chunkIndex;
 }
 if(io.prover())need(prepared->empty(),"classified unused prepared object");
 sealed_=true;
}
void Commitments::constrainBaseTails(){
 need(sealed_ && !tails_constrained_ && !authenticated_ && claims_.empty() && io_.session()==session_ && identity(layout_)==identity_,"classified tail constraint lifecycle");for(auto&r:roots_)need(r.pc.layout==r1::Layout::RowMajor,"classified root must be RowMajor");std::set<std::string>seen;
 for(std::size_t k=0;k<roots_.size();++k){auto&r=roots_[k];if(!r.spec.registered || r.spec.model)continue;auto chunk=std::min<std::size_t>(64,65536/r.pc.columns);
  // Full valid rows need no registration. Model tails are fixed by the trusted
  // R6 anchor and registerModel() rerandomization binding in the constructor.
  for(std::size_t start=r.spec.actual/r.pc.columns;start<r.pc.rows;start+=chunk){auto count=std::min(chunk,r.pc.rows-start);auto name="Classified/zero-tail/"+std::to_string(k)+"/"+std::to_string(start);Writer header;header.text(suite);header.text(identity_);header.text(session_);header.text(r.spec.id);header.number(start);header.number(count);header.number(r.pc.columns);for(std::size_t i=0;i<count;++i)header.point(r.pc.points[start+i]);bindPublic(io_,name+"/header",header.b);std::vector<std::vector<Fr>>masks;std::vector<Fr>blinds;Writer first;
   auto active=[&](std::size_t row){const auto begin=row*r.pc.columns;return begin>=r.spec.actual?std::size_t(0):std::min(r.pc.columns,r.spec.actual-begin);};
   if(io_.prover())for(std::size_t i=0;i<count;++i){std::vector<Fr>d(active(start+i));for(auto&x:d)x=rng_->scalar();auto rho=rng_->scalar();G1 mask;mask.clear();if(!d.empty())mask=master3::masterMSM(r.basis.data(),d.data(),d.size());first.point(mask+parameters(1).hiding*rho);masks.push_back(std::move(d));blinds.push_back(rho);}
   auto b=io_.message(name+"/masks",first.b);Reader mr{b};std::vector<G1>D;for(std::size_t i=0;i<count;++i){auto d=mr.point();need(seen.insert(hex(r1::hash(r1::encode(d)))).second,"classified reused tail mask");D.push_back(d);}mr.end();auto c=io_.coin(name+"/challenge");need(!c.isZero(),"classified zero tail challenge");Writer response;
   if(io_.prover())for(std::size_t i=0;i<count;++i){for(std::size_t j=0;j<masks[i].size();++j)response.scalar(masks[i][j]+c*r.value((start+i)*r.pc.columns+j));response.scalar(blinds[i]+c*r.blinds[start+i]);}
   b=io_.message(name+"/responses",response.b);tail_response_bytes+=b.size();Reader rr{b};for(std::size_t i=0;i<count;++i){auto n=active(start+i);std::vector<Fr>z(n);for(auto&x:z)x=rr.scalar();auto rho=rr.scalar();if(!io_.prover()){G1 nonblind;nonblind.clear();if(n)nonblind=master3::masterMSM(r.basis.data(),z.data(),n);need(nonblind+parameters(1).hiding*rho==D[i]+r.pc.points[start+i]*c,"classified zero-tail equation failed");}++tail_rows;}rr.end();
  }
 }
 Writer end;end.text(identity_);end.number(tail_rows,8);end.number(tail_response_bytes,8);bindPublic(io_,"Classified/base-tails-constrained",end.b);tails_constrained_=true;
}
void Commitments::rootOpening(std::size_t k,const Query&q,const Hidden*h,const std::string&id){auto started=Clock::now();auto&r=roots_.at(k);if(r.spec.model)r.open(io_,q,h,rng_,id);else openVector(io_,r.pc,q,io_.prover()?&*r.secret:nullptr,h,rng_,id);rootTiming("opening",r.spec.id,started);}
VectorOracle Commitments::view(const std::string&id){
 need(sealed_ && !authenticated_,"classified view lifecycle");const auto&v=layout_.views.at(id);VectorOracle out;out.id=id;out.actual=v.actual;out.padded=v.padded;
 if(io_.prover())out.value=[this,&v](std::size_t j){auto l=locate(layout_,v,j);return l.constant?l.bias:roots_.at(l.coordinate.root).value(l.coordinate.index)+l.bias;};
 out.open=[this,&v](IO&live,const Query&q,const Hidden*h,Random*,std::string name){need(&live==&io_,"classified foreign IO session");Hidden value;if(live.prover()){need(h && encode(h->handle)==encode(q.evaluation),"classified opening hidden identity");value=*h;}else value.handle=q.evaluation;Claim c{name,v.id,value,q.point,v.padded,[this,&v](std::size_t j){return locate(layout_,v,j);}};if(!v.mapped && v.pieces.size()<=1)c.only_root=int(v.start.root);defer(std::move(c));};return out;
}
void Commitments::defer(Claim c){
 need(sealed_ && tails_constrained_ && !authenticated_ && io_.session()==session_,"classified unconstrained tails/closed session");need(c.map && c.padded && c.padded<=r1::kMaxVector && c.point.size()==r1::log2exact(c.padded) && names_.insert(c.id).second,"classified missing/duplicate/invalid claim");crypto::validate(c.value.handle,session_,c.value.handle.role);c.map_digest=mapDigest(c);Writer w;w.text(suite);w.text(identity_);w.text(c.id);w.text(c.map_id);w.raw(Bytes(c.map_digest.begin(),c.map_digest.end()));w.raw(encode(c.value.handle));w.number(c.padded,8);for(auto&x:c.point)w.scalar(x);bindPublic(io_,"Classified/defer/"+std::to_string(claims_.size()),w.b);claims_.push_back(std::move(c));
}
void Commitments::deferBase(const std::string&id,std::size_t actual,std::size_t padded,std::function<std::size_t(std::size_t)>index,const Hidden&h,const std::vector<Fr>&q){need(actual>0 && actual<=padded,"classified base claim shape");defer({id,"native-base/"+id,h,q,padded,[this,index=std::move(index),actual](std::size_t j)->Located{if(j>=actual)return {{},Fr(0),true};auto at=index(j);need(at<baseSize(layout_),"classified base map out of bounds");return {baseCoordinate(layout_,at),Fr(0),false};}});}
Digest Commitments::mapDigest(const Claim&c)const{auto started=processCPU();++auth_metrics_.map_calls;auth_metrics_.map_coordinates+=c.padded;MapHash h;for(std::size_t j=0;j<c.padded;++j){auto l=c.map(j);need(l.constant || (l.coordinate.root<roots_.size() && l.coordinate.index<roots_[l.coordinate.root].spec.actual),"classified map root/coordinate bounds");if(c.only_root>=0)need(!l.constant && l.bias.isZero() && l.coordinate.root==std::size_t(c.only_root),"classified invalid root-only map hint");h.append(l);}auto d=h.finish();auth_metrics_.map_cpu+=processCPU()-started;return d;}
std::vector<Fr>Commitments::kernel(std::size_t k,const std::vector<Fr>&lambda)const{
 auto started=processCPU();++auth_metrics_.kernel_calls;auth_metrics_.kernel_peak_bytes=std::max<std::uint64_t>(auth_metrics_.kernel_peak_bytes,roots_.at(k).spec.padded*sizeof(Fr));std::vector<Fr>result(roots_.at(k).spec.padded,Fr(0));for(std::size_t i=0;i<claims_.size();++i){auto&c=claims_[i];if(c.only_root>=0 && std::size_t(c.only_root)!=k)continue;auth_metrics_.kernel_coordinates+=c.padded;auth_metrics_.weight_products+=c.padded;Weights w(c.point,&auth_metrics_);MapHash digest;for(std::size_t j=0;j<c.padded;++j){auto l=c.map(j);digest.append(l);if(!l.constant && l.coordinate.root==k){need(l.coordinate.index<roots_[k].spec.actual,"classified mapped coordinate outside actual root");result[l.coordinate.index]+=lambda[i]*w.at(j);}}need(digest.finish()==c.map_digest,"classified source map changed after defer");}auth_metrics_.kernel_cpu+=processCPU()-started;if(opt2MetricsEnabled())std::cerr<<"OPT2_KERNEL {\"root\":"<<k<<",\"cpu_seconds\":"<<processCPU()-started<<"}\n";
 return result;
}
Commitments::~Commitments(){if(opt2MetricsEnabled()){auto&m=auth_metrics_;std::cerr<<"OPT2_AUTH {\"map_calls\":"<<m.map_calls<<",\"map_coordinates\":"<<m.map_coordinates<<",\"map_cpu_seconds\":"<<m.map_cpu<<",\"kernel_calls\":"<<m.kernel_calls<<",\"kernel_coordinates\":"<<m.kernel_coordinates<<",\"kernel_cpu_seconds\":"<<m.kernel_cpu<<",\"weights_precomputations\":"<<m.weights<<",\"weights_terms\":"<<m.weight_terms<<",\"weights_cpu_seconds\":"<<m.weights_cpu<<",\"weight_products\":"<<m.weight_products<<",\"partial_cpu_seconds\":"<<m.partial_cpu<<",\"kernel_peak_bytes\":"<<m.kernel_peak_bytes<<"}\n";}}
void Commitments::authenticate(){
 need(sealed_ && tails_constrained_ && !authenticated_ && !claims_.empty() && claims_.size()<=8192 && identity(layout_)==identity_,"classified missing/closed/mutated authentication barrier");Writer summary;summary.text(suite);summary.text(identity_);summary.number(claims_.size());for(auto&c:claims_)summary.text(c.id);bindPublic(io_,"Classified/all-claims-fixed",summary.b);
 Authentication auth(claims_.size(),layout_.master_actual!=0);std::vector<Fr>lambda;std::vector<Hidden>values;Fr bias=0;for(std::size_t i=0;i<claims_.size();++i){auto&c=claims_[i];auto local=prepareClaim(c,auth,i);lambda.push_back(io_.coin("Classified/lambda/"+std::to_string(i)));need(!lambda.back().isZero(),"classified zero aggregation challenge");values.push_back(c.value);bias+=lambda.back()*local;}
 if(opt2MetricsEnabled()){
  std::uint64_t legacyCoordinates=0,legacyWeights=0,frozen=0,onlyRoot=0,nonRoot=0;
  for(std::size_t i=0;i<claims_.size();++i){auto&c=claims_[i];auto scans=c.only_root>=0?1:roots_.size();legacyCoordinates+=scans*c.padded;legacyWeights+=scans;if(c.only_root>=0)++onlyRoot;else ++nonRoot;if(auth.claims[i].frozen)++frozen;}
  std::cerr<<"OPT2_SCAN_PLAN {\"claims\":"<<claims_.size()<<",\"root_only_claims\":"<<onlyRoot<<",\"multi_root_claims\":"<<nonRoot<<",\"frozen_claims\":"<<frozen<<",\"legacy_kernel_calls\":"<<roots_.size()*(io_.prover()?2:1)<<",\"legacy_kernel_coordinates\":"<<legacyCoordinates*(io_.prover()?2:1)<<",\"legacy_weights_precomputations\":"<<legacyWeights*(io_.prover()?2:1)+nonRoot<<",\"map_cache_bytes\":"<<auth.map_bytes<<",\"weight_cache_bytes\":"<<auth.weight_bytes<<"}\n";
 }
 auto initial=combine(session_,"Classified/aggregate",values,lambda);initial.value-=bias;initial.handle.point-=scalarCommit(bias,Fr(0));bindPublic(io_,"Classified/aggregate",encode(initial.handle));std::vector<Hidden>partial;
 auto dot=io_.prover()?partialDot(lambda,auth):std::vector<Fr>(roots_.size(),Fr(0));for(std::size_t k=0;k<roots_.size();++k){auto rootStarted=Clock::now();auto v=dot[k];partial.push_back(exchangeHidden(io_,"Classified/partial/"+std::to_string(k),v,rng_));rootTiming("partial-exchange",roots_[k].spec.id,rootStarted);}
 auto witnesses=partial;witnesses.push_back(initial);Relations sum;sum.role="Classified/partial-sum";for(auto&h:witnesses)sum.handles.push_back(h.handle);std::vector<Fr>coeff(witnesses.size(),Fr(1));coeff.back()=Fr(-1);sum.linear={coeff};sigma(io_,sum,io_.prover()?&witnesses:nullptr,rng_);
 for(std::size_t k=0;k<roots_.size();++k){auto rootStarted=Clock::now();auto K=kernel(k,lambda,auth);auto name="Classified/root-proof/"+std::to_string(k);std::vector<Value>f;if(io_.prover())f={roots_[k].value,[&K](std::size_t j){return K.at(j);}};auto end=expressionSumcheck(io_,roots_[k].spec.padded,partial[k],f,Expression{2,{{Fr(1),{0,1}}}},rng_,name+"/sumcheck");auto expected=fold(K,end.point);std::vector<Fr>().swap(K);equalHidden(io_,end.endpoints[1],expected,rng_,name+"/kernel");rootOpening(k,{end.point,end.endpoints[0].handle},io_.prover()?&end.endpoints[0]:nullptr,name+"/opening");rootTiming("authentication-and-opening",roots_[k].spec.id,rootStarted);}
 authenticated_=true;io_.coin("Classified/all-claims-authenticated/ack");
}
}
