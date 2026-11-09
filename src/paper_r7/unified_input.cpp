#include "u_fold_state.hpp"
#include "unified_input.hpp"
#include "public_k.hpp"
#include <algorithm>
#include <map>
#include <chrono>
#include <iostream>
#include "opt2_metrics.hpp"
namespace zkgpt::r7::oneblock::unified {
namespace {
constexpr std::size_t maxDomain=std::size_t(1)<<32;
bool power(std::size_t n){return n && !(n&(n-1));}
// The virtual domain is never materialized. Do not apply r1::log2exact's
// separate 2^28 allocation bound to its metadata-only round count.
unsigned domainBits(std::size_t n){need(power(n) && n<=maxDomain,"unified virtual domain dimension bound");unsigned bits=0;while(n>1){n>>=1;++bits;}return bits;}
class SplitWeights {
 std::vector<Fr>low_,high_;unsigned split_;
 public:explicit SplitWeights(const std::vector<Fr>&q):split_(q.size()/2){low_=r1::weights({q.begin(),q.begin()+split_});high_=r1::weights({q.begin()+split_,q.end()});}
 Fr at(std::size_t j)const{return low_[j&(low_.size()-1)]*high_[j>>split_];}
};
Fr fixedBits(const Segment&s,const std::vector<Fr>&point,unsigned begin){Fr weight=1;for(unsigned j=begin;j<point.size();++j)weight*=((s.offset>>j)&1)?point[j]:Fr(1)-point[j];return weight;}
}
void validate(std::size_t n,const std::vector<Segment>&segments){
 need(power(n) && n<=maxDomain && !segments.empty() && segments.size()<=8193,"unified input domain/segment bound");
 auto sorted=segments;std::sort(sorted.begin(),sorted.end(),[](auto a,auto b){return a.offset<b.offset;});std::size_t end=0;
 for(auto s:sorted){need(power(s.padded) && s.padded<=n && s.offset<=n-s.padded && s.offset%s.padded==0 && s.offset>=end,"unified unaligned/overlapping/out-of-domain segment");end=s.offset+s.padded;}
}
Packing pack(const std::vector<Block>&blocks,std::size_t public_padded){
 need(power(public_padded) && public_padded<=maxDomain && blocks.size()<=8192,"unified public input packing bound");
 Packing out;out.regions[0]={public_padded,public_padded,public_padded,0};out.segments.push_back({0,public_padded});out.categories.push_back(0);
 auto align=[](std::size_t at,std::size_t alignment){auto rest=at%alignment;if(!rest)return at;auto gap=alignment-rest;need(at<=maxDomain-gap,"unified alignment overflow");return at+gap;};
 for(auto b:blocks){need(b.category>=1 && b.category<=3 && power(b.padded) && b.padded<=maxDomain,"unified private category/block bound");auto&r=out.regions[b.category];auto begin=align(r.used,b.padded);need(begin<=maxDomain-b.padded,"unified local region bound");out.segments.push_back({begin,b.padded});out.categories.push_back(b.category);r.used=begin+b.padded;r.alignment=std::max(r.alignment,b.padded);}
 std::array<std::size_t,4>used{};for(unsigned k=0;k<4;++k)used[k]=out.regions[k].used;auto capacities=independentCapacities(used);std::size_t end=0;
 for(unsigned k=0;k<4;++k){auto&r=out.regions[k];r.capacity=capacities[k];r.base=r.capacity?align(end,r.alignment):end;need(r.base<=maxDomain-r.capacity,"unified global region bound");end=r.base+r.capacity;}
 for(std::size_t k=0;k<out.segments.size();++k)out.segments[k].offset+=out.regions[out.categories[k]].base;
 out.domain=1;while(out.domain<end)out.domain*=2;
 validate(out.domain,out.segments);return out;
}
std::array<std::size_t,4>independentCapacities(const std::array<std::size_t,4>&used){std::array<std::size_t,4>out{};for(unsigned k=0;k<4;++k){need(used[k]<=maxDomain,"unified region capacity overflow");if(used[k]){out[k]=1;while(out[k]<used[k])out[k]*=2;}}return out;}
void validate(const Packing&p){
 need(!p.segments.empty() && p.categories.size()==p.segments.size() && p.categories[0]==0,"unified packing categories");std::vector<Block>blocks;for(std::size_t k=1;k<p.segments.size();++k)blocks.push_back({p.categories[k],p.segments[k].padded});auto expected=pack(blocks,p.segments[0].padded);
 need(p.domain==expected.domain,"unified nonminimal/noncanonical logical domain");
 for(unsigned k=0;k<4;++k){const auto&a=p.regions[k];const auto&b=expected.regions[k];need(a.used==b.used && a.capacity==b.capacity && a.alignment==b.alignment && a.base==b.base,"unified noncanonical independent region metadata");}
 for(std::size_t k=0;k<p.segments.size();++k){need(p.segments[k].offset==expected.segments[k].offset,"unified noncanonical segment offset");}
 validate(p.domain,p.segments);
}
Bytes descriptor(const Packing&p){validate(p);Writer w;w.text(suite);w.number(p.domain,8);for(const auto&r:p.regions)for(auto n:{r.used,r.capacity,r.alignment,r.base})w.number(n,8);w.number(p.segments.size());for(std::size_t k=0;k<p.segments.size();++k){w.number(p.categories[k]);w.number(p.segments[k].offset,8);w.number(p.segments[k].padded,8);}return w.b;}
void writeLayout(std::ostream&out,const Packing&p){validate(p);out<<"{\"suite\":\""<<suite<<"\",\"logical_domain\":"<<p.domain<<",\"logical_rounds\":"<<domainBits(p.domain)<<",\"regions\":[";std::size_t gaps=0,end=0,segment_gap=0;for(unsigned k=0;k<4;++k){if(k)out<<',';const auto&r=p.regions[k];out<<"{\"category\":"<<k<<",\"used\":"<<r.used<<",\"capacity\":"<<r.capacity<<",\"alignment\":"<<r.alignment<<",\"base\":"<<r.base<<'}';gaps+=r.base-end;end=r.base+r.capacity;std::size_t lengths=0;for(std::size_t j=0;j<p.segments.size();++j)if(p.categories[j]==k)lengths+=p.segments[j].padded;segment_gap+=r.used-lengths;}
 out<<"],\"region_gap_elements\":"<<gaps<<",\"segment_alignment_gap_elements\":"<<segment_gap<<",\"global_tail_elements\":"<<p.domain-end<<",\"segments\":[";for(std::size_t k=0;k<p.segments.size();++k){if(k)out<<',';out<<"{\"source\":"<<k<<",\"category\":"<<p.categories[k]<<",\"offset\":"<<p.segments[k].offset<<",\"padded\":"<<p.segments[k].padded<<'}';}out<<"]}";
}
Fr selector(const Segment&s,const std::vector<Fr>&point){need(power(s.padded) && point.size()>=r1::log2exact(s.padded) && point.size()<=32,"unified selector dimensions");return fixedBits(s,point,r1::log2exact(s.padded));}
static Round quadraticRoundImpl(std::size_t n,const std::vector<Segment>&segments,const std::vector<Fr>&prefix,const Factors&provider,bool folded_kernel){
 validate(n,segments);need(prefix.size()<domainBits(n) && provider,"unified round prefix/provider");const auto t=unsigned(prefix.size());Round out;
 struct Pair{Fr a=0,b=0;};std::map<std::size_t,Pair>collapsed;
 auto product=[&](const Fr&a0,const Fr&a1,const Fr&b0,const Fr&b1){auto da=a1-a0,db=b1-b0;out.coefficients[0]+=a0*b0;out.coefficients[1]+=a0*db+da*b0;out.coefficients[2]+=da*db;if((n>>t)==2){out.input_pair[0]+=a0;out.input_pair[1]+=a1;out.kernel_pair[0]+=b0;out.kernel_pair[1]+=b1;}};
 for(std::size_t k=0;k<segments.size();++k){auto s=segments[k];auto d=r1::log2exact(s.padded);auto low=std::min<unsigned>(t,d);SplitWeights weights({prefix.begin(),prefix.begin()+low});const auto block=std::size_t(1)<<low;unsigned calls=0;
  provider(k,[&](const Value&a,const Value&b){++calls;need(calls==1 && a && b,"unified factor oracle invocation");
   auto folded=[&](std::size_t j){Pair v;for(std::size_t i=0;i<block;++i){auto index=j*block+i;if(!low){v.a=a(index);if(!folded_kernel)v.b=b(index);}else{auto w=weights.at(i);v.a+=w*a(index);if(!folded_kernel)v.b+=w*b(index);}}if(folded_kernel)v.b=b(j);return v;};
   if(t<d){for(std::size_t j=0;j<(s.padded>>t)/2;++j){auto left=folded(2*j),right=folded(2*j+1);product(left.a,right.a,left.b,right.b);}}
   else{auto v=folded(0);auto w=fixedBits(s,prefix,d);auto&target=collapsed[s.offset>>t];target.a+=w*v.a;target.b+=w*v.b;}
  });need(calls==1,"unified missing factor oracle");
 }
 // Large aligned blocks occupy complete suffix pairs; scalar blocks can
 // collide, so combine both factors first (including X with a zero K block).
 for(auto it=collapsed.begin();it!=collapsed.end();){auto pair=it->first>>1;Pair left,right;do{if(it->first&1)right=it->second;else left=it->second;++it;}while(it!=collapsed.end() && (it->first>>1)==pair);product(left.a,right.a,left.b,right.b);}
 return out;
}
Round quadraticRound(std::size_t n,const std::vector<Segment>&s,const std::vector<Fr>&p,const Factors&f){return quadraticRoundImpl(n,s,p,f,false);}
Round quadraticRoundFolded(std::size_t n,const std::vector<Segment>&s,const std::vector<Fr>&p,const Factors&f){return quadraticRoundImpl(n,s,p,f,true);}
Reduction reduce(IO&io,std::size_t n,const std::vector<Segment>&segments,const Hidden&initial,
 const Factors&factors,const std::function<Fr(const std::vector<Fr>&)>&kernel_at,Random*rng,const std::string&role,PublicK*state,const Inputs&inputs){
 auto started=std::chrono::steady_clock::now();auto cpu=crypto::processCPU();
 validate(n,segments);need(kernel_at && (io.prover()?bool(factors)&&rng:!factors&&!rng),"unified private prover/verifier boundary");
 need(io.prover() || !state,"unified verifier cannot use prover K cache");
 need(io.prover() || !inputs,"OPT3D verifier cannot access sealed U storage");
 crypto::validate(initial.handle,io.session(),initial.handle.role);
 Writer statement;statement.text(suite);statement.number(n,8);statement.number(2);statement.raw(encode(initial.handle));statement.number(segments.size());for(auto s:segments){statement.number(s.offset,8);statement.number(s.padded,8);}bindPublic(io,role+"/statement",statement.b);
 auto previous=initial;std::vector<Fr>point;Round last;
 bool contiguous_inputs=true;if(io.prover() && inputs)for(std::size_t k=0;k<segments.size();++k){auto s=inputs(k);if(s.stored && !s.data)contiguous_inputs=false;}
 bool category_cache=true;if(const auto*v=std::getenv("ZKGPT_CATEGORY_U_FOLD")){std::string text(v);need(text=="0" || text=="1","category U cache switch");category_cache=text=="1";}
 std::uint64_t category_budget=512ull<<20;if(const auto*v=std::getenv("ZKGPT_CATEGORY_U_RAM_BYTES")){std::string text(v);need(!text.empty() && text.find_first_not_of("0123456789")==std::string::npos,"category U budget syntax");category_budget=std::stoull(text);need(category_budget<=512ull<<20,"category U budget cap");}
 std::unique_ptr<FoldedU>folded_u;
 if(io.prover() && state && inputs && ((contiguous_inputs && FoldedU::requested()) || (!contiguous_inputs && category_cache))){folded_u=std::make_unique<FoldedU>(n,segments,inputs,io.session(),contiguous_inputs?FoldedU::environmentBudget():category_budget,!contiguous_inputs);}
 auto retained=folded_u && folded_u->enabled()?folded_u.get():nullptr;
 for(unsigned t=0;t<domainBits(n);++t){auto round_started=std::chrono::steady_clock::now();auto name=role+"/round/"+std::to_string(t);std::uint64_t round_u_coordinates=0;if(io.prover()){
  if(state && inputs){UMetrics m;last=quadraticRoundStreaming(n,segments,point,factors,inputs,UOptions::environment(),&m,retained,io.session());round_u_coordinates=m.coordinates;
   if(crypto::opt2MetricsEnabled())std::cerr<<"U_SCAN_DETAIL "<<scanMetricsJSON(t,m)<<"\n";
   if(crypto::opt2MetricsEnabled())std::cerr<<"OPT3D_U_ROUND {\"round\":"<<t<<",\"U_scan_coordinates_including_public_X\":"<<m.coordinates<<",\"U_bytes_read\":"<<m.bytes<<",\"U_K_fused_load_bytes\":"<<m.fused_load_bytes<<",\"U_scan_cpu_seconds\":"<<m.scan_cpu<<",\"U_scan_wall_seconds\":"<<m.scan_wall<<",\"U_K_product_wall_seconds\":"<<m.product_wall<<",\"U_Fr_multiplications\":"<<m.multiplications<<",\"U_Fr_additions\":"<<m.additions<<",\"coefficient_Fr_multiplications\":"<<m.product_multiplications<<",\"coefficient_Fr_additions\":"<<m.product_additions<<",\"U_TILE_RAM_PEAK_BYTES\":"<<m.tile_bytes<<",\"U_THREAD_LOCAL_BYTES\":"<<m.thread_bytes<<",\"U_TOTAL_EXTRA_RAM_BYTES\":"<<m.extra_bytes<<",\"main_thread_cpp_allocations_including_K_provider\":"<<m.allocations<<",\"main_thread_cpp_allocated_bytes_including_K_provider\":"<<m.allocated_bytes<<"}\n";
  }else last=state?quadraticRoundFolded(n,segments,point,factors):quadraticRound(n,segments,point,factors);
 }
  std::vector<Hidden>h{previous};for(unsigned j=0;j<3;++j)h.push_back(exchangeHidden(io,name+"/c/"+std::to_string(j),last.coefficients[j],rng));
  Relations recurrence;recurrence.role=name+"/recurrence";for(auto&v:h)recurrence.handles.push_back(v.handle);recurrence.linear={{Fr(-1),Fr(2),Fr(1),Fr(1)}};sigma(io,recurrence,io.prover()?&h:nullptr,rng);
  auto r=io.coin(name+"/r");if(state)state->advance(io.session(),r);if(retained)retained->advance(io.session(),r,inputs);point.push_back(r);previous=combine(io.session(),name+"/derived",{h[1],h[2],h[3]},{Fr(1),r,r*r});bindPublic(io,name+"/derived",encode(previous.handle));
  if(crypto::opt2MetricsEnabled()){auto u=round_u_coordinates;std::cerr<<"OPT3C_ROUND {\"round\":"<<t<<",\"private_U_scan_coordinates\":"<<(io.prover()?u:0)<<",\"sumcheck_round_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-round_started).count()<<"}\n";}
 }
 Fr input=0,actual_kernel=0;if(io.prover()){
  if(n==1){factors(0,[&](const Value&a,const Value&b){input=a(0);actual_kernel=b(0);});}
  else{auto r=point.back();input=last.input_pair[0]+r*(last.input_pair[1]-last.input_pair[0]);actual_kernel=last.kernel_pair[0]+r*(last.kernel_pair[1]-last.kernel_pair[0]);}
 }
 auto expected=state?state->endpoint(io.session(),point):kernel_at(point);if(io.prover())need(expected==actual_kernel,"unified independently evaluated public kernel mismatch");
 auto input_end=exchangeHidden(io,role+"/input-evaluation",input,rng);auto public_kernel=publicHidden(io,role+"/public-kernel",expected);bindPublic(io,role+"/public-kernel",encode(public_kernel.handle));
 std::vector<Hidden>w{input_end,public_kernel,previous};Relations terminal;terminal.role=role+"/terminal";for(auto&h:w)terminal.handles.push_back(h.handle);terminal.products={{0,1,2}};sigma(io,terminal,io.prover()?&w:nullptr,rng);
 std::cerr<<"UNIFIED_SUMCHECK {\"domain\":"<<n<<",\"rounds\":"<<point.size()<<",\"wall_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<",\"cpu_seconds\":"<<crypto::processCPU()-cpu<<",\"includes_public_kernel_endpoint\":true}\n";
 if(folded_u)folded_u->report();
 return {initial,{input_end,public_kernel},point};
}
}
