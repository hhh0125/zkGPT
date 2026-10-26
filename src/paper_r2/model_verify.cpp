#include "model_queries.hpp"
#include "model_resources.hpp"
#include <chrono>
#include "models.hpp"
#include "wire.hpp"
#include <iostream>
namespace r1=zkgpt::r1;
using namespace zkgpt::r2;
int main(int argc,char **argv){try{
 r1::init();r1::require(argc>=3,"usage: r2_model_verify APPROVED_STATEMENT PROOF [--blocks 12] [--numerical-profile r4-partial-candidate-v1]");
 unsigned blocks=1;bool candidate=false;
 for(int i=3;i<argc;++i){const std::string flag=argv[i];
  if(flag=="--blocks" && i+1<argc)blocks=parseModelBlocks(argv[++i]);
  else if(flag=="--numerical-profile" && i+1<argc)candidate=parseModelNumericalProfile(argv[++i]);
  else throw std::invalid_argument("unknown model verification option");
 }
 r1::require(!candidate || blocks==1,"R4 full12 BLOCKED until complete numerical prerequisites pass");
 const auto profile=modelProfile(blocks,candidate);
 const auto start=std::chrono::steady_clock::now();const auto progress=[&](const std::string &stage){std::cerr<<"PHASE "<<stage<<" seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<std::endl;};
 auto [g,s]=parseStatement(r1::readFile(argv[1]));
 LLM nn(blocks,12,64,768,3072);nn.setR4NumericalCandidate(candidate);const auto c=nn.buildPublicCircuit();
 auto plan=planModelQueries(c,nn.getWitnessRegistry(),nn.parameterRegions(),r1::ExpProfile::LegacyClampOne);
 r1::require(g.circuit_fingerprint==fingerprintCircuit(c) && g.val0_log_size==r1::log2exact(plan.master.padded) && g.output_evaluation.isZero(),"unapproved block GKR metadata/output");
 const auto &a=g.model_shape,&b=plan.shape;
 r1::require(a.sequence_length==b.sequence_length && a.layer_count==b.layer_count && a.head_count==b.head_count && a.head_dimension==b.head_dimension && a.hidden_dimension==b.hidden_dimension && a.mlp_dimension==b.mlp_dimension,"unapproved block shape");
 r1::require(s.input.public_values.size()>=65536 && !s.input.children.empty(),"missing public X/Y/W");
 std::vector<Fr>x,y;for(std::size_t i=0;i<32768;++i){x.push_back(s.input.public_values[i].value);y.push_back(s.input.public_values[32768+i].value);}
 requireModelQueryStatement(plan,s,s.input.children.back().commitment,x,y,profile);
 auto p=parseProof(g,s,r1::readFile(argv[2]));const auto claims=verifyR2(c,g,s,p,progress);
 r1::require(claims.size()==modelResources(c,plan).terminals,"block input claim coverage");
 std::cout<<"ACCEPT real engineering public constraint system: GKR+5 Range Lasso+indexed clamp1 Exp Lasso+master bridge+unified input consistency+X/Y/W/constants/padding; terminals="<<claims.size()<<" blocks="<<blocks<<". Paper equivalence and publisher provenance unclaimed.\n";return 0;
 }catch(const std::exception&e){std::cerr<<"REJECT "<<e.what()<<'\n';return 2;}}
