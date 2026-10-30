#include "opt3a_reference.hpp"
#include "fs_io.hpp"
#include "paper_r2/r6_model.hpp"
#include <iomanip>
#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <sys/resource.h>
using namespace zkgpt::r7;using namespace oneblock;using namespace classified;
namespace ref=zkgpt::r7::oneblock::classified::opt3a_reference;
namespace zkgpt::r7::oneblock::classified {
struct AuthenticationTestAccess {
 using Auth=Commitments::Authentication;
 static auto&roots(Commitments&c){return c.roots_;}static auto&claims(Commitments&c){return c.claims_;}
 static Fr prepare(Commitments&c,Auth&a,std::size_t i){return c.prepareClaim(c.claims_[i],a,i);}
 static auto kernel(Commitments&c,std::size_t k,const std::vector<Fr>&l,const Auth&a){return c.kernel(k,l,a);}
 static auto dots(Commitments&c,const std::vector<Fr>&l,const Auth&a){return c.partialDot(l,a);}
 static void digest(Commitments&c){for(auto&a:c.claims_)a.map_digest=c.mapDigest(a);}
 static auto&metrics(Commitments&c){return c.auth_metrics_;}
};
}
using Access=AuthenticationTestAccess;
struct FixedRandom:Random {
 std::mt19937_64 random;explicit FixedRandom(std::uint64_t seed):random(seed){}
 void fill(std::uint8_t*p,std::size_t n)override{for(std::size_t i=0;i<n;++i){if(!(i%8))word=random();p[i]=std::uint8_t(word>>(8*(i%8)));}}std::uint64_t word=0;
};
struct Tape:IO {
 bool p=true;std::size_t cursor=0;std::vector<std::pair<std::string,Bytes>>data;
 zkgpt::r7::fs::CanonicalTranscript transcript{Bytes{1,3,3,7},Bytes{7,3,1}};
 bool prover()const override{return p;}std::string session()const override{return std::string(64,'a');}
 Digest prefix()const override{return transcript.prefix();}
 Bytes message(const std::string&n,const Bytes&b={})override{Bytes out;if(p){out=b;data.emplace_back(n,b);}else{need(cursor<data.size() && data[cursor].first==n,"OPT3A tape order");out=data[cursor++].second;}transcript.message(n,out);return out;}
 Fr coin(const std::string&n)override{return transcript.coin(n);}
};
struct Fixture {
 Layout layout;Prepared prepared;FixedRandom rng{1771};Tape io;std::unique_ptr<Commitments>pcs;
 std::uint64_t map_queries=0,rowmajor_queries=0;
 Fixture(){layout.native=std::string(64,'a');layout.roots={{"fixture/base",1,1,true,false}};layout.base={{0,0}};pcs=std::make_unique<Commitments>(io,layout,[](std::size_t){return Fr(1);},&prepared,&rng);}
 void setup(std::size_t n,std::size_t roots,unsigned pattern,unsigned values=0){
  auto&rr=Access::roots(*pcs);rr.clear();auto table=std::make_shared<std::vector<Fr>>(4096);for(auto&v:*table)v=rng.scalar(true);
  for(std::size_t k=0;k<roots;++k){Root r;r.spec={"fixture/"+std::to_string(k),n-7,n,k%2==0,k%2==0};r.value=[table,k,values](std::size_t j){if(values==1)return Fr(0);if(values==2)return Fr(1);if(values==3)return Fr(j==19);return table->at((j+97*k)%table->size());};rr.push_back(std::move(r));}
  auto&cc=Access::claims(*pcs);cc.clear();
  for(unsigned i=0;i<4;++i){Claim a;a.id="fixture/claim/"+std::to_string(i);a.padded=n;for(unsigned j=0;j<r1::log2exact(n);++j)a.point.push_back(rng.scalar());
   a.map=[this,n,roots,i,pattern](std::size_t j)->Located{++map_queries;if(j>=n-13)return {{},Fr(7),true};auto root=std::uint32_t((j/(n/roots+1)+i)%roots);auto local=j%(n/roots+1);std::size_t at;
    if(pattern==0)at=local;else if(pattern==1)at=3*local;else if(pattern==2)at=n-20-local;else if(pattern==3)at=19;else at=(j*257+17)% (n-7);
    return {{root,std::uint32_t(at)},Fr(int(i)-2),false};};cc.push_back(std::move(a));}
  Access::digest(*pcs);
 }
};
void contract(){Fixture f;f.setup(1024,6,1);Access::Auth a(4);a.map_bytes=a.map_budget-16*sizeof(Access::Auth::Run);Access::prepare(*f.pcs,a,0);need(a.claims[0].frozen,"OPT3A strided map must freeze within bounded cache");auto before=f.map_queries;std::vector<Fr>l(4,Fr(3));Access::claims(*f.pcs).resize(1);l.resize(1);Access::kernel(*f.pcs,0,l,a);need(f.map_queries==before,"frozen strided map retained callback");std::cout<<"OPT3A_BOUNDED_STRIDED_MAP_PASS\n";}
void equivalence(std::size_t n,unsigned pattern,unsigned values){
 Fixture f;f.setup(n,6,pattern,values);auto&c=*f.pcs;auto&claims=Access::claims(c);auto&roots=Access::roots(c);Access::Auth a(claims.size());ref::Authentication b(claims.size(),false);std::vector<Fr>lambda;Fr bias=0,expectedBias=0;
 for(std::size_t i=0;i<claims.size();++i){auto l=values==4?Fr(0):values==5?Fr(i==1):f.rng.scalar();lambda.push_back(l);bias+=l*Access::prepare(c,a,i);expectedBias+=l*ref::prepare(claims[i],roots,b,i);}need(bias==expectedBias,"OPT3A affine bias mismatch");
 auto gotDot=Access::dots(c,lambda,a);auto refDot=ref::partialDot(lambda,roots,claims,b);need(gotDot==refDot,"OPT3A partialDot mismatch");Fr aggregate=bias,referenceAggregate=expectedBias;
 for(std::size_t k=0;k<roots.size();++k){auto got=Access::kernel(c,k,lambda,a);auto expected=ref::kernel(k,lambda,roots,claims,b);need(got==expected,"OPT3A elementwise kernel mismatch");Fr dot=0;for(std::size_t j=0;j<got.size();++j)dot+=got[j]*roots[k].value(j);need(dot==gotDot[k],"OPT3A kernel/dot relation mismatch");aggregate+=gotDot[k];referenceAggregate+=refDot[k];
  if(n<=16384){std::vector<Fr>dense(n,Fr(0));for(std::size_t i=0;i<claims.size();++i){auto w=r1::weights(claims[i].point);for(std::size_t j=0;j<n;++j){auto l=claims[i].map(j);if(!l.constant && l.coordinate.root==k)dense[l.coordinate.index]+=lambda[i]*w[j];}}need(got==dense,"OPT3A independent dense Fr mismatch");}
 }need(aggregate==referenceAggregate,"OPT3A aggregate authentication claim mismatch");
 std::cout<<"{\"test\":\"equivalence\",\"n\":"<<n<<",\"pattern\":"<<pattern<<",\"values\":"<<values<<",\"status\":\"PASS\"}\n";
}

