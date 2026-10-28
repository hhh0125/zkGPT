#include "models.hpp"
#include "gkr_proof_builder.hpp"
#include "gkr_verifier.hpp"
#include "model_queries.hpp"
#include "model_resources.hpp"
#include <fstream>
#include <iostream>
namespace r1=zkgpt::r1;using namespace zkgpt;
int main(int argc,char**argv){try{r1::init();r1::require(argc==2,"OUTPUT_REPORT");LLM nn(1,12,64,768,3072);nn.setR5BEngineeringCandidate();prover p;nn.create(p,false);nn.validateCurrentWitness(p);GeneratorDomain domain{"zkGPT/main",std::uint32_t(std::size_t(1)<<(p.cc.l-p.cc.l/2))};GKRProofBuilder b(&p,p.C,nn.getWitnessRegistry().shape(),domain);b.build();std::vector<GKRInputTerminalClaim>claims;std::string error;r1::require(verifyGKRWithClaims(p.C,b.publicStatement(),b.proof(),claims,&error),error);auto plan=r2::planModelQueries(p.C,nn.getWitnessRegistry(),nn.parameterRegions(),r1::ExpProfile::LegacyClampOne);auto expected=r2::modelResources(p.C,plan).terminals;r1::require(claims.size()==expected,"R5B real GKR terminal coverage");std::ofstream out(argv[1]);out<<"{\"scope\":\"real R5B GKR only, not production Range/Exp/common-input acceptance\",\"gkr_accepted\":true,\"terminals\":"<<claims.size()<<",\"circuit\":\""<<plan.circuit_id<<"\"}";out.close();r1::require(!out.fail(),"report write failed");std::cout<<"PASS real R5B GKR-only with "<<claims.size()<<" authenticated input terminals; combined proof still required\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
