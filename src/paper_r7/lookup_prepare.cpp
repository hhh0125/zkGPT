#include "lookup_prepare.hpp"
#include <array>
namespace zkgpt::r7::oneblock::classified {
namespace {
std::size_t integer(const Fr&f,std::uint64_t bound){auto b=r1::encode(f);std::uint64_t n=0;for(unsigned j=0;j<8;++j)n|=std::uint64_t(b[j])<<(8*j);for(unsigned j=8;j<32;++j)need(!b[j],"prepared integer overflow");need(n<bound,"prepared table/query bound");return n;}
Value saved(Prepared&p,const std::string&id,std::size_t n){need(!p.count(id),"prepared duplicate identity");auto v=std::make_shared<std::vector<Fr>>(n,Fr(0));p.emplace(id,v);return [v](std::size_t i){return v->at(i);};}
void memory(Prepared&p,const std::string&id,std::size_t n,const Value&a,const Value&v,const std::vector<Fr>&table){saved(p,id+"/read",n);saved(p,id+"/final",table.size());auto&read=*p.at(id+"/read");auto&final=*p.at(id+"/final");std::vector<std::size_t>count(table.size(),0);for(std::size_t i=0;i<n;++i){auto j=integer(a(i),table.size());need(v(i)==table[j],"prepared Memory value/table mismatch");read[i]=Fr(count[j]++);}for(std::size_t j=0;j<count.size();++j)final[j]=Fr(count[j]);}
void range(Prepared&p,const std::string&id,std::size_t actual,std::size_t n,const Value&q,unsigned bits){
 for(unsigned h=0;h<(bits+8)/9;++h){auto name=id+"/digit/"+std::to_string(h);auto digit=saved(p,name,n);auto&out=*p.at(name);auto width=std::min(9u,bits-9*h);for(std::size_t i=0;i<n;++i){auto bytes=r1::encode(q(i));for(unsigned k=bits;k<256;++k)need(!(bytes[k/8]&(1u<<(k%8))),"prepared Range width");need(i<actual || q(i).isZero(),"prepared Range logical tail");unsigned value=0;for(unsigned j=0;j<width;++j){auto k=9*h+j;value|=((bytes[k/8]>>(k%8))&1u)<<j;}out[i]=Fr(value);}std::vector<Fr>table(std::size_t(1)<<width);for(std::size_t i=0;i<table.size();++i)table[i]=Fr(i);memory(p,id+"/memory/"+std::to_string(h),n,digit,digit,table);}
}
}
Prepared prepareLookup(const R7Context&c,const Value&base){
 need(bool(base),"prepared requires private base");Prepared p;std::vector<Value>sources;for(auto&source:c.plan.sources){auto spans=source.spans;auto actual=source.spec.actual;sources.push_back([base,spans,actual](std::size_t i){if(i>=actual)return Fr(0);for(auto&s:spans){if(i<s.count)return base(s.offset+i)+s.bias;i-=s.count;}throw std::runtime_error("prepared source map");});}
 for(auto&r:c.plan.ranges){auto&s=c.plan.sources.at(r.child).spec;range(p,"r7/"+r.id,s.actual,s.padded,sources.at(r.child),r.bits);}
 for(auto&e:c.plan.exps){auto id="r7/"+e.id;auto&s=c.plan.sources.at(e.index_child).spec;auto t=sources.at(e.index_child),E=sources.at(e.value_child);std::array<std::string,4>names{"x","selector","delta","native-E"};std::array<Value,4>helper;for(unsigned k=0;k<4;++k)helper[k]=saved(p,id+"/"+names[k],s.padded);
  for(std::size_t i=0;i<s.padded;++i){if(i>=s.actual){p.at(id+"/native-E")->at(i)=E(i)+Fr(1048576);continue;}auto raw=integer(t(i),std::uint64_t(1)<<32);p.at(id+"/x")->at(i)=Fr(std::min<std::size_t>(raw,8191));p.at(id+"/selector")->at(i)=Fr(raw<8191?1:0);p.at(id+"/delta")->at(i)=Fr(raw<8191?8190-raw:raw-8191);p.at(id+"/native-E")->at(i)=E(i);}
  range(p,id+"/range/t",s.actual,s.padded,t,32);for(unsigned k=0;k<3;++k)range(p,id+"/range/"+names[k],s.actual,s.padded,helper[k],std::array<unsigned,3>{13,1,32}[k]);memory(p,id+"/indexed",s.padded,helper[0],helper[3],r1::expTable(e.profile));
 }
 std::size_t expected=0;for(auto&o:c.objects)if(o.kind!=ObjectKind::BaseMaster && o.kind!=ObjectKind::MappedChild && o.kind!=ObjectKind::ModelW){++expected;need(p.count(o.id) && p.at(o.id)->size()==o.padded,"prepared native identity/shape");}need(p.size()==expected,"prepared unregistered object");return p;
}
}