void edgeCases(){
 for(std::size_t n:{1u,2u,4u,8u})for(unsigned query=0;query<3;++query)for(unsigned mapping=0;mapping<4;++mapping){
  Fixture f;auto&roots=Access::roots(*f.pcs);roots.resize(2);for(std::size_t k=0;k<2;++k){roots[k].spec={"edge/"+std::to_string(k),n,n,k==0,k==0};roots[k].value=[k](std::size_t j){return Fr(j+1+3*k);};}
  auto&claims=Access::claims(*f.pcs);claims.clear();Claim c;c.id="edge";c.padded=n;for(unsigned i=0;i<r1::log2exact(n);++i)c.point.push_back(Fr(query==2?i%2:query));
  c.map=[n,mapping](std::size_t j)->Located{if(mapping==0)return {{},Fr(7),true};if(mapping==1 && j+1==n)return {{},Fr(0),true};return {{std::uint32_t(j%2),std::uint32_t(mapping==3?n-1-j:j)},Fr(mapping==2?-3:0),false};};claims.push_back(std::move(c));Access::digest(*f.pcs);
  Access::Auth a(1);ref::Authentication b(1,false);need(Access::prepare(*f.pcs,a,0)==ref::prepare(claims[0],roots,b,0),"edge affine bias");
  for(Fr lambda:{Fr(0),Fr(1),Fr(-1)}){std::vector<Fr>l{lambda};need(Access::dots(*f.pcs,l,a)==ref::partialDot(l,roots,claims,b),"edge partialDot");for(std::size_t k=0;k<2;++k)need(Access::kernel(*f.pcs,k,l,a)==ref::kernel(k,l,roots,claims,b),"edge kernel");}
 }
 // Public maximum supported coordinate deltas, without allocating a huge root.
 for(bool reverse:{false,true}){Fixture f;auto&roots=Access::roots(*f.pcs);roots[0].spec.actual=roots[0].spec.padded=r1::kMaxVector;roots[0].value=[](std::size_t j){return Fr(j+1);};auto&claims=Access::claims(*f.pcs);claims.clear();Claim c;c.id="large-stride";c.padded=2;c.point={Fr(1)};c.map=[reverse](std::size_t j)->Located{return {{0,std::uint32_t((j!=std::size_t(reverse))?r1::kMaxVector-1:0)},Fr(0),false};};claims.push_back(std::move(c));Access::digest(*f.pcs);Access::Auth a(1);ref::Authentication b(1,false);Access::prepare(*f.pcs,a,0);ref::prepare(claims[0],roots,b,0);need(Access::dots(*f.pcs,{Fr(1)},a)==ref::partialDot({Fr(1)},roots,claims,b),"large signed stride partialDot");}
 std::cout<<"OPT3A_EDGE_CASES_PASS padded=1,2,4,8 query=0,1,mixed lambda=0,1,-1 constant-and-singleton-map max-supported-signed-stride\n";
}
void transcript(const char*path){
 Layout l;l.native=std::string(64,'a');l.roots={{"C_aux",5,8,true,false},{"helper",8,8,false,false}};for(unsigned j=0;j<5;++j)l.base.push_back({0,j});View v;v.id="helper/view";v.actual=5;v.padded=8;v.start={1,0};v.kind=ObjectKind::ExpHelper;l.views.emplace(v.id,v);
 std::vector<Fr>values{Fr(2),Fr(3),Fr(5),Fr(7),Fr(11),Fr(13),Fr(17),Fr(19)};
 Prepared data;data[v.id]=std::make_shared<std::vector<Fr>>(values);FixedRandom rng(713);Tape p;Commitments pcs(p,l,[](std::size_t j){return Fr(j+1);},&data,&rng);pcs.constrainBaseTails();auto view=pcs.view(v.id);std::vector<Fr>q{Fr(3),Fr(5),Fr(7)};auto h=exchangeHidden(p,"fixture/value",r1::mle(values,q),&rng);
 view.open(p,{q,h.handle},&h,&rng,"fixture/view");pcs.authenticate();
 Tape rx;rx.p=false;rx.data=p.data;Commitments verifier(rx,l,{},nullptr,nullptr);verifier.constrainBaseTails();auto vv=verifier.view(v.id);auto vh=exchangeHidden(rx,"fixture/value",Fr(0),nullptr);vv.open(rx,{q,vh.handle},nullptr,nullptr,"fixture/view");verifier.authenticate();
 need(rx.cursor==rx.data.size() && rx.prefix()==p.prefix(),"OPT3A transcript replay/EOF");Writer out;for(auto&m:p.data){out.text(m.first);out.number(m.second.size(),8);out.raw(m.second);}auto digest=p.prefix();out.raw(Bytes(digest.begin(),digest.end()));
 std::ifstream existing(path,std::ios::binary);need(!existing.good(),"OPT3A transcript output exists");std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(out.b.data()),out.b.size());need(bool(file),"OPT3A transcript write");std::cout<<"OPT3A_TRANSCRIPT_PASS messages="<<p.data.size()<<" bytes="<<out.b.size()<<"\n";
}

