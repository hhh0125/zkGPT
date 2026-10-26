#include "protocol.hpp"
#include "wire.hpp"
#include "tests/gkr_test_fixture.hpp"
#include <iostream>
namespace r1=zkgpt::r1;
using namespace zkgpt::r2;
int main(){try{
 r1::init();gkr_test::SmallFconnFixture f;const auto &w=f.proverInstance().val[0];
 auto root=r1::PCSpec::make("r3/progress-master",32,32,r1::Layout::RowMajor);
 auto q=r1::PCSpec::make("r3/progress-query",1,1,r1::Layout::ColumnMajor);
 std::vector<std::vector<Fr>> v{{Fr(3)}};r1::LookupBatchSpec s;
 s.input={"progress-must-not-change-transcript",r1::commit(root,w),{{r1::commit(q,v[0]),{{0,1,Fr(0)}}}},{{0,Fr(3)}}};s.ranges={{0,"r3/progress-range",4}};
 auto g=f.statement(),gg=g;auto p=proveR2(f.proverInstance(),g,s,w,v);
 std::vector<std::string> events;auto observed=proveR2(f.proverInstance(),gg,s,w,v,[&](const std::string &e){events.push_back(e);});
 r1::require(serializeProof(g,s,p)==serializeProof(gg,s,observed),"progress changed proof bytes/challenges");
 const std::vector<std::string> expected{"range/0/begin","range/0/end","gkr/begin","gkr/end","bridge/begin","bridge/end","input/begin","input/end"};
 r1::require(events==expected,"missing/reordered proof phases");
 events.clear();verifyR2(f.circuit(),gg,s,observed,[&](const std::string&e){events.push_back(e);});r1::require(events==expected,"verifier progress phases");
 std::cout<<"PASS optional phase progress preserves exact R2 proof bytes and verification\n";return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
