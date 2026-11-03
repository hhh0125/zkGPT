// Baseline OPT3-C fixture retained verbatim except its entrypoint name.
// Reuse the public mapping reader and deterministic public test input provider.
// This executable is a benchmark, never a production witness/proof generator.
#define main opt3a_fixture_main
#include "opt3a_kernel_tests.cpp"
#undef main
#include "public_k.hpp"
namespace ui=zkgpt::r7::oneblock::unified;
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

void foldingBench(Fixture&f,const std::string&fixture,std::size_t n,const std::string&mode){
 auto&c=*f.pcs;auto&claims=Access::claims(c);auto&roots=Access::roots(c);Access::Auth auth(claims.size(),true);
 std::vector<Fr>rho;for(std::size_t i=0;i<claims.size();++i){Access::prepare(c,auth,i);rho.push_back(Fr(int(i%97)+3));}
 std::vector<ui::Segment>segments{{0,fixture=="Full12-public-mapping"?32768u:4u}};
 std::array<std::size_t,4>cursor{};
 for(std::size_t k=0;k<roots.size();++k){auto&r=roots[k];auto category=fixture=="Full12-public-mapping"?(r.spec.model?1u:(r.spec.id.rfind("C_aux",0)==0?2u:3u)):unsigned(k%3+1);auto padded=r.spec.padded;auto&at=cursor[category];at=(at+padded-1)&~(padded-1);segments.push_back({category*(std::size_t(1)<<30)+at,padded});at+=padded;}
 const auto domain=std::size_t(1)<<32;ui::validate(domain,segments);Access::metrics(c)=AuthMetrics{};
 auto started=std::chrono::steady_clock::now();auto cpu=processCPU();crypto::AllocationTrace allocations;
 std::unique_ptr<ui::PublicK>state;ui::KBudget cap=mode=="ram"?ui::KBudget{2ull<<30,0}:mode=="file"?ui::KBudget{0,4ull<<30}:ui::KBudget{512ull<<20,4ull<<30};
 std::uint64_t capacity=0;for(auto s:segments)capacity+=s.padded*sizeof(Fr);if(mode=="ram")need(capacity<=cap.ram,"benchmark RAM mode exceeds hard cap; report infeasible rather than allocating");
 auto build=[&](std::size_t k){return k?Access::kernel(c,k-1,rho,auth):std::vector<Fr>(segments[0].padded,Fr(0));};
 if(fixture!="Full12-public-mapping" && mode=="hybrid")cap={std::min<std::uint64_t>(512ull<<20,n*16),std::min<std::uint64_t>(4ull<<30,n*32)};
 if(mode!="reference")state=std::make_unique<ui::PublicK>(segments,build,cap,f.io.session(),domain);
 ui::Factors factors=[&](std::size_t k,const ui::Consume&consume){Value a=k?roots[k-1].value:Value([](std::size_t j){return Fr(j+11);});if(state)state->withRoot(k,f.io.session(),[&](const Value&b){consume(a,b);});else{auto b=build(k);consume(a,[&](std::size_t j){return b.at(j);});}};
 std::vector<Fr>point;Writer digest;std::vector<double>rounds;ui::Round last;std::uint64_t u=0;
 for(unsigned t=0;t<32;++t){auto roundStart=std::chrono::steady_clock::now();last=state?ui::quadraticRoundFolded(domain,segments,point,factors):ui::quadraticRound(domain,segments,point,factors);for(auto&v:last.coefficients)digest.scalar(v);auto r=Fr(-3-int(t));if(state)state->advance(f.io.session(),r);point.push_back(r);digest.scalar(r);rounds.push_back(std::chrono::duration<double>(std::chrono::steady_clock::now()-roundStart).count());for(auto&r:roots)u+=r.spec.padded;}
 Fr endpoint=0;if(state)endpoint=state->endpoint(f.io.session(),point);else for(std::size_t k=0;k<segments.size();++k){auto b=build(k);auto d=r1::log2exact(b.size());auto length=b.size();for(unsigned t=0;t<d;++t){for(std::size_t j=0;j<length/2;++j)b[j]=b[2*j]+point[t]*(b[2*j+1]-b[2*j]);length/=2;}endpoint+=ui::selector(segments[k],point)*b[0];}
 auto r=point.back();need(endpoint==last.kernel_pair[0]+r*(last.kernel_pair[1]-last.kernel_pair[0]),"benchmark terminal K mismatch");digest.scalar(endpoint);digest.scalar(last.input_pair[0]+r*(last.input_pair[1]-last.input_pair[0]));
 auto wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();auto&m=Access::metrics(c);struct rusage usage{};getrusage(RUSAGE_SELF,&usage);
 std::cout<<std::setprecision(12)<<"OPT3C_BENCH {\"fixture\":\""<<fixture<<"\",\"n\":"<<n<<",\"mode\":\""<<mode<<"\",\"roots\":"<<roots.size()<<",\"claims\":"<<claims.size()<<",\"rounds\":32,\"unique_root_padded_bytes\":"<<capacity<<",\"kernel_calls\":"<<m.kernel_calls<<",\"kernel_coordinates\":"<<m.kernel_coordinates<<",\"kernel_cpu_seconds\":"<<m.kernel_cpu<<",\"private_U_scan_coordinates\":"<<u<<",\"unified_sumcheck_seconds\":"<<wall<<",\"cpu_seconds\":"<<processCPU()-cpu<<",\"prover_peak_rss_kib\":"<<usage.ru_maxrss<<",\"cpp_allocations\":"<<allocations.count()<<",\"cpp_allocated_bytes\":"<<allocations.bytes()<<",\"coefficient_challenge_endpoint_digest\":\""<<hex(r1::hash(digest.b))<<"\",\"synthetic_public_U_only\":true,\"sumcheck_round_seconds\":[";for(unsigned j=0;j<rounds.size();++j){if(j)std::cout<<',';std::cout<<rounds[j];}std::cout<<"]}"<<std::endl;if(state)state->report();
}
int opt3cInheritedMain(int argc,char**argv){try{r1::init();need(argc>=4,"benchmark fixture mode size/path");std::string fixture=argv[1],mode=argv[2];if(fixture=="native"){NativeMaps native(argv[3]);foldingBench(native.f,"Full12-public-mapping",0,mode);}else{auto n=std::stoull(argv[3]);Fixture f;f.setup(n,6,1);foldingBench(f,"six-root-public-map",n,mode);}return 0;}catch(const std::exception&e){std::cerr<<"OPT3C_BENCH_FAILED "<<e.what()<<'\n';return 1;}}

