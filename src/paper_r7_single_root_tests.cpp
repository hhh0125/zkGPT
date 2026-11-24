#include "paper_r7_classified_commitments.hpp"
#include "paper_r7_category_tiles.hpp"
#include "paper_r7_fs_io.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <type_traits>
using namespace zkgpt::r7;
using namespace crypto;
namespace cl=oneblock::classified;
namespace sh=oneblock::single;
namespace zkgpt::r7::oneblock::classified {
struct AuthenticationTestAccess {
 static const std::vector<Root>&roots(const Commitments&c){return c.roots_;}
 static const single::Secret&secret(const Commitments&c){return *c.single_secret_;}
};
}
namespace {
template<class F>void rejects(F f,const char*reason){try{f();}catch(const std::exception&e){need(std::string(e.what()).find(reason)!=std::string::npos,e.what());return;}throw std::runtime_error("expected rejection");}
// Reproducible test randomness only. Production continues to use OSRandom.
class FixedRandom:public Random {
 std::uint64_t state=17;
 public:void fill(std::uint8_t*out,std::size_t n)override{for(std::size_t i=0;i<n;++i){state^=state<<13;state^=state>>7;state^=state<<17;out[i]=std::uint8_t(state);}}
};
class LocalIO:public IO {
 public:bool prover()const override{return true;}std::string session()const override{return std::string(64,'b');}Digest prefix()const override{return {};}
 Bytes message(const std::string&,const Bytes&b={})override{return b;}Fr coin(const std::string&)override{return Fr(2);}
};
R7Context context(std::size_t n){
 R7Context c;c.layout_sha256=std::string(64,'a');c.plan.master.actual=4*n;
 c.plan.sources.resize(2);c.plan.sources[0].spec.actual=n;c.plan.sources[0].spans={{2*n,n,Fr(0)}};
 // Deliberately unsorted W spans preserve W's local source order.
 c.plan.sources[1].spec.actual=n;c.plan.sources[1].spans={{n+n/2,n-n/2,Fr(0)},{n,n/2,Fr(0)}};
 c.objects.push_back({"helper/a","",ObjectKind::ExpHelper,3,4,{},true});
 c.objects.push_back({"helper/b","",ObjectKind::ExpHelper,n,n,{},true});
 c.objects.push_back({"mapped","",ObjectKind::MappedChild,6,8,{{n-2,6,Fr(5)}},false});
 return c;
}
void layout(std::size_t n){
 auto c=context(n);auto p=cl::categoryLayout(c,{{0,n,Fr(0)}});
 need(p.roots.size()==5 && p.base.empty(),"one root for every nonempty category");
 std::array<std::size_t,5>ids{};std::array<bool,5>seen{};
 for(std::size_t k=0;k<p.roots.size();++k){auto&r=p.roots[k];need(r.category<5 && !seen[r.category],"unique category root");seen[r.category]=true;ids[r.category]=k;need(r.category_offset==0 && r.actual==r.padded,"root actual storage without padding");need(r.actual==(r.category==4?n+4:n),"root complete category length");}
 auto check=[&](std::size_t at,unsigned category,std::size_t local){need(cl::baseCoordinate(p,at)==cl::Coordinate{std::uint32_t(ids[category]),std::uint32_t(local)},"base category coordinate");};
 for(std::size_t j:{std::size_t(0),n/2-1,n/2,n-1}){
  check(j,0,j);auto wlocal=j<n/2?n-n/2+j:j-n/2;check(n+j,1,wlocal);check(2*n+j,3,j);check(3*n+j,2,j);
 }
 for(const auto&run:p.base_runs){need(run.start.root<p.roots.size() && run.count<=p.roots[run.start.root].actual-run.start.index,"base run bounds");}
 auto&helper=p.views.at("helper/b");need(helper.pieces.size()==1 && helper.start==cl::Coordinate{std::uint32_t(ids[4]),4},"helper stays contiguous past former cap");
 for(std::size_t j:{std::size_t(0),n-1})need(cl::locate(p,helper,j).coordinate==cl::Coordinate{std::uint32_t(ids[4]),std::uint32_t(4+j)},"helper offset and final index");
 auto&mapped=p.views.at("mapped");for(std::size_t j=0;j<6;++j){auto at=cl::locate(p,mapped,j);need(!at.constant && at.bias==Fr(5) && at.coordinate==cl::baseCoordinate(p,n-2+j),"mapped category boundary/bias");}
 need(cl::locate(p,mapped,6).constant && cl::locate(p,mapped,7).constant,"mapped virtual padding");
 need(p.categories[0].padded>=n && p.categories[4].padded>=n+4,"logical category padding");
 std::vector<std::size_t>covered(p.roots.size(),0);auto tiles=cl::categoryTiles(p);
 for(const auto&t:tiles){need(t.begin==covered[t.root] && t.segment.padded<=(1u<<20) && t.segment.offset==p.categories[p.roots[t.root].category].logical_offset+t.begin,"computation tile bounds and offsets");covered[t.root]+=t.segment.padded;}
 for(std::size_t k=0;k<p.roots.size();++k)need(covered[k]==p.roots[k].actual,"tiles cover full roots beyond old cap");
 auto id=cl::identity(p);need(id==cl::identity(cl::categoryLayout(c,{{0,n,Fr(0)}})),"deterministic layout identity");
 p.roots[ids[4]].category_offset=1;need(cl::identity(p)!=id,"root mapping bound in identity");
 rejects([&]{cl::baseCoordinate(p,4*n);},"base bounds");
 rejects([&]{cl::offsetCoordinate(p,{std::uint32_t(ids[0]),0},n);},"coordinate overflow/bounds");
 std::cout<<"PASS category n="<<n<<" roots=5 runs="<<p.base_runs.size()<<'\n';
}
void bounds(){
 static_assert(std::is_same_v<decltype(cl::Coordinate{}.index),std::uint32_t>);
 auto c=context(8);c.objects[1].actual=c.objects[1].padded=std::uint64_t(1)<<32;
 // helper/a's four cells plus helper/b would exceed uint32_t coordinates.
 rejects([&]{cl::categoryLayout(c,{{0,8,Fr(0)}});},"root uint32 index overflow");
 c.objects[1].actual=c.objects[1].padded=std::numeric_limits<std::uint64_t>::max();
 rejects([&]{cl::categoryLayout(c,{{0,8,Fr(0)}});},"root uint32 index overflow");
 cl::Layout p;p.roots.push_back({"boundary",std::size_t(1)<<32,std::size_t(1)<<32});
 need(cl::offsetCoordinate(p,{0,UINT32_MAX},0).index==UINT32_MAX,"last uint32 index is valid");
 rejects([&]{cl::offsetCoordinate(p,{0,UINT32_MAX},1);},"coordinate overflow/bounds");
 p.roots[0].actual+=1;rejects([&]{cl::offsetCoordinate(p,{0,UINT32_MAX},1);},"coordinate overflow/bounds");
 auto empty=context(8);empty.objects.clear();empty.plan.sources.erase(empty.plan.sources.begin());empty.plan.master.actual=16;
 auto l=cl::categoryLayout(empty,{{0,8,Fr(0)}});need(l.roots.size()==2 && !l.categories[2].actual && !l.categories[3].actual && !l.categories[4].actual,"empty categories have no roots");
}
void borrowed(){
 auto c=context(8);auto p=cl::categoryLayout(c,{{0,8,Fr(0)}});cl::Prepared prepared;
 for(auto name:{"helper/a","helper/b"}){auto&v=p.views.at(name);auto data=std::make_shared<std::vector<Fr>>();for(std::size_t j=0;j<v.padded;++j)data->push_back(Fr(100+j));prepared[name]=data;}
 LocalIO io;FixedRandom rng;std::vector<Fr>x;for(unsigned j=0;j<8;++j)x.emplace_back(j+1);
 cl::Commitments commitments(io,p,[](std::size_t j){return Fr(j+1);},&prepared,&rng,x);
 need(prepared.empty(),"helper storage ownership transferred");auto&roots=cl::AuthenticationTestAccess::roots(commitments);
 for(std::size_t j=0;j<32;++j){auto at=cl::baseCoordinate(p,j);need(roots[at.root].value(at.index)==Fr(j+1),"borrowed base mapping");}
 for(auto name:{"helper/a","helper/b"}){auto&v=p.views.at(name);for(std::size_t j=0;j<v.padded;++j){auto at=cl::locate(p,v,j);need(roots[at.coordinate.root].value(at.coordinate.index)==Fr(100+j),"borrowed helper stored padding");}}
 auto&s=cl::AuthenticationTestAccess::secret(commitments);for(std::size_t k=0;k<s.pc.plan().segments.size();++k){auto&seg=s.pc.plan().segments[k];for(std::size_t j=seg.actual;j<seg.padded;++j)need(s.sources[k](j).isZero(),"category logical padding is zero");}
}
void protocol(fs::FsIO&io){
 auto c=context(8);auto p=cl::categoryLayout(c,{{0,8,Fr(0)}});cl::Prepared prepared;OSRandom rng;
 if(io.prover())for(auto name:{"helper/a","helper/b"}){auto data=std::make_shared<std::vector<Fr>>();for(std::size_t j=0;j<p.views.at(name).padded;++j)data->emplace_back(100+j);prepared[name]=data;}
 std::vector<Fr>x;for(unsigned j=0;j<8;++j)x.emplace_back(j+1);
 cl::Commitments commitments(io,p,io.prover()?oneblock::Value([](std::size_t j){return Fr(j+1);}):oneblock::Value{},io.prover()?&prepared:nullptr,io.prover()?&rng:nullptr,x);
 commitments.constrainBaseTails();
 for(auto name:{"helper/a","helper/b","mapped"}){auto v=commitments.view(name);std::vector<Fr>point(r1::log2exact(v.padded),Fr(2));auto weights=r1::weights(point);Fr value=0;
  if(io.prover())for(std::size_t j=0;j<v.padded;++j)value+=weights[j]*v.value(j);
  auto h=oneblock::exchangeHidden(io,std::string(name)+"/evaluation",value,io.prover()?&rng:nullptr);
  v.open(io,{point,h.handle},io.prover()?&h:nullptr,io.prover()?&rng:nullptr,std::string(name)+"/opening");
 }
 commitments.authenticateUnifiedInput(x);need(commitments.authenticated() && commitments.logicalCommitments()==1,"generated layout independent authentication");
}
void benchmark(unsigned workers){
 auto p=sh::makeCategoryPlan({3,0,(1u<<21)+1,0,0},std::string(64,'a'));
 FixedRandom rng;auto before=metrics();auto s=sh::commit(p,{[](std::size_t j){return Fr(j+1);},[](std::size_t j){Fr v;v.setHashOf(&j,sizeof(j));return v;}},rng,workers);
 auto after=metrics();Writer w;for(auto&point:s.pc.rows())w.point(point);
 std::cout<<"BENCH rows="<<p.rows<<" columns="<<p.columns<<" msm_calls="<<after.msm_calls-before.msm_calls<<" msm_terms="<<after.msm_terms-before.msm_terms<<" max_msm="<<after.max_msm<<" rows_sha256="<<hex(r1::hash(w.b))<<'\n';
}
void msm(){
 auto p=sh::makeCategoryPlan({3,0,(1u<<19)+1,0,0},std::string(64,'a'));need(p.columns==2048,"test crosses old 1024 MSM boundary");
 auto source=[](std::size_t j){if(j>=2048 && j<4096)return Fr(0);Fr v;v.setHashOf(&j,sizeof(j));return v;};
 std::vector<oneblock::Value>sources{[](std::size_t j){return Fr(j+1);},source};
 FixedRandom rng;auto before=metrics();metrics().max_msm=0;auto s=sh::commit(p,sources,rng,4);
 auto after=metrics();std::uint64_t touched=0;
 auto bases=hyrax_a3::generators(p.columns);auto original=bases;auto H=parameters(1).hiding;
 for(std::size_t i=0;i<p.rows;++i){std::vector<Fr>row(p.columns,Fr(0));bool active=false;auto begin=i*p.columns;
  for(auto&run:p.runs){auto lo=std::max(begin,run.physical_offset),hi=std::min(begin+p.columns,run.physical_offset+run.count);if(lo>=hi)continue;active=true;for(auto j=lo;j<hi;++j)row[j-begin]=sources[run.source](run.source_offset+j-run.physical_offset);}
  G1 old;old.clear();if(active){++touched;for(std::size_t j=0;j<p.columns;j+=1024){auto n=std::min<std::size_t>(1024,p.columns-j);std::vector<G1>scratch(bases.begin()+j,bases.begin()+j+n);G1 part;G1::mulVec(part,scratch.data(),row.data()+j,n);old+=part;}}
  need(s.pc.rows()[i]==old+H*s.blinds[i],"full row equals previous chunked commitment");
 }
 need(after.msm_calls-before.msm_calls==p.rows && after.msm_terms-before.msm_terms==p.rows*p.columns && after.max_msm==p.columns,"actual full-row MSM metrics");
 FixedRandom rng2;auto serial=sh::commit(p,sources,rng2,1);need(serial.blinds==s.blinds && serial.pc.rows()==s.pc.rows(),"parallel/serial fixed blind commitment equality");
 need(bases==original,"reference bases unchanged");
 // A real all-zero source row still commits to its independent blind.
 auto zero=sh::makeCategoryPlan({8,0,0,0,0},std::string(64,'a'));FixedRandom zr;auto z=sh::commit(zero,{[](std::size_t){return Fr(0);}},zr,2);for(std::size_t i=0;i<zero.rows;++i)need(z.pc.rows()[i]==H*z.blinds[i],"all zero row Pedersen semantics");
 rejects([&]{sh::commit(zero,{[](std::size_t)->Fr{throw std::runtime_error("source failure");}},zr,2);},"source failure");
 std::cout<<"PASS full-row MSM columns="<<p.columns<<" calls="<<p.rows<<" terms="<<p.rows*p.columns<<" previous_calls="<<touched*2<<" fixed-blind reference, zero rows, parallelism, exceptions\n";
}
}
int main(int argc,char**argv){try{r1::init();std::string mode=argc>1?argv[1]:"all";if(mode=="prove" || mode=="verify"){
 need(argc==3,"prove|verify PROOF");fs::Binding binding{"OneblockSingleHyrax","single-root-generated-layout",fs::generatorIdentity(),Bytes{1,2,3}};
 std::unique_ptr<fs::FsIO>io;if(mode=="prove")io=std::make_unique<fs::FsProverIO>(argv[2],binding);else io=std::make_unique<fs::FsVerifierIO>(argv[2],binding);
 protocol(*io);io->finish();std::cout<<io->metadataJson()<<'\n';return 0;
 }if(mode=="bench"){benchmark(argc>2?unsigned(std::stoul(argv[2])):4);return 0;}if(mode=="all" || mode=="layout"){layout(8);layout(1u<<24);layout((1u<<24)+3);bounds();borrowed();}if(mode=="all" || mode=="msm")msm();return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
