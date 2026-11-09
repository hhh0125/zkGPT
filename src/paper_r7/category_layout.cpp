#include "classified_layout.hpp"
#include "single_hyrax.hpp"
#include "r6_adapter.hpp"
#include <algorithm>
#include <set>
namespace zkgpt::r7::oneblock::classified {
namespace {
constexpr const char*names[]{"X","W","Aux","LookupQueries","LookupHelpers"};
void run(std::vector<Piece>&out,std::size_t begin,std::size_t count,Coordinate start){
 if(!out.empty()){auto&prev=out.back();if(prev.begin+prev.count==begin && prev.start.root==start.root && std::uint64_t(prev.start.index)+prev.count==start.index){prev.count+=count;return;}}
 out.push_back({begin,count,start});
}
}
Layout makeCategoryLayout(const R7Context&c,std::size_t cap){return categoryLayout(c,nativeXSpans(c),cap);}
Layout categoryLayout(const R7Context&c,const std::vector<r1::InputSpan>&xs,std::size_t cap){
 need(cap && cap<=(1u<<24) && !(cap&(cap-1)) && !c.plan.sources.empty(),"category storage cap/sources");
 Layout p;p.category_mode=true;p.native=c.layout_sha256;p.master_actual=c.plan.master.actual;p.x_spans=xs;
 need(p.master_actual && p.master_actual<=(1u<<28) && !xs.empty(),"category master/X bounds");
 struct Span{std::size_t begin,end,local;};std::vector<Span>x,w;std::set<std::size_t>cuts{0,p.master_actual};std::map<std::size_t,int>events;
 auto spans=[&](const std::vector<r1::InputSpan>&input,std::vector<Span>&out){std::size_t local=0;for(const auto&s:input){need(s.count && s.offset<p.master_actual && s.count<=p.master_actual-s.offset && s.bias.isZero(),"category trusted X/W bounds/bias");out.push_back({s.offset,s.offset+s.count,local});local+=s.count;cuts.insert(s.offset);cuts.insert(s.offset+s.count);}std::sort(out.begin(),out.end(),[](auto a,auto b){return a.begin<b.begin;});for(std::size_t k=1;k<out.size();++k)need(out[k-1].end<=out[k].begin,"category X/W overlap/duplicate");return local;};
 auto xn=spans(xs,x);const auto&weight=c.plan.sources.back();need(spans(weight.spans,w)==weight.spec.actual,"category W coverage");
 for(auto a:x)for(auto b:w)need(a.end<=b.begin || b.end<=a.begin,"category X duplicated in W");
 for(std::size_t k=0;k+1<c.plan.sources.size();++k){std::size_t count=0;for(const auto&s:c.plan.sources[k].spans){need(s.count && s.offset<p.master_actual && s.count<=p.master_actual-s.offset,"category query bounds");count+=s.count;cuts.insert(s.offset);cuts.insert(s.offset+s.count);++events[s.offset];--events[s.offset+s.count];}need(count==c.plan.sources[k].spec.actual,"category query coverage");}
 std::array<std::vector<std::size_t>,5>chunks;
 auto append=[&](unsigned category,std::vector<Piece>&out,std::size_t begin,std::size_t count){
  while(count){auto&ids=chunks[category];auto&cat=p.categories[category];
   if(ids.empty() || p.roots[ids.back()].actual==cap){need(p.roots.size()<UINT32_MAX,"category chunk index overflow");ids.push_back(p.roots.size());p.roots.push_back({std::string(names[category])+"/storage/"+std::to_string(ids.size()-1),0,0,category!=4,category==1,category,cat.actual});}
   const auto k=ids.back();auto&r=p.roots[k];auto n=std::min(count,cap-r.actual);run(out,begin,n,{std::uint32_t(k),std::uint32_t(r.actual)});r.actual+=n;r.padded=r.actual;cat.actual+=n;begin+=n;count-=n;
  }
 };
 std::vector<Piece>wr,xr;append(0,xr,0,xn);append(1,wr,0,weight.spec.actual);
 auto mapped=[&](std::vector<Piece>&out,const std::vector<Piece>&source,std::size_t original,std::size_t local,std::size_t count){
  while(count){auto it=std::upper_bound(source.begin(),source.end(),local,[](std::size_t j,const Piece&r){return j<r.begin;});need(it!=source.begin(),"category source prefix");--it;auto delta=local-it->begin;need(delta<it->count,"category source gap");auto n=std::min(count,it->count-delta);run(out,original,n,{it->start.root,std::uint32_t(it->start.index+delta)});original+=n;local+=n;count-=n;}
 };
 auto xi=std::size_t(0),wi=std::size_t(0);int active=0;
 for(auto it=cuts.begin();std::next(it)!=cuts.end();++it){auto begin=*it,end=*std::next(it);active+=events[begin];need(active>=0,"category query interval balance");while(xi<x.size() && x[xi].end<=begin)++xi;while(wi<w.size() && w[wi].end<=begin)++wi;
  if(xi<x.size() && x[xi].begin<=begin)mapped(p.base_runs,xr,begin,x[xi].local+begin-x[xi].begin,end-begin);
  else if(wi<w.size() && w[wi].begin<=begin)mapped(p.base_runs,wr,begin,w[wi].local+begin-w[wi].begin,end-begin);
  else append(active?3:2,p.base_runs,begin,end-begin);
 }
 std::size_t covered=0;for(const auto&r:p.base_runs){need(r.begin==covered && r.count,"category master gap");covered+=r.count;}need(covered==p.master_actual,"category complete master coverage");
 std::vector<const WitnessObject*>helpers;for(const auto&o:c.objects)if(o.kind!=ObjectKind::BaseMaster){View v;v.id=o.id;v.actual=o.actual;v.padded=o.padded;v.kind=o.kind;v.mapped=o.kind==ObjectKind::MappedChild || o.kind==ObjectKind::ModelW;v.spans=o.spans;need(v.actual && v.actual<=v.padded && p.views.emplace(v.id,v).second,"category object identity/shape");if(!v.mapped)helpers.push_back(&o);}
 // Distinct objects retain all materialized local padding, even nonzero cells.
 std::sort(helpers.begin(),helpers.end(),[](auto a,auto b){return a->id<b->id;});
 for(auto*o:helpers){auto&v=p.views.at(o->id);append(4,v.pieces,0,o->padded);v.start=v.pieces.front().start;}
 std::array<std::size_t,5>actual{};for(unsigned k=0;k<5;++k)actual[k]=p.categories[k].actual;auto plan=single::makeCategoryPlan(actual,std::string(64,'0'));
 for(const auto&s:plan.segments)for(unsigned k=0;k<5;++k)if(s.source_id==names[k]){p.categories[k].padded=s.padded;p.categories[k].logical_offset=s.offset;p.categories[k].physical_offset=s.physical_offset;}
 return p;
}
std::string categoryIdentity(const Layout&p){
 need(p.category_mode && p.base.empty() && p.master_actual,"category map mode");Writer w;w.text("R7-FIVE-CONTIGUOUS-CATEGORIES-COMMITTED-X-v1");w.text(p.native);w.number(p.master_actual,8);
 for(auto&c:p.categories)for(auto n:{c.actual,c.padded,c.logical_offset,c.physical_offset})w.number(n,8);
 w.number(p.x_spans.size());for(auto&s:p.x_spans){w.number(s.offset,8);w.number(s.count,8);w.scalar(s.bias);}
 w.number(p.roots.size());for(auto&r:p.roots){w.text(r.id);w.number(r.actual,8);w.number(r.padded,8);w.number(r.category);w.number(r.category_offset,8);w.number(r.registered);w.number(r.model);}
 auto pieces=[&](const std::vector<Piece>&runs){w.number(runs.size());for(auto&r:runs){w.number(r.begin,8);w.number(r.count,8);w.number(r.start.root);w.number(r.start.index);}};
 pieces(p.base_runs);w.number(p.views.size());for(auto&[id,v]:p.views){w.text(id);w.number(v.actual,8);w.number(v.padded,8);w.number(unsigned(v.kind));w.number(v.mapped);w.number(v.spans.size());for(auto&s:v.spans){w.number(s.offset,8);w.number(s.count,8);w.scalar(s.bias);}pieces(v.pieces);}
 return hex(r1::hash(w.b));
}
void writeCategoryLayout(std::ostream&o,const Layout&p){o<<"{\"suite\":\"R7-FIVE-CONTIGUOUS-CATEGORIES-COMMITTED-X-v1\",\"identity\":\""<<identity(p)<<"\",\"storage_chunks\":"<<p.roots.size()<<",\"base_runs\":"<<p.base_runs.size()<<",\"root_padding\":0}\n";}
}
