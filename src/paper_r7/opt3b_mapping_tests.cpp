#define main opt3a_fixture_main
#include "opt3a_kernel_tests.cpp"
#undef main
#include "opt3b_reference.hpp"
#include <sstream>
#include <sys/syscall.h>
// Test-only syscall fault injection. Normal benchmarks use real syscalls;
// production executables do not link these wrappers or the flags.
namespace ioFault {enum Mode{normal,interruptAndShort,partialFailure,endEarly};thread_local Mode writing=normal,reading=normal;thread_local unsigned writes=0,reads=0;}
extern "C" ssize_t pwrite(int fd,const void*data,size_t n,off_t at){
 auto mode=ioFault::writing;auto call=ioFault::writes++;if(mode==ioFault::interruptAndShort && call==0){errno=EINTR;return -1;}if(mode==ioFault::partialFailure && call>0){errno=EIO;return -1;}
 if(mode!=ioFault::normal){n=std::min(n,size_t(7));}return ::syscall(SYS_pwrite64,fd,data,n,at);
}
extern "C" ssize_t pread(int fd,void*data,size_t n,off_t at){
 auto mode=ioFault::reading;auto call=ioFault::reads++;if(mode==ioFault::interruptAndShort && call==0){errno=EINTR;return -1;}if(mode==ioFault::partialFailure && call>0){errno=EIO;return -1;}if(mode==ioFault::endEarly && call>0)return 0;
 if(mode!=ioFault::normal){n=std::min(n,size_t(7));}return ::syscall(SYS_pread64,fd,data,n,at);
}
namespace baseline=zkgpt::r7::oneblock::classified::opt3b_reference;
struct NativeMaps {
 Fixture f;layeredCircuit graph;std::uint64_t coordinates=0;
 explicit NativeMaps(const char*path){
  auto context=master3::trustedContext(12);f.layout=makeLayout(context);
  LLM nn(12,12,64,768,3072);{master3::QuietStdout quiet;zkgpt::r6::configure(nn);graph=nn.buildPublicCircuit();}
  auto&roots=Access::roots(*f.pcs);roots.clear();for(auto&s:f.layout.roots){Root r;r.spec=s;r.value=[](std::size_t j){return Fr(1+j%29);};roots.push_back(std::move(r));}
  std::ifstream in(path,std::ios::binary);need(bool(in),"OPT3B public input open");Bytes bytes((std::istreambuf_iterator<char>(in)),{});Reader all{bytes};auto pub=all.raw(all.number());Reader pr{pub};pr.text();need(pr.text()==identity(f.layout),"OPT3B public map layout identity");auto count=pr.number();auto indices=std::make_shared<std::vector<std::size_t>>();for(std::size_t i=0;i<count;++i){indices->push_back(pr.number(8));pr.scalar();}pr.end();
  auto&claims=Access::claims(*f.pcs);claims.clear();auto n=all.number();need(n==1502,"OPT3B native declaration count");
  for(std::size_t i=0;i<n;++i){auto payload=all.raw(all.number());Reader r{payload};need(r.text()==classified::suite && r.text()==identity(f.layout),"OPT3B old defer identity");Claim c;c.id=r.text();c.map_id=r.text();auto digest=r.raw(32);std::copy(digest.begin(),digest.end(),c.map_digest.begin());r.text();r.text();r.text();r.point();c.padded=r.number(8);for(unsigned j=0;j<r1::log2exact(c.padded);++j)c.point.push_back(r.scalar());r.end();
   if(c.map_id=="native-base/public/X-Y-constants")c.map=[this,indices](std::size_t j)->Located{++f.map_queries;if(j>=indices->size())return {{},Fr(0),true};++f.rowmajor_queries;return {baseCoordinate(f.layout,indices->at(j)),Fr(0),false};};
   else if(c.map_id.rfind("native-base/gkr/layer/",0)==0){std::stringstream ss(c.id);std::string word;std::vector<std::string>parts;while(std::getline(ss,word,'/'))parts.push_back(word);need(parts.size()==6,"OPT3B terminal name");auto layer=std::stoull(parts[2]);auto*ids=parts[4]=="v"?&graph.circuit.at(layer).ori_id_v:&graph.circuit.at(layer).ori_id_u;c.map=[this,ids](std::size_t j)->Located{++f.map_queries;if(j>=ids->size())return {{},Fr(0),true};++f.rowmajor_queries;return {baseCoordinate(f.layout,ids->at(j)),Fr(0),false};};}
   else{const auto*v=&f.layout.views.at(c.map_id);if(!v->mapped && v->pieces.size()<=1)c.only_root=int(v->start.root);c.map=[this,v](std::size_t j){++f.map_queries;auto l=locate(f.layout,*v,j);f.rowmajor_queries+=v->mapped && !l.constant;return l;};}
   coordinates+=c.padded;claims.push_back(std::move(c));
  }all.end();
 }
};
struct Census {
 struct Track {std::size_t j=0,count=0;std::int64_t logicalStride=0,coordinate=0,coordinateStride=0;};
 std::vector<Track>last;Track previous;std::size_t previousRoot=0;bool previousValid=false;
 std::uint64_t logicalRuns=0,rootRuns=0,constants=0,biases=0,negative=0,zero=0;std::vector<std::uint64_t>hits;
 explicit Census(std::size_t n):last(n),hits(n){}
 void observe(std::size_t j,const Located&l){
  biases+=!l.bias.isZero();if(l.constant){++constants;previousValid=false;return;}auto k=l.coordinate.root;auto at=std::int64_t(l.coordinate.index);++hits.at(k);
  if(!previousValid || previousRoot!=k || previous.j+previous.count!=j || (previous.count>1 && at-previous.coordinate!=std::int64_t(previous.count)*previous.coordinateStride)){++logicalRuns;previous={j,1,1,at,0};}
  else{if(previous.count==1){previous.coordinateStride=at-previous.coordinate;negative+=previous.coordinateStride<0;zero+=previous.coordinateStride==0;}++previous.count;}previousRoot=k;previousValid=true;
  auto&t=last[k];if(!t.count || (t.count>1 && (j-t.j!=t.count*std::size_t(t.logicalStride) || at-t.coordinate!=std::int64_t(t.count)*t.coordinateStride))){++rootRuns;t={j,1,0,at,0};}
  else{if(t.count==1){t.logicalStride=std::int64_t(j-t.j);t.coordinateStride=at-t.coordinate;}++t.count;}
 }
};
void profile(const char*path){
 NativeMaps native(path);auto&c=*native.f.pcs;auto&claims=Access::claims(c);auto&roots=Access::roots(c);baseline::Engine ref(roots,claims);baseline::Engine::Authentication auth(claims.size(),true);std::size_t frozen=0;std::uint64_t fallback=0,matched11=0;auto start=processCPU();
 for(std::size_t i=0;i<claims.size();++i){Census census(roots.size());ref.observe=[&](std::size_t j,const Located&l){census.observe(j,l);};auto remaining=auth.map_budget-auth.map_bytes;ref.prepareClaim(claims[i],auth,i);auto&p=auth.claims[i];frozen+=p.frozen;if(!p.frozen){fallback+=claims[i].padded;matched11+=census.hits[11];}
  auto rootCount=std::count_if(census.hits.begin(),census.hits.end(),[](auto n){return n>0;});
  std::cout<<"OPT3B_CENSUS {\"claim\":"<<i<<",\"id\":\""<<claims[i].id<<"\",\"map_id\":\""<<claims[i].map_id<<"\",\"padded\":"<<claims[i].padded<<",\"frozen\":"<<(p.frozen?"true":"false")<<",\"remaining_budget\":"<<remaining<<",\"run_capacity\":"<<p.runs.capacity()<<",\"logical_contiguous_runs\":"<<census.logicalRuns<<",\"root_centric_strided_runs\":"<<census.rootRuns<<",\"root_count\":"<<rootCount<<",\"constants\":"<<census.constants<<",\"bias_entries\":"<<census.biases<<",\"negative_runs\":"<<census.negative<<",\"zero_stride_runs\":"<<census.zero<<",\"reason\":\""<<(p.frozen?"FROZEN":remaining<sizeof(baseline::Engine::Authentication::Run)?"CACHE_CAPACITY_EXHAUSTED":"RUN_COUNT_EXCEEDS_REMAINING_CACHE")<<"\",\"root11_matches\":"<<census.hits[11]<<",\"digest_equal\":true}"<<std::endl;
 }
 need(frozen==1076 && fallback==583705824,"OPT3B native census differs from actual OPT3A Full12");std::cout<<"OPT3B_CENSUS_SUMMARY {\"claims\":"<<claims.size()<<",\"frozen\":"<<frozen<<",\"unfrozen\":"<<claims.size()-frozen<<",\"per_root_fallback_queries\":"<<fallback<<",\"unfrozen_root11_matches\":"<<matched11<<",\"cache_bytes\":"<<auth.map_bytes<<",\"cpu_seconds\":"<<processCPU()-start<<"}\n";
}
void rootCentricContract(){
 Fixture f;auto&roots=Access::roots(*f.pcs);roots.clear();for(unsigned k=0;k<45;++k){Root r;r.spec={"contract/"+std::to_string(k),512,512,k%2==0,k%2==0};r.value=[k](std::size_t j){return Fr(j+1+k);};roots.push_back(std::move(r));}
 auto&claims=Access::claims(*f.pcs);claims.clear();Claim c;c.id="interleaved/sparse-root11";c.padded=16384;for(unsigned k=0;k<14;++k)c.point.push_back(f.rng.scalar());
 c.map=[&f](std::size_t j)->Located{++f.map_queries;if(j>=16371)return {{},Fr(7),true};unsigned k=j%44;if(k>=11)++k;if(j==10 || j==15900)k=11;auto at=std::uint32_t(j/44);if(k==0)at=511-at;if(k==1)at=7;return {{k,at},Fr(-3),false};};claims.push_back(std::move(c));Access::digest(*f.pcs);
 Access::Auth a(1);a.map_bytes=a.map_budget-8192;auto bias=Access::prepare(*f.pcs,a,0);need(a.claims[0].frozen,"OPT3B interleaved mapping must freeze in 8192 bytes");need(a.map_bytes<=a.map_budget,"OPT3B root plan budget overflow");
 std::vector<Fr>lambda{Fr(5)};auto before=f.map_queries;auto dot=Access::dots(*f.pcs,lambda,a);auto got=Access::kernel(*f.pcs,11,lambda,a);need(f.map_queries==before,"OPT3B frozen root plan reused callback");
 baseline::Engine reference(roots,claims);baseline::Engine::Authentication b(1,false);need(reference.prepareClaim(claims[0],b,0)==bias,"OPT3B contract bias");need(got==reference.kernel(11,lambda,b) && dot==reference.partialDot(lambda,b),"OPT3B contract algebra");
 claims[0].map=[](std::size_t)->Located{throw std::runtime_error("frozen callback reused");};need(got==Access::kernel(*f.pcs,11,lambda,a),"OPT3B frozen sparse root result");std::cout<<"OPT3B_ROOT_CENTRIC_CONTRACT_PASS\n";
}
void checkPlanIdentity(const Claim&claim,const Access::Auth&auth,std::size_t i,std::size_t roots){
 auto&p=auth.claims[i];if(!p.frozen)return;std::vector<unsigned char>seen(claim.padded,0);
 for(std::size_t k=0;k<roots;++k)p.forRoot(k,auth.store,[&](const auto&r){need(r.count && r.logical_step && r.first.root==k,"OPT3B plan run identity");std::int64_t at=r.first.index;for(std::size_t n=0;n<r.count;++n,at+=r.stride){auto j=r.begin+n*r.logical_step;need(j<claim.padded && !seen[j]++,"OPT3B duplicated logical entry");auto expected=claim.map(j);need(!expected.constant && expected.coordinate.root==k && at==expected.coordinate.index,"OPT3B plan coordinate mismatch");}});
 for(std::size_t j=0;j<claim.padded;++j)need(seen[j]==!claim.map(j).constant,"OPT3B plan omitted mapping entry");
}
template<class F>void rejectsB(F&&run,const char*message){bool rejected=false;try{run();}catch(const std::exception&){rejected=true;}need(rejected,message);}
void equivalenceB(std::size_t n,unsigned pattern,bool tight){
 Fixture f;f.setup(n,6,pattern);auto&c=*f.pcs;auto&claims=Access::claims(c);auto&roots=Access::roots(c);baseline::Engine old(roots,claims);baseline::Engine::Authentication b(claims.size(),false);Access::Auth a(claims.size());if(tight)a.map_bytes=a.map_budget-8192;
 std::vector<Fr>lambda;Fr bias=0,expected=0;for(std::size_t i=0;i<claims.size();++i){lambda.push_back(i%2?Fr(-1):Fr(0));bias+=lambda.back()*Access::prepare(c,a,i);expected+=lambda.back()*old.prepareClaim(claims[i],b,i);checkPlanIdentity(claims[i],a,i,roots.size());}need(bias==expected,"OPT3B bias equivalence");
 auto dot=Access::dots(c,lambda,a);need(dot==old.partialDot(lambda,b),"OPT3B dot equivalence");for(std::size_t k=0;k<roots.size();++k)need(Access::kernel(c,k,lambda,a)==old.kernel(k,lambda,b),"OPT3B kernel equivalence");need(a.map_bytes<=a.map_budget && a.store.bytes<=a.store.limit,"OPT3B plan cache bound");
}
void spillAndNegatives(){
 Fixture f;f.setup(16384,6,1);auto&c=*f.pcs;auto&claims=Access::claims(c);auto&roots=Access::roots(c);
 for(unsigned i=0;i<claims.size();++i)claims[i].map=[&f,i](std::size_t j)->Located{++f.map_queries;if(j>=16371)return {{},Fr(7),true};auto x=std::uint64_t(j+17)*std::uint64_t(j+3)+97*i;return {{std::uint32_t((j+i)%6),std::uint32_t(x%16377)},Fr(-3),false};};Access::digest(c);
 baseline::Engine old(roots,claims);baseline::Engine::Authentication b(claims.size(),false);Access::Auth a(claims.size());a.map_bytes=a.map_budget-8192;
 Access::Auth exhausted(claims.size());exhausted.map_bytes=exhausted.map_budget-8192;exhausted.store.limit=0;std::vector<Fr>lambda(claims.size(),Fr(3));
 for(std::size_t i=0;i<claims.size();++i){auto bias=old.prepareClaim(claims[i],b,i);need(Access::prepare(c,a,i)==bias && Access::prepare(c,exhausted,i)==bias,"OPT3B spill bias");need(a.claims[i].frozen && !a.claims[i].disk_roots.empty(),"OPT3B fragmented plan did not stream");need(!exhausted.claims[i].frozen && std::string(exhausted.claims[i].reason)=="FILE_BUDGET_EXHAUSTED","OPT3B file budget did not fallback");checkPlanIdentity(claims[i],a,i,roots.size());}
 auto reference=old.partialDot(lambda,b);need(Access::dots(c,lambda,a)==reference && Access::dots(c,lambda,exhausted)==reference,"OPT3B spill/fallback dot");for(std::size_t k=0;k<roots.size();++k){auto expected=old.kernel(k,lambda,b);need(Access::kernel(c,k,lambda,a)==expected && Access::kernel(c,k,lambda,exhausted)==expected,"OPT3B spill/fallback kernel");}
 auto before=f.map_queries;Access::dots(c,lambda,a);for(std::size_t k=0;k<roots.size();++k)Access::kernel(c,k,lambda,a);need(before==f.map_queries,"OPT3B streamed frozen plan reused callback");
 auto good=claims[0].map;claims[0].map=[](std::size_t j)->Located{return {{99,std::uint32_t(j)},Fr(0),false};};Access::Auth bad(claims.size());rejectsB([&]{Access::prepare(c,bad,0);},"OPT3B wrong root accepted");claims[0].map=[](std::size_t)->Located{return {{0,16377},Fr(0),false};};rejectsB([&]{Access::prepare(c,bad,0);},"OPT3B outside actual accepted");
 claims[0].map=[good](std::size_t j){auto l=good(j);l.bias+=Fr(1);return l;};rejectsB([&]{Access::prepare(c,bad,0);},"OPT3B changed digest accepted");rejectsB([&]{Access::kernel(c,0,lambda,exhausted);},"OPT3B mutable fallback accepted");
 claims[0].map=good;claims[0].only_root=0;rejectsB([&]{Access::prepare(c,bad,0);},"OPT3B invalid root hint accepted");claims[0].only_root=-1;
 claims[0].map=[](std::size_t)->Located{throw std::runtime_error("frozen callback reused");};Access::kernel(c,0,lambda,a);Access::dots(c,lambda,a);
 need(a.map_bytes<=a.map_budget && a.store.bytes<=a.store.limit && exhausted.store.bytes==0,"OPT3B exhausted bound");mapping::RootSpan wrong;wrong.head=a.store.bytes+1;wrong.runs=1;rejectsB([&]{a.store.each(wrong,[](const auto&){});},"OPT3B invalid file bounds accepted");
 std::cout<<"OPT3B_STREAMED_PLAN_NEGATIVES_PASS\n";
}
void benchmarkB(Fixture&f,const std::string&name,std::size_t n){
 auto&c=*f.pcs;auto&claims=Access::claims(c);auto&roots=Access::roots(c);baseline::Engine old(roots,claims);baseline::Engine::Authentication b(claims.size(),true);Access::Auth a(claims.size(),true);std::vector<Fr>lambda;for(std::size_t i=0;i<claims.size();++i)lambda.push_back(f.rng.scalar());
 Measurement oldMap,newMap,oldKernel,newKernel,oldPartial,newPartial,oldSparse,newSparse,oldReuse,newReuse;Fr bias=0,expected=0;
 oldMap.measure([&]{for(std::size_t i=0;i<claims.size();++i)expected+=lambda[i]*old.prepareClaim(claims[i],b,i);},f.map_queries,f.rowmajor_queries);
 newMap.measure([&]{for(std::size_t i=0;i<claims.size();++i)bias+=lambda[i]*Access::prepare(c,a,i);},f.map_queries,f.rowmajor_queries);need(bias==expected,"OPT3B benchmark bias");
 std::vector<Fr>oldDot,newDot;oldPartial.measure([&]{oldDot=old.partialDot(lambda,b);},f.map_queries,f.rowmajor_queries);newPartial.measure([&]{newDot=Access::dots(c,lambda,a);},f.map_queries,f.rowmajor_queries);need(oldDot==newDot,"OPT3B benchmark dot");
 for(std::size_t k=0;k<roots.size();++k){std::vector<Fr>before,after;auto first=[&]{oldKernel.measure([&]{before=old.kernel(k,lambda,b);},f.map_queries,f.rowmajor_queries);};auto second=[&]{newKernel.measure([&]{after=Access::kernel(c,k,lambda,a);},f.map_queries,f.rowmajor_queries);};if(k%2){second();first();}else{first();second();}need(before==after,"OPT3B benchmark elementwise K");}
 const auto firstMetrics=Access::metrics(c);const auto oldFirstMetrics=old.auth_metrics_;
 std::size_t sparse=roots.size()>11?11:0;std::vector<Fr>before,after;
 oldSparse.measure([&]{before=old.kernel(sparse,lambda,b);},f.map_queries,f.rowmajor_queries);newSparse.measure([&]{after=Access::kernel(c,sparse,lambda,a);},f.map_queries,f.rowmajor_queries);need(before==after,"OPT3B sparse root mismatch");
 std::size_t reuse=0;oldReuse.measure([&]{before=old.kernel(reuse,lambda,b);},f.map_queries,f.rowmajor_queries);newReuse.measure([&]{after=Access::kernel(c,reuse,lambda,a);},f.map_queries,f.rowmajor_queries);need(before==after,"OPT3B plan reuse mismatch");
 std::size_t oldFrozen=0,newFrozen=0,ram=0,file=0;for(auto&p:b.claims)oldFrozen+=p.frozen;for(auto&p:a.claims){newFrozen+=p.frozen;ram+=p.frozen && p.disk_roots.empty();file+=p.frozen && !p.disk_roots.empty();}struct rusage usage{};getrusage(RUSAGE_SELF,&usage);
 std::cout<<std::setprecision(12)<<"OPT3B_BENCH {\"fixture\":\""<<name<<"\",\"n\":"<<n<<",\"roots\":"<<roots.size()<<",\"claims\":"<<claims.size()<<",\"old_mapping\":";oldMap.json();std::cout<<",\"new_mapping\":";newMap.json();std::cout<<",\"old_kernel\":";oldKernel.json();std::cout<<",\"new_kernel\":";newKernel.json();std::cout<<",\"old_partial\":";oldPartial.json();std::cout<<",\"new_partial\":";newPartial.json();std::cout<<",\"old_sparse_root\":";oldSparse.json();std::cout<<",\"new_sparse_root\":";newSparse.json();std::cout<<",\"old_reuse_root0\":";oldReuse.json();std::cout<<",\"new_reuse_root0\":";newReuse.json();
 std::cout<<",\"old_frozen\":"<<oldFrozen<<",\"new_frozen\":"<<newFrozen<<",\"RAM_claims\":"<<ram<<",\"file_claims\":"<<file<<",\"old_RAM_bytes\":"<<b.map_bytes<<",\"new_RAM_bytes\":"<<a.map_bytes<<",\"file_bytes\":"<<a.store.bytes<<",\"file_read_bytes_including_reuse\":"<<a.store.read_bytes<<",\"old_fallback_map_queries\":"<<oldFirstMetrics.fallback_map_queries<<",\"new_fallback_map_queries\":"<<firstMetrics.fallback_map_queries<<",\"old_kernel_coordinates\":"<<oldFirstMetrics.kernel_coordinates<<",\"new_kernel_coordinates\":"<<firstMetrics.kernel_coordinates<<",\"old_kernel_cpu_seconds\":"<<oldFirstMetrics.kernel_cpu<<",\"new_kernel_cpu_seconds\":"<<firstMetrics.kernel_cpu<<",\"old_map_cpu_seconds\":"<<oldFirstMetrics.map_cpu<<",\"new_map_cpu_seconds\":"<<firstMetrics.map_cpu<<",\"combined_process_peak_rss_kib\":"<<usage.ru_maxrss<<",\"Fr_elementwise_equal\":true,\"partialDot_equal\":true,\"aggregate_equal\":true}"<<std::endl;
}
void partialExhaustionAndIO(){
 Fixture f;f.setup(16384,6,1);auto&c=*f.pcs;auto&claims=Access::claims(c);claims.resize(2);auto&roots=Access::roots(c);
 for(unsigned i=0;i<claims.size();++i)claims[i].map=[i](std::size_t j)->Located{if(j>=16371)return {{},Fr(7),true};return {{std::uint32_t((j+i)%6),std::uint32_t((std::uint64_t(j+17)*(j+3)+i)%16377)},Fr(-3),false};};Access::digest(c);
 Access::Auth a(2);a.map_bytes=a.map_budget-8192;Access::prepare(c,a,0);need(a.claims[0].frozen && a.store.bytes>0,"OPT3B prior chain missing");auto before=a.store.bytes;a.store.limit=before+8192;Access::prepare(c,a,1);need(!a.claims[1].frozen && a.store.bytes>before && a.store.bytes<=a.store.limit,"OPT3B partial exhaustion not exercised");checkPlanIdentity(claims[0],a,0,roots.size());
 baseline::Engine reference(roots,claims);baseline::Engine::Authentication b(2,false);for(unsigned i=0;i<2;++i)reference.prepareClaim(claims[i],b,i);std::vector<Fr>lambda{Fr(3),Fr(5)};need(Access::dots(c,lambda,a)==reference.partialDot(lambda,b),"OPT3B prior chain/capped fallback dot");for(unsigned k=0;k<roots.size();++k)need(Access::kernel(c,k,lambda,a)==reference.kernel(k,lambda,b),"OPT3B prior chain/capped fallback K");
 mapping::Run run{0,4,{0,0},1,1};mapping::RootSpan span;mapping::Store store(4096);
 struct rlimit old{};need(::getrlimit(RLIMIT_NOFILE,&old)==0,"OPT3B getrlimit");auto limited=old;limited.rlim_cur=0;need(::setrlimit(RLIMIT_NOFILE,&limited)==0,"OPT3B test open limit");bool openRejected=false;try{store.append(span,&run,1);}catch(const std::exception&){openRejected=true;}need(::setrlimit(RLIMIT_NOFILE,&old)==0 && openRejected,"OPT3B open failure not closed");
 ioFault::writing=ioFault::partialFailure;ioFault::writes=0;rejectsB([&]{store.append(span,&run,1);},"OPT3B partial pwrite accepted");ioFault::writing=ioFault::normal;need(store.bytes==0 && span.head==mapping::absent,"OPT3B failed write published plan");
 ioFault::writing=ioFault::interruptAndShort;ioFault::writes=0;need(store.append(span,&run,1),"OPT3B short/EINTR write failed");ioFault::writing=ioFault::normal;
 ioFault::reading=ioFault::interruptAndShort;ioFault::reads=0;unsigned count=0;store.each(span,[&](const auto&r){need(r.begin==run.begin && r.count==run.count && r.first==run.first,"OPT3B short read content");++count;});ioFault::reading=ioFault::normal;need(count==1,"OPT3B short read count");
 for(auto mode:{ioFault::partialFailure,ioFault::endEarly}){ioFault::reading=mode;ioFault::reads=0;rejectsB([&]{store.each(span,[](const auto&){});},"OPT3B failed/truncated pread accepted");ioFault::reading=ioFault::normal;}
 std::cout<<"OPT3B_PARTIAL_EXHAUSTION_IO_FAILURES_PASS\n";
}
// Test-only original authentication control/message sequence with the independent
// OPT3-A Engine substituted for its three numerical helpers. Tiny roots keep
// cryptographic work small while a 32768-entry map forces streaming runs.
void referenceAuthenticate(Commitments&pcs,const Layout&layout,Tape&io,Random*rng){
 auto&claims=Access::claims(pcs);auto&roots=Access::roots(pcs);auto session=io.session();auto id=identity(layout);Writer summary;summary.text(classified::suite);summary.text(id);summary.number(claims.size());for(auto&c:claims)summary.text(c.id);bindPublic(io,"Classified/all-claims-fixed",summary.b);
 baseline::Engine engine(roots,claims);baseline::Engine::Authentication auth(claims.size(),false);std::vector<Fr>lambda;std::vector<Hidden>values;Fr bias=0;for(std::size_t i=0;i<claims.size();++i){auto local=engine.prepareClaim(claims[i],auth,i);lambda.push_back(io.coin("Classified/lambda/"+std::to_string(i)));need(!lambda.back().isZero(),"reference zero lambda");values.push_back(claims[i].value);bias+=lambda.back()*local;}
 auto initial=combine(session,"Classified/aggregate",values,lambda);initial.value-=bias;initial.handle.point-=scalarCommit(bias,Fr(0));bindPublic(io,"Classified/aggregate",encode(initial.handle));std::vector<Hidden>partial;auto dot=engine.partialDot(lambda,auth);for(std::size_t k=0;k<roots.size();++k)partial.push_back(exchangeHidden(io,"Classified/partial/"+std::to_string(k),dot[k],rng));
 auto witnesses=partial;witnesses.push_back(initial);Relations sum;sum.role="Classified/partial-sum";for(auto&h:witnesses)sum.handles.push_back(h.handle);std::vector<Fr>coeff(witnesses.size(),Fr(1));coeff.back()=Fr(-1);sum.linear={coeff};sigma(io,sum,&witnesses,rng);
 for(std::size_t k=0;k<roots.size();++k){auto K=engine.kernel(k,lambda,auth);auto name="Classified/root-proof/"+std::to_string(k);std::vector<Value>functions{roots[k].value,[&K](std::size_t j){return K.at(j);}};auto end=expressionSumcheck(io,roots[k].spec.padded,partial[k],functions,Expression{2,{{Fr(1),{0,1}}}},rng,name+"/sumcheck");auto expected=r1::mle(K,end.point);equalHidden(io,end.endpoints[1],expected,rng,name+"/kernel");openVector(io,roots[k].pc,{end.point,end.endpoints[0].handle},&*roots[k].secret,&end.endpoints[0],rng,name+"/opening");}
 io.coin("Classified/all-claims-authenticated/ack");
}
Bytes spilledAuthentication(bool reference,bool prover,const std::vector<std::pair<std::string,Bytes>>*input,std::vector<std::pair<std::string,Bytes>>*output){
 Layout l;l.native=std::string(64,'a');l.roots={{"C_aux",5,8,true,false},{"helper",8,8,false,false}};for(unsigned j=0;j<5;++j)l.base.push_back({0,j});View v;v.id="helper/view";v.actual=8;v.padded=8;v.start={1,0};v.kind=ObjectKind::ExpHelper;l.views.emplace(v.id,v);
 std::vector<Fr>values{Fr(2),Fr(3),Fr(5),Fr(7),Fr(11),Fr(13),Fr(17),Fr(19)};Prepared data;if(prover)data[v.id]=std::make_shared<std::vector<Fr>>(values);FixedRandom random(713);Random*rng=prover?&random:nullptr;Tape io;io.p=prover;if(input)io.data=*input;
 Commitments pcs(io,l,prover?Value([](std::size_t j){return Fr(j+1);}):Value{},prover?&data:nullptr,rng);pcs.constrainBaseTails();Claim claim;claim.id="fixture/interleaved-streamed";claim.map_id="fixture/public-irregular-map";claim.padded=32768;for(unsigned j=0;j<15;++j)claim.point.push_back(Fr(j+3));claim.map=[](std::size_t j)->Located{if(j>=32755)return {{},Fr(7),true};auto k=unsigned(j%2);auto at=std::uint32_t((j*j+17*j+3)%(k?8:5));return {{k,at},Fr(-3),false};};
 Fr value=0;if(prover){auto weights=r1::weights(claim.point);for(std::size_t j=0;j<claim.padded;++j){auto at=claim.map(j);value+=weights[j]*(at.constant?at.bias:(at.coordinate.root?values[at.coordinate.index]:Fr(at.coordinate.index+1))+at.bias);}}
 claim.value=exchangeHidden(io,"fixture/streamed-value",value,rng);pcs.defer(std::move(claim));if(reference)referenceAuthenticate(pcs,l,io,rng);else pcs.authenticate();
 if(!prover)need(io.cursor==io.data.size(),"OPT3B spilled verifier EOF");if(output)*output=io.data;Writer out;for(auto&m:io.data){out.text(m.first);out.number(m.second.size(),8);out.raw(m.second);}auto digest=io.prefix();out.raw(Bytes(digest.begin(),digest.end()));return out.b;
}
void spilledTranscript(const char*prefix){
 std::vector<std::pair<std::string,Bytes>>oldTape,newTape;auto before=spilledAuthentication(true,true,nullptr,&oldTape);auto after=spilledAuthentication(false,true,nullptr,&newTape);need(before==after,"OPT3B spilled full transcript differs");need(after==spilledAuthentication(false,false,&newTape,nullptr),"OPT3B spilled verifier transcript differs");
 for(auto pair:{std::make_pair(std::string(prefix)+"-before.bin",&before),std::make_pair(std::string(prefix)+"-after.bin",&after)}){std::ifstream existing(pair.first,std::ios::binary);need(!existing.good(),"OPT3B transcript path exists");std::ofstream out(pair.first,std::ios::binary);out.write(reinterpret_cast<const char*>(pair.second->data()),pair.second->size());need(bool(out),"OPT3B transcript output");}
 std::cout<<"OPT3B_SPILLED_TRANSCRIPT_PASS messages="<<newTape.size()<<" bytes="<<after.size()<<"\n";
}
int main(int argc,char**argv){try{r1::init();need(argc>=2,"OPT3B mode required");std::string mode=argv[1];if(mode=="profile"){need(argc==3,"profile input");profile(argv[2]);}else if(mode=="contract")rootCentricContract();else if(mode=="correctness"){rootCentricContract();for(auto n:{1024u,16384u})for(unsigned pattern=0;pattern<5;++pattern)for(bool tight:{false,true})equivalenceB(n,pattern,tight);spillAndNegatives();partialExhaustionAndIO();std::cout<<"OPT3B_MAPPING_EQUIVALENCE_PASS\n";}
 else if(mode=="io-correctness")partialExhaustionAndIO();
 else if(mode=="spilled-transcript"){need(argc==3,"spilled transcript prefix");spilledTranscript(argv[2]);}
 else if(mode=="bench"){need(argc==3,"bench size");auto n=std::stoull(argv[2]);Fixture f;f.setup(n,45,1);benchmarkB(f,"interleaved45roots",n);}
 else if(mode=="native-bench"){need(argc==4,"native bench input and selected ordinals");NativeMaps native(argv[2]);std::ifstream input(argv[3]);std::set<std::size_t>selected;std::size_t i;while(input>>i)selected.insert(i);auto&claims=Access::claims(*native.f.pcs);std::vector<Claim>sample;for(std::size_t j=0;j<claims.size();++j)if(selected.count(j))sample.push_back(std::move(claims[j]));need(sample.size()==24,"native sample identity count");claims=std::move(sample);benchmarkB(native.f,"native24fragmented-45roots",1u<<24);}
 else throw std::runtime_error("OPT3B unknown mode");return 0;}catch(const std::exception&e){std::cerr<<"OPT3B_FAILED "<<e.what()<<'\n';return 1;}}