struct Measurement {
 double cpu=0,wall=0;std::uint64_t allocations=0,bytes=0,maps=0,rowmajor=0;
 template<class F>void measure(F&&f,std::uint64_t&queries,std::uint64_t&coordinateQueries){auto coordinateBefore=coordinateQueries;auto t=std::chrono::steady_clock::now();auto c=processCPU();auto before=queries;crypto::AllocationTrace trace;f();cpu+=processCPU()-c;wall+=std::chrono::duration<double>(std::chrono::steady_clock::now()-t).count();allocations+=trace.count();bytes+=trace.bytes();maps+=queries-before;rowmajor+=coordinateQueries-coordinateBefore;}
 void json()const{std::cout<<"{\"cpu_seconds\":"<<cpu<<",\"wall_seconds\":"<<wall<<",\"cpp_allocations\":"<<allocations<<",\"cpp_allocated_bytes\":"<<bytes<<",\"map_queries\":"<<maps<<",\"RowMajor_base_coordinate_lookups\":"<<rowmajor<<"}";}
};
void benchmark(Fixture&f,const std::string&name,std::size_t n){
 auto&c=*f.pcs;auto&claims=Access::claims(c);auto&roots=Access::roots(c);Access::Auth a(claims.size(),true);ref::Authentication b(claims.size(),true);std::vector<Fr>lambda;for(std::size_t i=0;i<claims.size();++i)lambda.push_back(f.rng.scalar());
 Measurement oldMap,newMap,oldKernel,newKernel,oldPartial,newPartial;Fr oldBias=0,newBias=0;
 oldMap.measure([&]{for(std::size_t i=0;i<claims.size();++i)oldBias+=lambda[i]*ref::prepare(claims[i],roots,b,i);},f.map_queries,f.rowmajor_queries);
 newMap.measure([&]{for(std::size_t i=0;i<claims.size();++i)newBias+=lambda[i]*Access::prepare(c,a,i);},f.map_queries,f.rowmajor_queries);need(oldBias==newBias,"benchmark bias mismatch");
 std::vector<Fr>oldDot,newDot;oldPartial.measure([&]{oldDot=ref::partialDot(lambda,roots,claims,b);},f.map_queries,f.rowmajor_queries);newPartial.measure([&]{newDot=Access::dots(c,lambda,a);},f.map_queries,f.rowmajor_queries);need(oldDot==newDot,"benchmark dot mismatch");
 for(std::size_t k=0;k<roots.size();++k){std::vector<Fr>oldK,newK;
  // Alternate order across roots to reduce systematic warm-up/order bias.
  auto oldCall=[&]{oldKernel.measure([&]{oldK=ref::kernel(k,lambda,roots,claims,b);},f.map_queries,f.rowmajor_queries);};auto newCall=[&]{newKernel.measure([&]{newK=Access::kernel(c,k,lambda,a);},f.map_queries,f.rowmajor_queries);};if(k%2){newCall();oldCall();}else{oldCall();newCall();}
  need(oldK==newK,"benchmark elementwise kernel mismatch");
 }
 struct rusage usage{};getrusage(RUSAGE_SELF,&usage);std::size_t oldFrozen=0,newFrozen=0;for(auto&p:b.claims)oldFrozen+=p.frozen;for(auto&p:a.claims)newFrozen+=p.frozen;auto&m=Access::metrics(c);
 std::cout<<std::setprecision(12)<<"OPT3A_BENCH {\"fixture\":\""<<name<<"\",\"n\":"<<n<<",\"roots\":"<<roots.size()<<",\"claims\":"<<claims.size()<<",\"old_mapping\":";oldMap.json();std::cout<<",\"new_mapping\":";newMap.json();std::cout<<",\"old_kernel\":";oldKernel.json();std::cout<<",\"new_kernel\":";newKernel.json();std::cout<<",\"old_partial\":";oldPartial.json();std::cout<<",\"new_partial\":";newPartial.json();
 std::cout<<",\"old_frozen\":"<<oldFrozen<<",\"new_frozen\":"<<newFrozen<<",\"old_map_cache_bytes\":"<<b.map_bytes<<",\"new_map_cache_bytes\":"<<a.map_bytes<<",\"weight_cache_bytes\":"<<a.weight_bytes<<",\"kernel_calls\":"<<roots.size()<<",\"new_kernel_coordinates\":"<<m.kernel_coordinates<<",\"matched_coordinates\":"<<m.kernel_matched<<",\"new_kernel_and_partial_Fr_multiplications\":"<<m.fr_multiplications<<",\"new_kernel_and_partial_Fr_additions\":"<<m.fr_additions<<",\"old_kernel_and_partial_Fr_multiplications\":"<<5*m.kernel_matched<<",\"old_kernel_and_partial_Fr_additions\":"<<2*m.kernel_matched<<",\"combined_process_peak_rss_kib\":"<<usage.ru_maxrss<<",\"Fr_elementwise_equal\":true,\"partialDot_equal\":true,\"authentication_claim_equal\":true}"<<std::endl;
}
void nativeBenchmark(){
 auto context=master3::trustedContext(12);auto layout=makeLayout(context);need(layout.roots.size()==45,"native Full12 root count");
 LLM nn(12,12,64,768,3072);auto graph=[&]{master3::QuietStdout quiet;zkgpt::r6::configure(nn);return nn.buildPublicCircuit();}();Fixture f;auto&roots=Access::roots(*f.pcs);roots.clear();for(auto&s:layout.roots){Root r;r.spec=s;r.value=[](std::size_t j){return Fr(1+j%29);};roots.push_back(std::move(r));}
 auto&claims=Access::claims(*f.pcs);claims.clear();
 // Real public RowMajor source routing and native GKR terminal index arrays.
 const View*selected=nullptr;for(auto&entry:layout.views){auto&v=entry.second;if(v.mapped && v.padded<=(1u<<24) && (!selected || v.padded>selected->padded))selected=&v;}
 need(selected,"native mapped view missing");Claim v;v.id=selected->id;v.padded=selected->padded;v.map=[&f,&layout,selected](std::size_t j){++f.map_queries;auto out=locate(layout,*selected,j);f.rowmajor_queries+=!out.constant;return out;};claims.push_back(std::move(v));
 std::size_t bestLayer=0,bestSize=0;bool bestPort=false;
 for(std::size_t i=1;i<graph.circuit.size();++i)for(bool port:{false,true}){auto&ids=port?graph.circuit[i].ori_id_v:graph.circuit[i].ori_id_u;if(ids.size()>bestSize && ids.size()<=(1u<<24)){bestSize=ids.size();bestLayer=i;bestPort=port;}}
 need(bestSize,"native terminal missing");auto*ids=bestPort?&graph.circuit[bestLayer].ori_id_v:&graph.circuit[bestLayer].ori_id_u;Claim t;t.id="native/layer/"+std::to_string(bestLayer)+(bestPort?"/v":"/u");t.padded=power2(bestSize);t.map=[&f,&layout,ids,bestSize](std::size_t j)->Located{++f.map_queries;if(j>=bestSize)return {{},Fr(0),true};++f.rowmajor_queries;return {baseCoordinate(layout,ids->at(j)),Fr(0),false};};claims.push_back(std::move(t));
 for(auto&c:claims)for(unsigned j=0;j<r1::log2exact(c.padded);++j)c.point.push_back(f.rng.scalar());Access::digest(*f.pcs);
 std::cerr<<"NATIVE_BENCH_LAYOUT "<<identity(layout)<<" source="<<claims[0].id<<" source_padded="<<claims[0].padded<<" terminal="<<claims[1].id<<" terminal_padded="<<claims[1].padded<<" roots=45 physical_root_max=16777216\n";
 benchmark(f,"native-full12-public-map-45roots",1u<<24);
}
int main(int argc,char**argv){try{r1::init();std::string mode=argc>1?argv[1]:"correctness";
 if(mode=="contract")contract();
 else if(mode=="correctness"){edgeCases();for(unsigned values=0;values<6;++values)for(unsigned pattern=0;pattern<5;++pattern)equivalence(1024,pattern,values);for(unsigned p=0;p<5;++p)equivalence(16384,p,0);std::cout<<"OPT3A_KERNEL_EQUIVALENCE_PASS\n";}
 else if(mode=="bench"){need(argc==3,"bench requires size");auto n=std::stoull(argv[2]);Fixture f;f.setup(n,6,1);benchmark(f,"strided-cross-root-affine",n);}
 else if(mode=="native-bench")nativeBenchmark();
 else if(mode=="transcript"){need(argc==3,"transcript output path");transcript(argv[2]);}
 else throw std::runtime_error("unknown OPT3A mode");return 0;
 }catch(const std::exception&e){std::cerr<<"OPT3A_FAILED "<<e.what()<<'\n';return 1;}}
