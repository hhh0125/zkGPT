#include "paper_r1_wire.hpp"
#include "paper_r7_oneblock_plan.hpp"
#include "paper_r7_wire.hpp"
#include "paper_r2_range_split.hpp"
#include <iostream>
#include <map>
using namespace zkgpt::r7;
struct Group {std::size_t actual=0,padded=0;unsigned bits=0;std::vector<zkgpt::r1::InputSpan>spans;};
void append(std::vector<zkgpt::r1::InputSpan>&out,const std::vector<zkgpt::r1::InputSpan>&in){for(auto&s:in){if(!out.empty() && out.back().offset+out.back().count==s.offset && out.back().bias==s.bias)out.back().count+=s.count;else out.push_back(s);}}
int main(int argc,char**argv){try{
 crypto::need(argc==3,"old-statement new-statement");zkgpt::r1::init();auto old=R7Wire::decodeStatement(zkgpt::r1::readFile(argv[1]));std::map<std::string,Group>original;
 setenv("ZKGPT_RANGE_SPLIT_LARGE","0",1);
 {auto c=master3::trustedContext(12);crypto::need(validateStatement(old,c).code==Code::Ok,"baseline public statement changed");for(auto&r:c.plan.ranges){auto&s=c.plan.sources[r.child];auto&g=original[r.id];g.actual=s.spec.actual;g.padded=s.spec.padded;g.bits=r.bits;append(g.spans,s.spans);}}
 setenv("ZKGPT_RANGE_SPLIT_LARGE","1",1);auto c=master3::trustedContext(12);crypto::need(validateStatement(old,c).code!=Code::Ok,"old unsplit statement accepted for split layout");
 std::map<std::string,Group>combined;unsigned parts=0;
 for(auto&r:c.plan.ranges){auto id=r.id.substr(0,r.id.find("/part/"));auto&s=c.plan.sources[r.child];auto&g=combined[id];g.actual+=s.spec.actual;g.padded+=s.spec.padded;g.bits=r.bits;append(g.spans,s.spans);++parts;std::cout<<"RANGE_PART "<<r.id<<" actual="<<s.spec.actual<<" padded="<<s.spec.padded<<" bits="<<r.bits<<'\n';}
 crypto::need(combined.size()==original.size(),"group dropped/added");unsigned changed=0;
 for(auto&[id,a]:original){auto&b=combined.at(id);crypto::need(a.actual==b.actual && a.bits==b.bits && a.spans.size()==b.spans.size(),"source coverage shape changed");for(std::size_t j=0;j<a.spans.size();++j)crypto::need(a.spans[j].offset==b.spans[j].offset && a.spans[j].count==b.spans[j].count && a.spans[j].bias==b.spans[j].bias,"source position/bias changed");if(a.padded!=b.padded){++changed;crypto::need(id=="r2/model/range/4" || id=="r2/model/range/6","unexpected group split");}}
 crypto::need(changed==2 && parts==original.size()+2,"unexpected number of split groups");
 auto s=makeStatement(c,old.x,old.y);crypto::need(validateStatement(s,c).code==Code::Ok,"new split public statement invalid");zkgpt::r1::writeFile(argv[2],R7Wire::encodeStatement(s));
 std::cout<<"PASS full12 trusted plan: two large groups split, exact original coverage and bias, old statement rejected, new statement accepted\n";return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
