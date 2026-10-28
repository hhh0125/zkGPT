#include "wire.hpp"
#include "r5b_trust.hpp"
#include "paper_r1/wire.hpp"
#include <filesystem>
#include <iostream>
namespace r1=zkgpt::r1;using namespace zkgpt;
int main(int argc,char**argv){try{r1::init();r1::require(argc==3,"VALID_DIR NEW_DIR");std::filesystem::path in=argv[1],out=argv[2];r1::require(!std::filesystem::exists(out),"R5B attack artifacts exist");std::filesystem::create_directories(out);auto[g,s]=r2::parseStatement(r1::readFile((in/"model.statement").string()));auto p=r2::parseProof(g,s,r1::readFile((in/"model.proof").string()));
 auto save=[&](const std::string&name,const r1::LookupBatchSpec&ss,const r2::Proof&pp){r1::writeFile((out/(name+".statement")).string(),r2::serializeStatement(g,ss));r1::writeFile((out/(name+".proof")).string(),r2::serializeProof(g,ss,pp));};
 auto bad=s;bad.input.context="R4-partial-numerical-NOT-PAPER-APPROVED/wrong-profile/"+std::string(64,'0');save("wrong-profile-context",bad,p);
 bool found=false;for(std::size_t j=0;j<s.ranges.size();++j)if(s.ranges[j].bits==11){bad=s;bad.ranges[j].bits=12;save("rounding-closed-upper-bound",bad,p);auto k=s.ranges[j].child;bad=s;bad.input.children[k].commitment.rows[0]+=r1::generators(bad.input.children[k].commitment.spec).u;save("rounding-remainder-child",bad,p);found=true;break;}r1::require(found,"actual FC remainder Range missing");
 found=false;for(std::size_t k=0;k<s.input.children.size();++k)for(std::size_t j=0;j<s.input.children[k].spans.size();++j)if(!found && s.input.children[k].spans[j].bias==Fr(-1)){bad=s;bad.input.children[k].spans[j].bias=Fr(0);save("LN-positive-root-view",bad,p);found=true;}r1::require(found,"actual positive sigma view missing");
 std::cout<<"PASS 4 actual R5B statement/proof mutations: wrong profile, enlarged rounding endpoint domain, remainder child replacement, removed LN positivity shift\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