void uBench(const std::string&mode,std::size_t n,bool native,unsigned threads){
 std::vector<Fr>data(n);for(std::size_t j=0;j<n;++j)data[j]=Fr(int(j%71)-35);
 std::vector<ui::Block>blocks;if(native){auto layout=makeLayout(master3::trustedContext(12));need(layout.roots.size()==45,"OPT3D real Full12 root count");for(auto&r:layout.roots){need(r.padded<=n,"OPT3D fixture storage bound");blocks.push_back({r.model?1u:r.id.rfind("C_aux",0)==0?2u:3u,r.padded});}}else blocks={{1,n},{2,n},{3,n}};
 auto p=ui::pack(blocks,native?32768:4);const auto domain=std::size_t(1)<<32;const auto session="opt3d-public-bench";
 auto build=[&](std::size_t k){auto size=p.segments[k].padded;std::vector<Fr>v(size);for(std::size_t j=0;j<size;++j)v[j]=k?Fr(int((j*7+k)%79)-39):Fr(0);return v;};
 ui::PublicK state(p.segments,build,{512ull<<20,4ull<<30},session,domain);
 ui::Factors factors=[&](std::size_t k,const ui::Consume&c){state.withRoot(k,session,[&](const Value&b){c([&](std::size_t j){return data.at(j);},b);});};
 ui::Inputs inputs=[&](std::size_t k){return ui::InputSpan{data.data(),p.segments[k].padded,p.segments[k].padded};};
 auto start=std::chrono::steady_clock::now();auto cpu=processCPU();Writer digest;std::vector<Fr>point;ui::Round last;ui::UMetrics sum;std::vector<double>rounds;
 for(unsigned t=0;t<32;++t){auto begin=std::chrono::steady_clock::now();ui::UMetrics m;crypto::AllocationTrace round_allocations;
  last=mode=="reference"?ui::quadraticRoundFolded(domain,p.segments,point,factors):ui::quadraticRoundStreaming(domain,p.segments,point,factors,mode=="getter"?ui::Inputs{}:inputs,{threads,1024,mode!="unfactored",{}},&m);
  m.allocations=round_allocations.count();m.allocated_bytes=round_allocations.bytes();for(auto&v:last.coefficients)digest.scalar(v);Fr r=-3-int(t);state.advance(session,r);point.push_back(r);digest.scalar(r);rounds.push_back(std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count());
  sum.fused_load_bytes+=m.fused_load_bytes;sum.product_multiplications+=m.product_multiplications;sum.product_additions+=m.product_additions;sum.allocations+=m.allocations;sum.allocated_bytes+=m.allocated_bytes;sum.coordinates+=m.coordinates;sum.bytes+=m.bytes;sum.mapping_calls+=m.mapping_calls;sum.multiplications+=m.multiplications;sum.additions+=m.additions;sum.scan_cpu+=m.scan_cpu;sum.scan_wall+=m.scan_wall;sum.product_wall+=m.product_wall;sum.extra_bytes=std::max(sum.extra_bytes,m.extra_bytes);sum.tile_bytes=std::max(sum.tile_bytes,m.tile_bytes);sum.thread_bytes=std::max(sum.thread_bytes,m.thread_bytes);
 }
 auto r=point.back();auto k=state.endpoint(session,point);need(k==last.kernel_pair[0]+r*(last.kernel_pair[1]-last.kernel_pair[0]),"OPT3D benchmark endpoint");digest.scalar(k);digest.scalar(last.input_pair[0]+r*(last.input_pair[1]-last.input_pair[0]));
 struct rusage usage{};getrusage(RUSAGE_SELF,&usage);std::uint64_t count=0;for(auto s:p.segments)count+=s.padded;count*=32;
 std::cout<<std::setprecision(14)<<"OPT3D_BENCH {\"mode\":\""<<mode<<"\",\"n\":"<<n<<",\"native_shapes\":"<<(native?"true":"false")<<",\"threads\":"<<threads<<",\"roots\":"<<blocks.size()<<",\"rounds\":32,\"U_scan_coordinates\":"<<count<<",\"U_bytes_read\":"<<count*sizeof(Fr)<<",\"U_address_mapping_calls\":"<<sum.mapping_calls<<",\"U_address_mapping_cpu_seconds\":0,\"U_K_fused_load_bytes\":"<<(mode=="reference"?"null":std::to_string(sum.fused_load_bytes))<<",\"main_thread_cpp_allocations_including_K_provider\":"<<sum.allocations<<",\"main_thread_cpp_allocated_bytes_including_K_provider\":"<<sum.allocated_bytes<<",\"coefficient_Fr_multiplications\":"<<(mode=="reference"?"null":std::to_string(sum.product_multiplications))<<",\"coefficient_Fr_additions\":"<<(mode=="reference"?"null":std::to_string(sum.product_additions))<<",\"U_scan_cpu_seconds\":"<<(mode=="reference"?"null":std::to_string(sum.scan_cpu))<<",\"U_scan_wall_seconds\":"<<(mode=="reference"?"null":std::to_string(sum.scan_wall))<<",\"U_K_product_wall_seconds\":"<<(mode=="reference"?"null":std::to_string(sum.product_wall))<<",\"U_TILE_RAM_PEAK_BYTES\":"<<(mode=="reference"?"null":std::to_string(sum.tile_bytes))<<",\"U_THREAD_LOCAL_BYTES\":"<<(mode=="reference"?"null":std::to_string(sum.thread_bytes))<<",\"U_TOTAL_EXTRA_RAM_BYTES\":"<<(mode=="reference"?"null":std::to_string(sum.extra_bytes))<<",\"U_Fr_multiplications\":"<<(mode=="reference"?count/32*62:sum.multiplications)<<",\"U_Fr_additions\":"<<(mode=="reference"?count/32*31:sum.additions)<<",\"unified_sumcheck_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<",\"cpu_seconds\":"<<processCPU()-cpu<<",\"prover_peak_rss_kib\":"<<usage.ru_maxrss<<",\"coefficient_challenge_endpoint_digest\":\""<<hex(r1::hash(digest.b))<<"\",\"public_fixture_only\":true,\"metric_scope\":\"Fr counts are U contraction only; reference scan subphases unavailable, not zero; bytes are Fr payload loads, not hardware bus traffic\",\"sumcheck_round_seconds\":[";for(unsigned t=0;t<32;++t){if(t)std::cout<<',';std::cout<<rounds[t];}std::cout<<"]}\n";state.report();
}
int main(int argc,char**argv){try{r1::init();need(argc==5,"OPT3D bench mode n native threads");uBench(argv[1],std::stoull(argv[2]),std::string(argv[3])=="native",unsigned(std::stoul(argv[4])));return 0;}catch(const std::exception&e){std::cerr<<"OPT3D_BENCH_FAILED "<<e.what()<<'\n';return 1;}}
