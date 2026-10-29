from pathlib import Path
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd().resolve()==R
P=R/'src/paper_r7'
def edit(n,a,b):
 p=P/n;s=p.read_text();assert a in s,(n,a[:100]);p.write_text(s.replace(a,b))
edit('classified_commitments.hpp',' mutable AuthMetrics auth_metrics_;',' mutable AuthMetrics auth_metrics_;\n struct Authentication;\n Fr prepareClaim(const Claim&,Authentication&,std::size_t)const;\n std::vector<Fr>kernel(std::size_t,const std::vector<Fr>&,const Authentication&)const;\n std::vector<Fr>partialDot(const std::vector<Fr>&,const Authentication&)const;')
edit('classified_commitments.cpp','Fr at(std::size_t i)const{return a_[i&(a_.size()-1)]*b_[i>>split_];}};','std::size_t bytes()const{return (a_.capacity()+b_.capacity())*sizeof(Fr);}Fr at(std::size_t i)const{return a_[i&(a_.size()-1)]*b_[i>>split_];}};')
needle='Commitments::Commitments(IO&io'
extra=r'''// All cached state is owned by one stack-scoped authentication session.
// Neither a caller callback nor Layout storage is retained in a frozen map.
struct Commitments::Authentication {
 struct Run {std::size_t begin,count;Coordinate first;};
 struct Prepared {std::vector<Run>runs;bool frozen=false;std::unique_ptr<Weights>weights;};
 static constexpr std::size_t map_limit=16u<<20,weight_limit=32u<<20;
 std::vector<Prepared>claims;std::size_t map_bytes=0,weight_bytes=0;
 explicit Authentication(std::size_t n):claims(n){}
};
Fr Commitments::prepareClaim(const Claim&c,Authentication&auth,std::size_t i)const{
 auto started=processCPU();++auth_metrics_.map_calls;auth_metrics_.map_coordinates+=c.padded;
 auto&p=auth.claims[i];auto maxRuns=(Authentication::map_limit-auth.map_bytes)/sizeof(Authentication::Run);bool fits=maxRuns>0;
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
 if(weights->bytes()<=Authentication::weight_limit-auth.weight_bytes){auth.weight_bytes+=weights->bytes();p.weights=std::move(weights);}
 auth_metrics_.map_cpu+=processCPU()-started;return bias;
}
std::vector<Fr>Commitments::kernel(std::size_t k,const std::vector<Fr>&lambda,const Authentication&auth)const{
 auto started=processCPU();++auth_metrics_.kernel_calls;auto n=roots_.at(k).spec.padded;auth_metrics_.kernel_peak_bytes=std::max<std::uint64_t>(auth_metrics_.kernel_peak_bytes,n*sizeof(Fr));std::vector<Fr>result(n,Fr(0));std::uint64_t visited=0;
 for(std::size_t i=0;i<claims_.size();++i){auto&c=claims_[i];if(c.only_root>=0 && std::size_t(c.only_root)!=k)continue;auto&p=auth.claims[i];std::unique_ptr<Weights>scratch;if(!p.weights)scratch=std::make_unique<Weights>(c.point,&auth_metrics_);auto&w=p.weights?*p.weights:*scratch;
  if(p.frozen){for(auto&run:p.runs)if(run.first.root==k)for(std::size_t j=0;j<run.count;++j){result[run.first.index+j]+=lambda[i]*w.at(run.begin+j);++visited;}}
  else{MapHash digest;for(std::size_t j=0;j<c.padded;++j){auto l=c.map(j);digest.append(l);++visited;if(!l.constant && l.coordinate.root==k){need(l.coordinate.index<roots_[k].spec.actual,"classified mapped coordinate outside actual root");result[l.coordinate.index]+=lambda[i]*w.at(j);}}need(digest.finish()==c.map_digest,"classified source map changed after defer");}
 }
 auth_metrics_.kernel_coordinates+=visited;auth_metrics_.weight_products+=visited;auth_metrics_.kernel_cpu+=processCPU()-started;
 if(opt2MetricsEnabled())std::cerr<<"OPT2_KERNEL {\"root\":"<<k<<",\"coordinates\":"<<visited<<",\"cpu_seconds\":"<<processCPU()-started<<"}\n";return result;
}
std::vector<Fr>Commitments::partialDot(const std::vector<Fr>&lambda,const Authentication&auth)const{
 auto started=processCPU();std::vector<Fr>values(roots_.size(),Fr(0));std::uint64_t visited=0;
 for(std::size_t i=0;i<claims_.size();++i){auto&c=claims_[i];auto&p=auth.claims[i];std::unique_ptr<Weights>scratch;if(!p.weights)scratch=std::make_unique<Weights>(c.point,&auth_metrics_);auto&w=p.weights?*p.weights:*scratch;
  if(p.frozen){for(auto&run:p.runs)for(std::size_t j=0;j<run.count;++j){values[run.first.root]+=lambda[i]*w.at(run.begin+j)*roots_[run.first.root].value(run.first.index+j);++visited;}}
  else{MapHash digest;for(std::size_t j=0;j<c.padded;++j){auto l=c.map(j);digest.append(l);++visited;if(!l.constant){need(l.coordinate.root<roots_.size() && l.coordinate.index<roots_[l.coordinate.root].spec.actual,"classified partial map bounds");values[l.coordinate.root]+=lambda[i]*w.at(j)*roots_[l.coordinate.root].value(l.coordinate.index);}}need(digest.finish()==c.map_digest,"classified source map changed after defer");}
 }
 auth_metrics_.partial_cpu+=processCPU()-started;auth_metrics_.weight_products+=visited;
 if(opt2MetricsEnabled())std::cerr<<"OPT2_PARTIAL {\"coordinates\":"<<visited<<",\"cpu_seconds\":"<<processCPU()-started<<",\"map_cache_bytes\":"<<auth.map_bytes<<",\"weight_cache_bytes\":"<<auth.weight_bytes<<"}\n";return values;
}
'''
edit('classified_commitments.cpp',needle,extra+needle)
old='std::vector<Fr>lambda;std::vector<Hidden>values;Fr bias=0;for(std::size_t i=0;i<claims_.size();++i){auto&c=claims_[i];need(mapDigest(c)==c.map_digest,"classified source map changed after defer");lambda.push_back(io_.coin("Classified/lambda/"+std::to_string(i)));need(!lambda.back().isZero(),"classified zero aggregation challenge");values.push_back(c.value);if(c.only_root<0){Weights w(c.point,&auth_metrics_);Fr local=0;MapHash digest;for(std::size_t j=0;j<c.padded;++j){auto l=c.map(j);digest.append(l);local+=w.at(j)*l.bias;}need(digest.finish()==c.map_digest,"classified bias map changed after defer");bias+=lambda.back()*local;}}'
new='Authentication auth(claims_.size());std::vector<Fr>lambda;std::vector<Hidden>values;Fr bias=0;for(std::size_t i=0;i<claims_.size();++i){auto&c=claims_[i];auto local=prepareClaim(c,auth,i);lambda.push_back(io_.coin("Classified/lambda/"+std::to_string(i)));need(!lambda.back().isZero(),"classified zero aggregation challenge");values.push_back(c.value);bias+=lambda.back()*local;}'
edit('classified_commitments.cpp',old,new)
old='for(std::size_t k=0;k<roots_.size();++k){auto rootStarted=Clock::now();auto partialStarted=processCPU();Fr v=0;if(io_.prover()){auto K=kernel(k,lambda);for(std::size_t j=0;j<roots_[k].spec.actual;++j)v+=roots_[k].value(j)*K[j];}auth_metrics_.partial_cpu+=processCPU()-partialStarted;partial.push_back(exchangeHidden'
new='auto dot=io_.prover()?partialDot(lambda,auth):std::vector<Fr>(roots_.size(),Fr(0));for(std::size_t k=0;k<roots_.size();++k){auto rootStarted=Clock::now();auto v=dot[k];partial.push_back(exchangeHidden'
edit('classified_commitments.cpp',old,new)
edit('classified_commitments.cpp','auto K=kernel(k,lambda);auto name=','auto K=kernel(k,lambda,auth);auto name=')
# Extend the pre-existing dense/reference harness before running the new path.
edit('opt2_kernel_tests.cpp','auto start=std::chrono::steady_clock::now();auto cpu=processCPU();Fr checksum=0;',
'''Commitments::Authentication prepared(c.claims_.size());Fr bias=0;for(unsigned i=0;i<c.claims_.size();++i)bias+=lambda[i]*c.prepareClaim(c.claims_[i],prepared,i);
  auto start=std::chrono::steady_clock::now();auto cpu=processCPU();Fr checksum=0;auto dots=c.partialDot(lambda,prepared);''')
edit('opt2_kernel_tests.cpp','auto got=c.kernel(k,lambda);','auto got=c.kernel(k,lambda,prepared);if(!pass){auto ref=c.kernel(k,lambda);need(got==ref,"optimized/reference kernel mismatch");Fr dot=0;for(std::size_t j=0;j<got.size();++j)dot+=got[j]*c.roots_[k].value(j);need(dot==dots[k],"partial-dot dense mismatch");}')
# Benchmark should exclude the independent reference work, which is separately tested.
edit('opt2_kernel_tests.cpp','if(!pass){auto ref=','if(!bench && !pass){auto ref=')
edit('opt2_kernel_tests.cpp','mode\\\":\\\"reference','mode\\\":\\\"optimized')
print('SESSION-BOUNDED KERNEL OPTIMIZATION WRITTEN')
