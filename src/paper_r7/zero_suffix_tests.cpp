#include "unified_input.hpp"
#include <iostream>
#include <chrono>
#include <sys/resource.h>
using namespace zkgpt::r7::crypto;
namespace ui=zkgpt::r7::oneblock::unified;
void benchmark(int argc,char**argv){
 need(argc==7,"bench n stored threads elide repeats");auto n=std::stoull(argv[2]),stored=std::stoull(argv[3]);auto threads=std::stoul(argv[4]);bool elide=std::string(argv[5])=="1";auto repeats=std::stoul(argv[6]);
 need(n>=32 && n<=(1u<<20) && stored<=n && repeats>0 && repeats<=16,"bounded benchmark");
 std::vector<Fr>data(stored);for(std::size_t j=0;j<stored;++j)data[j]=Fr(int(j%71)-35);
 std::vector<ui::Segment>s{{0,n}};ui::Factors f=[&](std::size_t,const ui::Consume&c){c([&](std::size_t j){return j<stored?data[j]:Fr(0);},[](std::size_t j){return Fr(int(j%19)-9);});};
 ui::Inputs inputs=[&](std::size_t){return ui::InputSpan{data.data(),stored,n};};
 std::vector<Fr>q;auto bits=r1::log2exact(n);for(unsigned j=0;j<bits;++j)q.push_back(Fr(-3-int(j)));
 std::uint64_t covered=0,contracted=0,zeros=0,mul=0,add=0,allocation=0,copies=0;std::size_t extra=0;Writer digest;
 auto start=std::chrono::steady_clock::now();double scan=0;
 for(unsigned rep=0;rep<repeats;++rep)for(unsigned t:{0u,4u,bits-1}){ui::UMetrics m;ui::UOptions o{unsigned(threads),1024,true,{}};o.elide_zero_suffix=elide;
  auto got=ui::quadraticRoundStreaming(n,s,{q.begin(),q.begin()+t},f,inputs,o,&m);for(auto v:got.coefficients)digest.scalar(v);for(auto v:got.input_pair)digest.scalar(v);for(auto v:got.kernel_pair)digest.scalar(v);
  covered+=m.coordinates;contracted+=m.contracted_coordinates;zeros+=m.implicit_zero_coordinates;mul+=m.multiplications;add+=m.additions;allocation+=m.allocations;copies+=m.allocated_bytes;extra=std::max<std::size_t>(extra,m.extra_bytes);scan+=m.scan_wall;
 }
 struct rusage usage{};getrusage(RUSAGE_SELF,&usage);
 std::cout.precision(12);std::cout<<"BENCH {\"n\":"<<n<<",\"stored\":"<<stored<<",\"threads\":"<<threads<<",\"elide\":"<<(elide?"true":"false")<<",\"repeats\":"<<repeats<<",\"seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<",\"scan_seconds\":"<<scan<<",\"covered\":"<<covered<<",\"contracted\":"<<contracted<<",\"implicit_zero\":"<<zeros<<",\"Fr_mul\":"<<mul<<",\"Fr_add\":"<<add<<",\"main_thread_allocations\":"<<allocation<<",\"main_thread_allocated_bytes\":"<<copies<<",\"scratch_bound\":"<<extra<<",\"peak_rss_kib\":"<<usage.ru_maxrss<<",\"digest\":\""<<hex(r1::hash(digest.b))<<"\"}\n";
}
void equivalence(){
 const std::size_t n=1u<<18;std::vector<ui::Segment>s{{0,n}};
 for(auto stored:{0u,1u,17u,(1u<<17)+13,(1u<<18)-1,1u<<18})for(unsigned threads:{1u,2u,4u})for(unsigned variant=0;variant<3;++variant){
  std::vector<Fr>data(stored);for(std::size_t j=0;j<stored;++j)data[j]=Fr(int(j%13)-6);
  ui::Factors f=[&](std::size_t,const ui::Consume&c){c([&](std::size_t j){return j<stored?data[j]:Fr(0);},[](std::size_t j){return Fr(j%7+1);});};
  ui::Inputs inputs=[&](std::size_t){return ui::InputSpan{stored?data.data():nullptr,stored,n};};
  for(unsigned t:{0u,4u,16u,18u,19u}){std::vector<Fr>q(t,variant==0?Fr(0):variant==1?Fr(1):Fr(-3));
   auto ref=ui::quadraticRoundFolded(1u<<20,s,q,f);ui::UMetrics m;
   ui::UOptions o{threads,1024,variant!=2,{}};
   o.elide_zero_suffix=true;
   o.observe=[&](std::size_t,std::size_t first,const Fr*v,std::size_t count){auto width=std::size_t(1)<<std::min(t,18u);auto weights=r1::weights({q.begin(),q.begin()+std::min(t,18u)});for(std::size_t j=0;j<count;++j){Fr expected=0;for(std::size_t z=0;z<width;++z){auto at=(first+j)*width+z;if(at<stored)expected+=weights[z]*data[at];}need(v[j]==expected,"zero suffix coordinate contraction mismatch");}};
   auto got=ui::quadraticRoundStreaming(1u<<20,s,q,f,inputs,o,&m);
   need(got.coefficients==ref.coefficients && got.input_pair==ref.input_pair && got.kernel_pair==ref.kernel_pair,"zero suffix endpoint/coefficient mismatch");
   need(m.contracted_coordinates==stored && m.implicit_zero_coordinates==n-stored && m.extra_bytes<=8ull<<20,"zero suffix work/memory counters");
  }
 }
 std::cout<<"R7_ZERO_SUFFIX_BOUNDARIES_CHALLENGES_THREADS_COEFFICIENTS_TESTED\n";
}
int main(int argc,char**argv){try{
 r1::init();if(argc>1 && std::string(argv[1])=="bench"){benchmark(argc,argv);return 0;}
 const std::size_t n=1u<<18;std::vector<ui::Segment>s{{0,n}};
 std::vector<Fr>data(17,Fr(3));std::vector<Fr>q(4,Fr(2));ui::UMetrics m;
 ui::Factors f=[&](std::size_t,const ui::Consume&c){c([&](std::size_t j){return j<data.size()?data[j]:Fr(0);},[](std::size_t){return Fr(7);});};
 ui::UOptions experimental{1,1024,true,{}};experimental.elide_zero_suffix=true;
 auto got=ui::quadraticRoundStreaming(n,s,q,f,[&](std::size_t){return ui::InputSpan{data.data(),data.size(),n};},experimental,&m);
 auto reference=ui::quadraticRoundFolded(n,s,q,f);
 need(got.coefficients==reference.coefficients,"zero suffix changed polynomial");
 need(m.multiplications<=data.size(),"R7 implicit source zero suffix still performs field multiplications");
 std::cout<<"R7_ZERO_SUFFIX_ARITHMETIC_BUDGET_TESTED\n";equivalence();return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
