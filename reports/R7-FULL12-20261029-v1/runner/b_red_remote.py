import subprocess
Path('src/paper_r7/partitioned_layout_tests.cpp').write_text(r'''#include "classified_layout.hpp"
#include <iostream>
#include <set>
using namespace zkgpt::r7;using namespace oneblock::classified;
int main(){try{
 zkgpt::r1::init();R7Context c;c.layout_sha256=std::string(64,'a');c.plan.shape.layer_count=12;
 c.plan.master=zkgpt::r1::PCSpec::make("master",31,32,zkgpt::r1::Layout::RowMajor);
 c.plan.sources.push_back({zkgpt::r1::PCSpec::make("query",31,32,zkgpt::r1::Layout::RowMajor),{{0,31,Fr(2)}}});
 c.plan.sources.push_back({zkgpt::r1::PCSpec::make("W",11,16,zkgpt::r1::Layout::RowMajor),{{3,5,Fr(0)},{16,6,Fr(0)}}});
 c.objects={{"r7/master","",ObjectKind::BaseMaster,31,32,{},false},{"r7/query","r7/master",ObjectKind::MappedChild,31,32,{{0,31,Fr(2)}},false},{"r7/W","r7/master",ObjectKind::ModelW,11,16,{{3,5,Fr(0)},{16,6,Fr(0)}},false},{"r7/helper","r7/query",ObjectKind::RangeDigit,20,32,{},false}};
 auto p=makeLayout(c,8);validate(p,c);need(p.base.empty() && baseSize(p)==31,"partition map must not allocate per-coordinate array");
 std::set<std::pair<unsigned,unsigned>>seen;for(std::size_t i=0;i<31;++i){auto x=baseCoordinate(p,i);need(x.root<p.roots.size() && x.index<p.roots[x.root].actual && seen.insert({x.root,x.index}).second,"partition injectivity and bounds");}
 std::size_t total=0,models=0;std::set<std::string>ids;for(auto&r:p.roots){need(r.padded<=8 && r.actual<=r.padded && ids.insert(r.id).second,"bounded unique physical roots");if(r.registered)total+=r.actual;if(r.model){++models;need(r.registered,"W knowledge retained");}}
 need(total==31 && models==2,"complete base routing and W chunks");
 auto&w=p.views.at("r7/W");for(std::size_t i=0;i<11;++i){auto x=locate(p,w,i);need(!x.constant && x.bias.isZero() && x.coordinate==baseCoordinate(p,i<5?3+i:16+i-5),"W logical to physical coordinates");}
 need(locate(p,w,15).constant,"W logical padded tail");auto&q=p.views.at("r7/query");need(locate(p,q,7).bias==Fr(2),"source affine bias preserved");need(locate(p,q,31).constant,"query virtual tail");
 auto&h=p.views.at("r7/helper");need(h.pieces.size()==4,"helper crosses four bounded roots");for(std::size_t i=0;i<32;++i){auto x=locate(p,h,i);need(!x.constant && p.roots[x.coordinate.root].actual>x.coordinate.index,"helper complete logical padded domain");}
 auto bad=p;bad.base_runs.front().start.index++;need(identity(bad)!=identity(p),"wrong partition identity changes digest");bool rejected=false;try{validate(bad,c);}catch(...){rejected=true;}need(rejected,"wrong map rejected against native context");
 bad=p;bad.roots.front().id+="/replacement";need(identity(bad)!=identity(p),"wrong root identity changes digest");
 std::cout<<"PARTITIONED_MAPPING_TESTED roots="<<p.roots.size()<<" runs="<<p.base_runs.size()<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
''')
p=Path('src/paper_r7/CMakeLists.txt');p.write_text(p.read_text()+'\nadd_executable(r7_partitioned_layout_tests partitioned_layout_tests.cpp)\ntarget_link_libraries(r7_partitioned_layout_tests PRIVATE paper_r7)\n')
stage=Path('reports/R7-FULL12-20261029-v1')
subprocess.run(['cmake','-S','.','-B','build/release'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)
with (stage/'partition-red.log').open('wb') as out:r=subprocess.run(['cmake','--build','build/release','--target','r7_partitioned_layout_tests','-j4'],stdout=out,stderr=subprocess.STDOUT)
assert r.returncode!=0;print((stage/'partition-red.log').read_text()[-2000:])
