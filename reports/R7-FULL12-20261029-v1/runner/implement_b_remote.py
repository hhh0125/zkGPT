def edit(name,old,new):
 p=Path('src/paper_r7')/name;s=p.read_text();assert old in s,(name,old);p.write_text(s.replace(old,new))
edit('classified_layout.hpp','struct View {','struct Piece {std::size_t begin=0,count=0;Coordinate start;};\nstruct View {')
edit('classified_layout.hpp','Coordinate start;ObjectKind kind;','Coordinate start;ObjectKind kind;std::vector<Piece>pieces;')
edit('classified_layout.hpp','std::map<std::string,View>views;};','std::map<std::string,View>views;std::vector<Piece>base_runs;std::size_t master_actual=0;};\nLayout makePartitionedLayout(const R7Context&,std::size_t);\nstd::size_t baseSize(const Layout&);Coordinate baseCoordinate(const Layout&,std::size_t);\nstd::string partitionedIdentity(const Layout&);\nvoid writePartitionedLayout(std::ostream&,const Layout&);')
edit('classified_layout.hpp','void validate(const Layout&,const R7Context&);','void validate(const Layout&,const R7Context&,std::size_t slab_limit=1u<<24);')
edit('classified_layout.cpp','Layout makeLayout(const R7Context&c,std::size_t cap){','Layout makeLayout(const R7Context&c,std::size_t cap){\n if(c.plan.shape.layer_count==12)return makePartitionedLayout(c,cap);')
edit('classified_layout.cpp','Located locate(const Layout&p,const View&v,std::size_t j){need(j<v.padded,"classified view query bounds");','Located locate(const Layout&p,const View&v,std::size_t j){need(j<v.padded,"classified view query bounds");if(!v.mapped && !v.pieces.empty()){for(auto&part:v.pieces)if(j>=part.begin && j-part.begin<part.count)return {{part.start.root,std::uint32_t(part.start.index+j-part.begin)},Fr(0),false};throw std::runtime_error("partitioned helper map omission");}')
edit('classified_layout.cpp','s.offset+j<p.base.size(),"classified trusted map bounds");return {p.base[s.offset+j],s.bias,false};','s.offset+j<baseSize(p),"classified trusted map bounds");return {baseCoordinate(p,s.offset+j),s.bias,false};')
edit('classified_layout.cpp','std::string identity(const Layout&p){','std::string identity(const Layout&p){\n if(p.master_actual)return partitionedIdentity(p);')
edit('classified_layout.cpp','void validate(const Layout&p,const R7Context&c){auto expected=makeLayout(c);','void validate(const Layout&p,const R7Context&c,std::size_t cap){auto expected=makeLayout(c,cap);')
edit('classified_layout.cpp','void writeLayout(std::ostream&o,const Layout&p){','void writeLayout(std::ostream&o,const Layout&p){\n if(p.master_actual){writePartitionedLayout(o,p);return;}')
edit('partitioned_layout_tests.cpp','validate(p,c);','validate(p,c,8);')
edit('partitioned_layout_tests.cpp','validate(bad,c);','validate(bad,c,8);')
Path('src/paper_r7/partitioned_layout.cpp').write_text(r'''#include "classified_layout.hpp"
#include <algorithm>
#include <limits>
namespace zkgpt::r7::oneblock::classified {
std::size_t baseSize(const Layout&p){return p.master_actual?p.master_actual:p.base.size();}
Coordinate baseCoordinate(const Layout&p,std::size_t at){
 need(at<baseSize(p),"partitioned base bounds");if(!p.master_actual)return p.base.at(at);
 auto it=std::upper_bound(p.base_runs.begin(),p.base_runs.end(),at,[](std::size_t i,const Piece&r){return i<r.begin;});
 need(it!=p.base_runs.begin(),"partitioned base prefix omission");--it;need(at-it->begin<it->count,"partitioned base gap");
 return {it->start.root,std::uint32_t(it->start.index+at-it->begin)};
}
namespace {
struct Span {std::size_t begin,end,logical;};
void shape(std::size_t cap){need(cap && cap<=(1u<<24) && !(cap&(cap-1)),"partitioned physical root cap");}
void addRun(std::vector<Piece>&out,std::size_t begin,std::size_t count,Coordinate start){
 if(!out.empty()){auto&prev=out.back();if(prev.begin+prev.count==begin && prev.start.root==start.root && std::uint64_t(prev.start.index)+prev.count==start.index){prev.count+=count;return;}}
 out.push_back({begin,count,start});
}
void appendCoordinates(Layout&p,std::vector<Piece>&runs,std::size_t begin,std::size_t count,const std::string&category,bool registered,bool model,std::size_t cap,std::vector<std::size_t>&roots){
 while(count){if(roots.empty() || p.roots[roots.back()].actual==cap){need(p.roots.size()<std::numeric_limits<std::uint32_t>::max(),"partitioned root index overflow");roots.push_back(p.roots.size());p.roots.push_back({category+"/chunk/"+std::to_string(roots.size()-1),0,0,registered,model});}
 auto k=roots.back();auto&r=p.roots[k];auto n=std::min(count,cap-r.actual);addRun(runs,begin,n,{std::uint32_t(k),std::uint32_t(r.actual)});r.actual+=n;begin+=n;count-=n;
 }
}
void encodePiece(Writer&w,const Piece&r){w.number(r.begin,8);w.number(r.count,8);w.number(r.start.root);w.number(r.start.index);}
}
Layout makePartitionedLayout(const R7Context&c,std::size_t cap){
 shape(cap);need(c.plan.shape.layer_count==12 && !c.plan.sources.empty(),"partitioned Full12 native target required");
 Layout p;p.native=c.layout_sha256;p.master_actual=c.plan.master.actual;need(p.master_actual>0 && p.master_actual<=(1u<<28),"partitioned native master bounds");
 const auto&w=c.plan.sources.back();std::vector<Span>weights;std::set<std::size_t>cuts{0,p.master_actual};std::map<std::size_t,int>queryEvents;
 std::size_t logical=0;for(auto&s:w.spans){need(s.bias.isZero() && s.count && s.offset<p.master_actual && s.count<=p.master_actual-s.offset,"partitioned W span bounds/bias");weights.push_back({s.offset,s.offset+s.count,logical});logical+=s.count;cuts.insert(s.offset);cuts.insert(s.offset+s.count);}
 need(logical==w.spec.actual,"partitioned W complete source");std::sort(weights.begin(),weights.end(),[](const Span&a,const Span&b){return a.begin<b.begin;});
 for(std::size_t i=1;i<weights.size();++i)need(weights[i-1].end<=weights[i].begin,"partitioned W overlap");
 for(std::size_t k=0;k+1<c.plan.sources.size();++k){std::size_t actual=0;for(auto&s:c.plan.sources[k].spans){need(s.count && s.offset<p.master_actual && s.count<=p.master_actual-s.offset,"partitioned query span bounds");cuts.insert(s.offset);cuts.insert(s.offset+s.count);++queryEvents[s.offset];--queryEvents[s.offset+s.count];actual+=s.count;}need(actual==c.plan.sources[k].spec.actual,"partitioned query source omission");}
 // W roots follow original logical W order, even if native master spans do not.
 std::vector<std::size_t>wRoots,qRoots,aRoots,hRoots;std::vector<Piece>wRuns;
 appendCoordinates(p,wRuns,0,w.spec.actual,"C_W",true,true,cap,wRoots);
 auto wAt=[&](std::size_t i){auto it=std::upper_bound(wRuns.begin(),wRuns.end(),i,[](std::size_t j,const Piece&r){return j<r.begin;});need(it!=wRuns.begin(),"partitioned W logical prefix");--it;return std::pair<Coordinate,std::size_t>{{it->start.root,std::uint32_t(it->start.index+i-it->begin)},it->count-(i-it->begin)};};
 int active=0;std::size_t wi=0;for(auto it=cuts.begin();std::next(it)!=cuts.end();++it){auto begin=*it,end=*std::next(it);active+=queryEvents[begin];need(active>=0,"partitioned query interval balance");while(wi<weights.size() && weights[wi].end<=begin)++wi;
  if(wi<weights.size() && weights[wi].begin<=begin && begin<weights[wi].end){std::size_t at=begin;while(at<end){auto loc=wAt(weights[wi].logical+at-weights[wi].begin);auto n=std::min(end-at,loc.second);addRun(p.base_runs,at,n,loc.first);at+=n;}}
  else appendCoordinates(p,p.base_runs,begin,end-begin,active?"C_lookup/queries":"C_aux",true,false,cap,active?qRoots:aRoots);
 }
 std::size_t covered=0;for(auto&r:p.base_runs){need(r.begin==covered && r.count,"partitioned native coordinate gap");covered+=r.count;}need(covered==p.master_actual,"partitioned complete master coverage");
 std::vector<const WitnessObject*>helpers;for(auto&o:c.objects)if(o.kind!=ObjectKind::BaseMaster){View v;v.id=o.id;v.actual=o.actual;v.padded=o.padded;v.kind=o.kind;v.mapped=o.kind==ObjectKind::MappedChild || o.kind==ObjectKind::ModelW;v.spans=o.spans;need(v.actual && v.actual<=v.padded && p.views.emplace(v.id,v).second,"partitioned duplicate/wrong-shape view");if(!v.mapped)helpers.push_back(&o);}
 std::sort(helpers.begin(),helpers.end(),[](auto a,auto b){return a->padded!=b->padded?a->padded>b->padded:a->id<b->id;});
 for(auto*o:helpers){auto&v=p.views.at(o->id);appendCoordinates(p,v.pieces,0,o->padded,"C_lookup/helpers",false,false,cap,hRoots);v.start=v.pieces.front().start;}
 for(auto&r:p.roots){r.padded=power2(r.actual);need(r.padded<=cap,"partitioned root exceeded configured cap");}
 return p;
}
std::string partitionedIdentity(const Layout&p){
 need(p.master_actual && p.base.empty(),"partitioned mixed dense/compressed map");Writer w;w.text("R7-FULL12-CLASSIFIED-PARTITION-ROWMAJOR-v1");w.text("hyrax-row-major-low-column-bits-v1");w.text(p.native);w.number(p.master_actual,8);w.number(p.roots.size());
 for(auto&r:p.roots){w.text(r.id);w.number(r.actual,8);w.number(r.padded,8);w.number(r.registered);w.number(r.model);}
 w.number(p.base_runs.size());for(auto&r:p.base_runs)encodePiece(w,r);w.number(p.views.size());
 for(auto&[id,v]:p.views){w.text(id);w.number(v.actual,8);w.number(v.padded,8);w.number(v.mapped);w.number(unsigned(v.kind));w.number(v.spans.size());for(auto&s:v.spans){w.number(s.offset,8);w.number(s.count,8);w.scalar(s.bias);}w.number(v.pieces.size());for(auto&r:v.pieces)encodePiece(w,r);}
 return hex(r1::hash(w.b));
}
void writePartitionedLayout(std::ostream&o,const Layout&p){
 // IDs are generated internally from already validated native PC roles.
 o<<"{\"suite\":\"R7-FULL12-CLASSIFIED-PARTITION-ROWMAJOR-v1\",\"status\":\"LAYOUT_PLANNED_NOT_AUTHENTICATED\",\"identity\":\""<<identity(p)<<"\",\"master_actual\":"<<baseSize(p)<<",\"dense_mapping_bytes\":0,\"base_coordinate_runs\":[";
 bool first=true;auto piece=[&](const Piece&r){if(!first)o<<',';first=false;o<<"{\"original\":"<<r.begin<<",\"count\":"<<r.count<<",\"root\":"<<r.start.root<<",\"local\":"<<r.start.index<<'}';};for(auto&r:p.base_runs)piece(r);
 o<<"],\"roots\":[";first=true;for(auto&r:p.roots){if(!first)o<<',';first=false;o<<"{\"id\":\""<<r.id<<"\",\"actual\":"<<r.actual<<",\"padded\":"<<r.padded<<",\"registered\":"<<(r.registered?"true":"false")<<",\"model\":"<<(r.model?"true":"false")<<",\"layout\":\"RowMajor\"}";}
 o<<"],\"views\":[";first=true;for(auto&[id,v]:p.views){if(!first)o<<',';first=false;o<<"{\"id\":\""<<id<<"\",\"actual\":"<<v.actual<<",\"padded\":"<<v.padded<<",\"mapped\":"<<(v.mapped?"true":"false")<<",\"pieces\":[";bool outer=first;first=true;for(auto&r:v.pieces)piece(r);first=outer;o<<"]}";}o<<"]}\n";
}
}
''')
p=Path('src/paper_r7/CMakeLists.txt');p.write_text(p.read_text()+'\ntarget_sources(paper_r7 PRIVATE partitioned_layout.cpp)\n')
# Runtime still requires a new independently pinned chunk anchor backend. Refuse
# this new layout at the old single-anchor boundary rather than mis-register W.
edit('classified_commitments.cpp',' need(io.prover()?bool(base)', ' need(!layout.master_actual,"Full12 partitioned commitments require independently pinned chunk-anchor runtime; unavailable");\n need(io.prover()?bool(base)')
print('BOUNDED_FULL12_PARTITION_IMPLEMENTED')
