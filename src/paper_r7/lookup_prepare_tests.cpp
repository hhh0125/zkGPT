#include "lookup_prepare.hpp"
#include <iostream>
using namespace zkgpt::r7;using namespace oneblock;using namespace classified;
int main(){try{r1::init();R7Context c;std::vector<Fr>base;const std::vector<std::uint32_t>raw{0,8190,8191,8192,0xffffffffu};for(auto x:raw)base.push_back(Fr(x));for(auto x:raw)base.push_back(Fr(r1::expValue(x,r1::ExpProfile::LegacyClampOne)));
 auto t=r1::PCSpec::make("unit/t",5,8,r1::Layout::ColumnMajor),E=r1::PCSpec::make("unit/E",5,8,r1::Layout::ColumnMajor);c.plan.sources={{t,{{0,5,Fr(0)}}},{E,{{5,5,Fr(0)}}}};c.plan.exps.push_back({0,1,"unit/exp",r1::ExpProfile::LegacyClampOne});r1::ExpSpec spec{"unit/exp",t,E,r1::ExpProfile::LegacyClampOne};
 auto object=[&](const r1::PCSpec&s,ObjectKind k){WitnessObject o;o.id="r7/"+s.role;o.actual=s.actual;o.padded=s.padded;o.kind=k;c.objects.push_back(o);};
 auto memory=[&](const r1::MemorySpec&s){object(r1::readSpec(s),ObjectKind::MemoryRead);object(r1::finalSpec(s),ObjectKind::MemoryFinal);};
 for(unsigned k=0;k<4;++k){object(r1::expHelperSpec(spec,k),ObjectKind::ExpHelper);auto range=r1::expRangeSpec(spec,k);for(unsigned h=0;h<(range.bits+8)/9;++h){object(r1::digitSpec(range,h),ObjectKind::RangeDigit);memory(r1::digitMemory(range,h));}}memory(r1::expMemorySpec(spec));
 auto p=prepareLookup(c,[&](std::size_t j){return base.at(j);});auto&id=p.at("r7/unit/exp/native-E");for(unsigned i=5;i<8;++i)need(id->at(i)==Fr(1048576),"prepared Exp logical nonzero padding");
 const std::vector<std::uint32_t>x{0,8190,8191,8191,8191,0,0,0};std::vector<std::size_t>counts(8192,0);auto read=p.at("r7/unit/exp/indexed/read"),final=p.at("r7/unit/exp/indexed/final");for(unsigned i=0;i<8;++i)need(read->at(i)==Fr(counts[x[i]]++),"prepared read exact query order and duplicates");for(unsigned j=0;j<counts.size();++j)need(final->at(j)==Fr(counts[j]),"prepared complete final counters");
 need(p.at("r7/unit/exp/selector")->at(1)==Fr(1) && p.at("r7/unit/exp/selector")->at(2)==Fr(0),"prepared exact Exp boundary");need(p.at("r7/unit/exp/delta")->at(4)==Fr(std::uint64_t(0xffffffffu)-8191),"prepared Exp u32 max");std::cout<<"LOOKUP_PREPARATION_TESTED objects="<<p.size()<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"LOOKUP_PREPARATION_FAILED "<<e.what()<<'\n';return 1;}}
