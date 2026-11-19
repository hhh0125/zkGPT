#include "paper_r7_lookup_prepare.hpp"
#include <array>
namespace zkgpt::r7::oneblock::classified {
namespace {
std::size_t integer(const Fr&f,std::uint64_t bound){auto b=r1::encode(f);std::uint64_t n=0;for(unsigned j=0;j<8;++j)n|=std::uint64_t(b[j])<<(8*j);for(unsigned j=8;j<32;++j)need(!b[j],"prepared integer overflow");need(n<bound,"prepared table/query bound");return n;}
std::vector<Fr>& saved(Prepared&p,const std::string&id,std::size_t n){return *p.emplace(id,std::make_shared<std::vector<Fr>>(n)).first->second;}
void memory(Prepared&p,const std::string&id,std::size_t n,const Value&a,const std::vector<Fr>&table){auto&read=saved(p,id+"/read",n);auto&final=saved(p,id+"/final",table.size());std::vector<std::size_t>count(table.size(),0);for(std::size_t i=0;i<n;++i){auto j=integer(a(i),table.size());read[i]=Fr(count[j]++);}for(std::size_t j=0;j<count.size();++j)final[j]=Fr(count[j]);}
void range(Prepared&p,const std::string&id,std::size_t n,const Value&q,unsigned bits){
 struct Lane {
  std::vector<Fr>*digit;
  std::vector<Fr>*read;
  std::vector<Fr>*final;
  unsigned mask;
  std::vector<std::size_t> counts;
 };
 std::vector<Lane> lanes;
 const unsigned count=(bits+8)/9;
 lanes.reserve(count);
 for(unsigned h=0;h<count;++h){
  const auto digit=id+"/digit/"+std::to_string(h);
  const auto memory=id+"/memory/"+std::to_string(h);
  const unsigned size=1u<<std::min(9u,bits-9*h);
  lanes.push_back({&saved(p,digit,n),&saved(p,memory+"/read",n),&saved(p,memory+"/final",size),size-1,std::vector<std::size_t>(size)});
 }
 for(std::size_t i=0;i<n;++i){
  const auto bytes=r1::encode(q(i));
  for(unsigned h=0;h<count;++h){
   auto&lane=lanes[h];const unsigned offset=9*h;
   const unsigned word=unsigned(bytes[offset/8]) | (unsigned(bytes[offset/8+1])<<8);
   const auto digit=(word>>(offset%8)) & lane.mask;
   (*lane.digit)[i]=Fr(digit);
   (*lane.read)[i]=Fr(lane.counts[digit]++);
  }
 }
 for(auto&lane:lanes)
  for(std::size_t j=0;j<lane.counts.size();++j)(*lane.final)[j]=Fr(lane.counts[j]);
}
}
Prepared prepareLookup(const R7Context&c,const Value&base){
 need(bool(base),"prepared requires private base");Prepared p;std::vector<Value>sources;for(auto&source:c.plan.sources){const auto&spans=source.spans;auto actual=source.spec.actual;sources.push_back([&base,&spans,actual](std::size_t i){if(i>=actual)return Fr(0);for(auto&s:spans){if(i<s.count)return base(s.offset+i)+s.bias;i-=s.count;}throw std::runtime_error("prepared source map");});}
 for(auto&r:c.plan.ranges){auto&s=c.plan.sources.at(r.child).spec;range(p,"r7/"+r.id,s.padded,sources.at(r.child),r.bits);}
 for(auto&e:c.plan.exps){auto id="r7/"+e.id;auto&s=c.plan.sources.at(e.index_child).spec;auto t=sources.at(e.index_child),E=sources.at(e.value_child);std::array<std::string,4>names{"x","selector","delta","native-E"};std::array<std::vector<Fr>*,4>helper;
  for(unsigned k=0;k<4;++k)helper[k]=&saved(p,id+"/"+names[k],s.padded);
  for(std::size_t i=0;i<s.padded;++i){
   if(i>=s.actual){
    for(unsigned k=0;k<3;++k)(*helper[k])[i]=Fr(0);
    (*helper[3])[i]=E(i)+Fr(1048576);continue;
   }
   auto raw=integer(t(i),std::uint64_t(1)<<32);
   (*helper[0])[i]=Fr(std::min<std::size_t>(raw,8191));
   (*helper[1])[i]=Fr(raw<8191?1:0);
   (*helper[2])[i]=Fr(raw<8191?8190-raw:raw-8191);
   (*helper[3])[i]=E(i);
  }
  range(p,id+"/range/t",s.padded,t,32);
  for(unsigned k=0;k<3;++k)range(p,id+"/range/"+names[k],s.padded,[&helper,k](std::size_t i){return (*helper[k])[i];},std::array<unsigned,3>{13,1,32}[k]);
  memory(p,id+"/indexed",s.padded,[&helper](std::size_t i){return (*helper[0])[i];},r1::expTable(e.profile));
 }
 return p;
}
}
