#include "master_layout.hpp"
#include "execution_target.hpp"
#include "paper_r2/r6_model.hpp"
#include "paper_r2/r5b_trust.hpp"
#include "fixed_cw.hpp"
#include <algorithm>
#include <set>
#include <iostream>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
namespace zkgpt::r7::master3 {
QuietStdout::QuietStdout(){std::cout.flush();std::fflush(stdout);saved_=dup(1);int fd=open("/dev/null",O_WRONLY);need(saved_>=0 && fd>=0 && dup2(fd,1)>=0,"M3 public-plan output isolation");close(fd);}
QuietStdout::~QuietStdout(){std::cout.flush();std::fflush(stdout);dup2(saved_,1);close(saved_);}
R7Context trustedContext(unsigned blocks){const auto target=executionTarget(blocks);QuietStdout quiet;return buildR6Context(target.manifest_pin,blocks);}
void validate(const Layout&l){
 if(!(l.actual==16642424 && l.padded==(1u<<24) && l.profile==r5b::profileHash() && l.context.size()==64 && l.object_count==331))throw std::runtime_error("M3 frozen layout identity: actual="+std::to_string(l.actual)+" padded="+std::to_string(l.padded)+" objects="+std::to_string(l.object_count)+" context_bytes="+std::to_string(l.context.size()));
 std::uint64_t end=0;std::set<std::string>ids;for(auto&r:l.regions){need(ids.insert(r.id).second && !r.id.empty() && r.offset==end && r.count>0 && r.offset<l.actual && r.count<=l.actual-r.offset && r.stride==1 && r.rows>0 && r.columns>0 && r.rows<=r.count && r.count/r.rows==r.columns && r.count%r.rows==0 && r.logical_rows<=r.rows && r.logical_columns<=r.columns,"M3 region overlap/coverage/overflow/shape");end+=r.count;}need(end==l.actual,"M3 missing layout coverage");
 need(l.projection.size()==2050,"M3 FC projection coverage");for(auto i:l.projection)need(i<l.actual,"M3 FC index bounds");
 ids.clear();need(l.views.size()==332 && l.views.front().kind==ObjectKind::BaseMaster,"M3 source identity count");for(auto&v:l.views){need(ids.insert(v.id).second && v.actual && v.actual<=v.padded,"M3 source identity conflict/length");if(!v.spans.empty()){std::uint64_t n=0;for(auto&s:v.spans){need(s.count && s.offset<l.actual && s.count<=l.actual-s.offset && n<=v.actual && s.count<=v.actual-n,"M3 view bounds/overflow");n+=s.count;}need(n==v.actual,"M3 view mapping omission");}}
}
Layout trustedLayout(const R7Context&context){
 QuietStdout quiet;
 LLM nn(1,12,64,768,3072);r6::configure(nn);auto g=nn.buildPublicCircuit();auto p=r2::planModelQueries(g,nn.getWitnessRegistry(),nn.parameterRegions(),r1::ExpProfile::LegacyClampOne);r6::auditGraph(g,nn,p);
 need(p.master==context.plan.master && p.circuit_id==context.plan.circuit_id,"M3 trusted plan mismatch");Layout l;l.actual=p.master.actual;l.padded=p.master.padded;l.profile=r5b::profileHash();l.context=context.layout_sha256;l.object_count=context.objects.size()-1;l.views=context.objects;
 std::vector<Region>known;known.push_back({"R6/public-X","X","row-major-32x1024",0,32768,32,1024,1});
 auto padded=[](std::size_t n){std::size_t p=1;while(p<n){need(p<=(1u<<23),"M3 physical shape overflow");p*=2;}return p;};
 for(auto&r:nn.parameterRegions())known.push_back({"R6/parameter/"+r.name,"W","original-padded-output-major",r.offset,r.count,padded(r.rows),padded(r.cols),1,r.rows,r.cols,""});
 std::size_t affine=0;for(auto&r:nn.getWitnessRegistry().constraints()){if(r.name=="r5b_input"){need(r.val0_offset==0 && r.count==32768,"M3 X/range alias mismatch");known.front().alias="R6/witness/r5b_input";continue;}known.push_back({"R6/witness/"+r.name,witnessKindName(r.kind),"original-R6-val0",r.val0_offset,r.count,1,r.count,1});if(r.name=="round_0_affine")affine=r.val0_offset;}
 std::sort(known.begin(),known.end(),[](auto&a,auto&b){return a.offset<b.offset;});std::size_t end=0;for(auto&r:known){need(r.offset>=end,"M3 trusted regions overlap");if(r.offset>end)l.regions.push_back({"R6/base-gap/"+std::to_string(end),"R6-base-unclassified","original-R6-val0",end,r.offset-end,1,r.offset-end,1});l.regions.push_back(r);end=r.offset+r.count;}if(end<l.actual)l.regions.push_back({"R6/base-gap/"+std::to_string(end),"R6-base-unclassified","original-R6-val0",end,l.actual-end,1,l.actual-end,1});
 const auto&fc=g.circuit.at(4);need(fc.ty==layerType::FCONN && fc.ori_id_u.size()==32768 && fc.ori_id_v.size()==4194304 && affine,"M3 frozen FC source structure");for(unsigned i=0;i<1024;++i)l.projection.push_back(fc.ori_id_u[i]);for(unsigned i=0;i<1024;++i)l.projection.push_back(fc.ori_id_v[i]);l.projection.push_back(affine);l.projection.push_back(nn.parameterRegions().at(1).offset);validate(l);return l;
}
Bytes encodeLayout(const Layout&l){validate(l);Writer w;w.text("R7-BASE-MASTER-LAYOUT-v1");w.text(l.profile);w.text(l.context);w.number(l.actual,8);w.number(l.padded,8);w.number(l.object_count);w.number(l.regions.size());for(auto&r:l.regions){w.text(r.id);w.text(r.kind);w.text(r.order);for(auto n:{r.offset,r.count,r.rows,r.columns,r.stride,r.logical_rows,r.logical_columns})w.number(n,8);w.text(r.alias);}w.number(l.projection.size());for(auto i:l.projection)w.number(i,8);w.number(l.views.size());for(auto&v:l.views){w.text(v.id);w.text(v.parent);w.number(unsigned(v.kind));w.number(v.actual,8);w.number(v.padded,8);w.number(v.spans.size());for(auto&s:v.spans){w.number(s.offset,8);w.number(s.count,8);w.scalar(s.bias);}}return w.b;}
std::string layoutDigest(const Layout&l){return hex(r1::hash(encodeLayout(l)));}
std::string scopeDigest(const Layout&l,std::size_t n,bool projection){Writer w;w.text(layoutDigest(l));w.text(projection?"trusted-FC-source-projection-2050":"original-Base-Master-prefix");w.number(n,8);return hex(r1::hash(w.b));}
Gather fcMap(unsigned k){need(k<3,"M3 FC map id");Gather m;m.id="R6/Layer4/FC0/source/"+std::to_string(k);m.child_size=k==2?1:1024;m.bias.assign(m.child_size,Fr(0));if(k==2){m.terms.push_back({0,2048,Fr(1)});m.terms.push_back({0,2049,Fr(-1)});}else for(unsigned i=0;i<1024;++i)m.terms.push_back({i,1024*k+i,Fr(1)});return m;}
Gather testMap(const std::string&kind){Gather m;m.id="test-gather/"+kind;m.child_size=16;m.bias.assign(16,Fr(0));for(unsigned i=0;i<15;++i){std::size_t source=kind=="repeat" && i<3?3:kind=="stride"?7+5*i:i+3;m.terms.push_back({i,source,Fr(1)});if(kind=="affine")m.bias[i]=Fr(int(i)-5);}need(kind=="repeat" || kind=="stride" || kind=="affine" || kind=="slice","M3 test map type");return m;}
void validate(const Gather&m,std::size_t n){need(!m.id.empty() && m.child_size && m.child_size<=1024 && !(m.child_size&(m.child_size-1)) && !m.terms.empty() && m.terms.size()<=2048 && m.bias.size()==m.child_size,"M3 gather shape");for(auto&t:m.terms)need(t.child<m.child_size && t.source<n && t.coefficient.isValid(),"M3 gather indices/coefficient");}
Bytes encodeGather(const Gather&m){Writer w;w.text(m.id);w.number(m.child_size);w.number(m.terms.size());for(auto&t:m.terms){w.number(t.child);w.number(t.source);w.scalar(t.coefficient);}for(auto&b:m.bias)w.scalar(b);return w.b;}
std::vector<Fr> gatherKernel(const Gather&m,const std::vector<Fr>&point,std::size_t n,Fr&bias){validate(m,n);need(point.size()==r1::log2exact(m.child_size),"M3 gather query dimension");auto weights=r1::weights(point);std::vector<Fr>k(n,Fr(0));bias=0;for(auto&t:m.terms)k[t.source]+=weights[t.child]*t.coefficient;for(std::size_t i=0;i<m.bias.size();++i)bias+=weights[i]*m.bias[i];return k;}
}
