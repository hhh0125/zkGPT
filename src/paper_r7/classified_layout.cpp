#include "classified_layout.hpp"
#include <algorithm>
#include <openssl/evp.h>
#include <sstream>
namespace zkgpt::r7::oneblock::classified {
std::size_t power2(std::size_t n){need(n && n<=(1u<<26),"classified size bound");std::size_t x=1;while(x<n)x*=2;return x;}
Layout makeLayout(const R7Context&c,std::size_t cap){
 if(c.plan.shape.layer_count==12)return makePartitionedLayout(c,cap);
 need(cap && cap<=(1u<<26) && !(cap&(cap-1)),"classified slab bound");Layout p;p.native=c.layout_sha256;p.base.resize(c.plan.master.actual);std::vector<unsigned char>tag(p.base.size(),0);
 p.roots={{"C_W",0,0,true,true},{"C_lookup/queries",0,0,true,false},{"C_aux",0,0,true,false}};
 need(!c.plan.sources.empty(),"classified missing W source");const auto&w=c.plan.sources.back();std::size_t windex=0;
 for(auto&s:w.spans){need(s.bias.isZero() && s.offset<p.base.size() && s.count<=p.base.size()-s.offset,"classified W span");for(std::size_t j=0;j<s.count;++j){auto i=s.offset+j;need(!tag[i],"classified W overlap");tag[i]=1;p.base[i]={0,std::uint32_t(windex++)};}}
 need(windex==w.spec.actual,"classified W actual");p.roots[0].actual=windex;p.roots[0].padded=w.spec.padded;
 for(std::size_t k=0;k+1<c.plan.sources.size();++k)for(auto&s:c.plan.sources[k].spans){need(s.offset<p.base.size() && s.count<=p.base.size()-s.offset,"classified query span");for(std::size_t j=0;j<s.count;++j)if(!tag[s.offset+j])tag[s.offset+j]=2;}
 for(std::size_t i=0;i<tag.size();++i)if(tag[i]!=1){auto root=tag[i]==2?1:2;p.base[i]={std::uint32_t(root),std::uint32_t(p.roots[root].actual++)};}
 for(std::size_t k=1;k<3;++k){need(p.roots[k].actual>0,"classified empty base category");p.roots[k].padded=power2(p.roots[k].actual);}
 std::vector<const WitnessObject*>helper;
 for(auto&o:c.objects)if(o.kind!=ObjectKind::BaseMaster){View v;v.id=o.id;v.actual=o.actual;v.padded=o.padded;v.kind=o.kind;v.mapped=o.kind==ObjectKind::MappedChild || o.kind==ObjectKind::ModelW;v.spans=o.spans;p.views.emplace(v.id,v);if(!v.mapped)helper.push_back(&o);}
 std::sort(helper.begin(),helper.end(),[](auto a,auto b){return a->padded!=b->padded?a->padded>b->padded:a->id<b->id;});
 for(auto*o:helper){need(o->padded<=cap,"classified object exceeds slab");std::size_t k=3;for(;k<p.roots.size();++k)if(o->padded<=cap-p.roots[k].actual)break;
  if(k==p.roots.size())p.roots.push_back({"C_lookup/helpers/"+std::to_string(k-3),0,0,false,false});
  auto&v=p.views.at(o->id);v.start={std::uint32_t(k),std::uint32_t(p.roots[k].actual)};p.roots[k].actual+=o->padded;
 }
 for(std::size_t k=3;k<p.roots.size();++k)p.roots[k].padded=power2(p.roots[k].actual);
 return p;
}
Located locate(const Layout&p,const View&v,std::size_t j){need(j<v.padded,"classified view query bounds");if(!v.mapped && !v.pieces.empty()){for(auto&part:v.pieces)if(j>=part.begin && j-part.begin<part.count)return {{part.start.root,std::uint32_t(part.start.index+j-part.begin)},Fr(0),false};throw std::runtime_error("partitioned helper map omission");}if(!v.mapped)return {{v.start.root,std::uint32_t(v.start.index+j)},Fr(0),false};if(j>=v.actual)return {{},Fr(0),true};for(auto&s:v.spans){if(j<s.count){need(s.offset+j<baseSize(p),"classified trusted map bounds");return {baseCoordinate(p,s.offset+j),s.bias,false};}j-=s.count;}throw std::runtime_error("classified missing source span");}
std::string identity(const Layout&p){
 if(p.master_actual)return partitionedIdentity(p);
 Writer w;w.text(suite);w.text("hyrax-row-major-low-column-bits-v1");w.text(p.native);w.number(p.roots.size());for(auto&r:p.roots){w.text(r.id);w.number(r.actual,8);w.number(r.padded,8);w.number(r.registered);w.number(r.model);}w.number(p.views.size());
 for(auto&[id,v]:p.views){w.text(id);w.number(v.actual,8);w.number(v.padded,8);w.number(v.mapped);w.number(unsigned(v.kind));w.number(v.start.root);w.number(v.start.index);w.number(v.spans.size());for(auto&s:v.spans){w.number(s.offset,8);w.number(s.count,8);w.scalar(s.bias);}}
 std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)>ctx(EVP_MD_CTX_new(),EVP_MD_CTX_free);need(ctx && EVP_DigestInit_ex(ctx.get(),EVP_sha256(),nullptr)==1,"classified digest init");need(EVP_DigestUpdate(ctx.get(),w.b.data(),w.b.size())==1,"classified digest metadata");std::array<unsigned char,32768>b{};std::size_t used=0;
 for(auto&v:p.base){for(auto x:{v.root,v.index})for(unsigned j=0;j<4;++j)b[used++]=std::uint8_t(x>>(8*j));if(used==b.size()){need(EVP_DigestUpdate(ctx.get(),b.data(),used)==1,"classified digest maps");used=0;}}
 if(used)need(EVP_DigestUpdate(ctx.get(),b.data(),used)==1,"classified digest final maps");
 Digest out{};unsigned n=0;need(EVP_DigestFinal_ex(ctx.get(),out.data(),&n)==1 && n==32,"classified digest end");return hex(out);
}
void validate(const Layout&p,const R7Context&c,std::size_t cap){auto expected=makeLayout(c,cap);need(identity(p)==identity(expected),"classified untrusted layout/map");}
void writeLayout(std::ostream&o,const Layout&p){
 if(p.master_actual){writePartitionedLayout(o,p);return;}
 o<<"{\"suite\":\""<<suite<<"\",\"identity\":\""<<identity(p)<<"\",\"roots\":[";bool first=true;for(auto&r:p.roots){if(!first)o<<',';first=false;o<<"{\"id\":\""<<r.id<<"\",\"actual\":"<<r.actual<<",\"padded\":"<<r.padded<<",\"columns\":"<<(r.model?4096:std::min<std::size_t>(1024,r.padded))<<",\"physical_tail\":\"zero\",\"basis\":\""<<(r.model?"approved-R6-row-major":"Hyrax-A3-row-major")<<"\",\"base_root\":"<<(r.registered?"true":"false")<<",\"full_base_registration\":false"<<'}';}
 o<<"],\"base_coordinate_runs\":[";first=true;for(std::size_t i=0;i<p.base.size();){auto begin=i;auto r=p.base[i];while(++i<p.base.size() && p.base[i].root==r.root && p.base[i].index==r.index+i-begin){}if(!first)o<<',';first=false;o<<"{\"original\":"<<begin<<",\"count\":"<<i-begin<<",\"root\":"<<r.root<<",\"local\":"<<r.index<<'}';}
 o<<"],\"views\":[";first=true;for(auto&[id,v]:p.views){if(!first)o<<',';first=false;o<<"{\"id\":\""<<id<<"\",\"kind\":"<<unsigned(v.kind)<<",\"actual\":"<<v.actual<<",\"logical_padded\":"<<v.padded<<",\"mapped\":"<<(v.mapped?"true":"false")<<",\"root\":"<<v.start.root<<",\"offset\":"<<v.start.index<<",\"spans\":[";bool sfirst=true;for(auto&s:v.spans){if(!sfirst)o<<',';sfirst=false;o<<"{\"base\":"<<s.offset<<",\"count\":"<<s.count<<",\"signed_bias_Fr_decimal\":\""<<s.bias.getStr()<<"\"}";}o<<"],\"padding\":\""<<(v.mapped?"virtual-zero":"stored-native-logical-values")<<"\"}";}o<<"]}\n";
}
}
