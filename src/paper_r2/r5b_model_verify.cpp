#include "r5b_cli.hpp"
#include "models.hpp"
#include "wire.hpp"
#include <iostream>
#include <chrono>
namespace r1=zkgpt::r1;using namespace zkgpt;
int main(int argc,char**argv){try{r1::init();r1::require(argc>=3,"usage: r5b_model_verify STATEMENT PROOF --trusted-anchor FILE --anchor-sha256 INDEPENDENT_HEX --model-manifest FILE");auto opt=r5b::options(argc,argv,3,false);auto start=std::chrono::steady_clock::now();auto progress=[&](const std::string&s){std::cerr<<"PHASE "<<s<<" seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<std::endl;};
 auto[g,s]=r2::parseStatement(r1::readFile(argv[1]));LLM nn(1,12,64,768,3072);nn.setR5BEngineeringCandidate();auto c=nn.buildPublicCircuit();auto plan=r2::planModelQueries(c,nn.getWitnessRegistry(),nn.parameterRegions(),r1::ExpProfile::LegacyClampOne);auto anchor=r5b::loadAnchor(opt.anchor,opt.sha,opt.manifest,plan);
 r1::require(g.circuit_fingerprint==fingerprintCircuit(c) && g.val0_log_size==r1::log2exact(plan.master.padded) && g.output_evaluation.isZero(),"R5B unapproved GKR circuit/output");const auto&a=g.model_shape,&b=plan.shape;r1::require(a.sequence_length==b.sequence_length && a.layer_count==b.layer_count && a.head_count==b.head_count && a.head_dimension==b.head_dimension && a.hidden_dimension==b.hidden_dimension && a.mlp_dimension==b.mlp_dimension,"R5B model shape mismatch");
 r1::require(s.input.public_values.size()>=65536 && !s.input.children.empty(),"R5B missing public X/Y/W");std::vector<Fr>x,y;for(std::size_t i=0;i<32768;++i){x.push_back(s.input.public_values[i].value);y.push_back(s.input.public_values[32768+i].value);}r5b::requireAnchorW(anchor,s.input.children.back().commitment);r2::requireModelQueryStatement(plan,s,anchor.w,x,y,anchor.model_id);progress("external-operator-W-and-layout/accepted");
 auto p=r2::parseProof(g,s,r1::readFile(argv[2]));auto claims=r2::verifyR2(c,g,s,p,progress);r1::require(claims.size()==r2::modelResources(c,plan).terminals,"R5B incomplete input terminal coverage");
 std::cout<<"ACCEPT R5B engineering candidate blocks=1: GKR + "<<s.ranges.size()<<" production Range Lasso + indexed clamp1 Exp Lasso + bridge + unified input consistency; external OPERATOR-TRUSTED W; terminals="<<claims.size()<<". APPROVED-PROFILE/PAPER-EQUIVALENT/publisher certification/security audit unclaimed.\n";return 0;
 }catch(const std::exception&e){std::cerr<<"REJECT "<<e.what()<<'\n';return 2;}}
