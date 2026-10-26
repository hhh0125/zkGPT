#include "models.hpp"
#include "r5b_numeric.hpp"
#include "gkr_proof_builder.hpp"
#include "gkr_verifier.hpp"
#include "paper_r1/core.hpp"
#include "utils.hpp"
#include <iostream>
namespace r1=zkgpt::r1;
class Fixture:public LLM {
public:
 Fixture():LLM(1,1,4,4,4){setR5BEngineeringCandidate();total_in_size=total_para_size=total_relu_in_size=total_ave_in_size=total_max_in_size=0;}
 void prepare(prover &p){SIZE=9;len=1;channel_out=4;p.C.init(0,SIZE);p.val.resize(SIZE);val=p.val.begin();p.val[0].assign(32,Fr(0));p.val[0][0]=3;const int weights[4]={512,-512,256,-256};for(int j=0;j<4;++j)p.val[0][4+4*j]=weights[j];
  for(int fc=0;fc<4;++fc){int li=1+2*fc;auto &matrix=p.C.circuit[li];initLayer(matrix,4,layerType::FCONN);matrix.need_phase2=true;for(int j=0;j<4;++j)matrix.ori_id_u.push_back(j);for(int j=0;j<16;++j)matrix.ori_id_v.push_back(4+j);matrix.size_u[0]=4;matrix.size_v[0]=16;p.val[li].resize(4);for(int j=0;j<4;++j)p.val[li][j]=3*weights[j];
   p.fc_input_row[fc]=1;p.fc_input_col[fc]=p.fc_row[fc]=p.fc_col[fc]=4;p.fc_input_id[fc]=0;matrix_storage_[fc].reset(new int[16]{});mat_values[fc]=matrix_storage_[fc].get();for(int j=0;j<4;++j)mat_values[fc][4*j]=weights[j];i64 id=li+1;r5bRound(p.C.circuit[li+1],id,-1);
   for(int j=0;j<4;++j)r1::require(p.val[0][q_offset+j]==Fr(j==0?2:-1+(j==2?2:0)),"actual R5B FC half-tie output");
  }
  initLayer(p.C.circuit[0],p.val[0].size(),layerType::INPUT);p.C.initSubset();p.mat_val=mat_values;std::size_t count=std::size_t(1)<<(p.C.circuit[0].bit_length-p.C.circuit[0].bit_length/2);p.gens.resize(count);auto u=gen_gi(p.gens.data(),count);p.gens.push_back(u);p.commitInput(p.gens,1);
 }
};
int main(){try{r1::init();Fixture f;prover p;f.prepare(p);const int bits=p.C.circuit[0].bit_length;GeneratorDomain domain{"zkGPT/main",std::uint32_t(std::size_t(1)<<(bits-bits/2))};GKRProofBuilder b(&p,p.C,WitnessShape{1,1,1,4,4,4},domain);b.build();std::vector<GKRInputTerminalClaim>claims;std::string error;r1::require(verifyGKRWithClaims(p.C,b.publicStatement(),b.proof(),claims,&error),error);std::size_t matrix=0;for(auto &q:claims)matrix+=p.C.circuit[q.layer_index].ty==layerType::FCONN;r1::require(matrix==16,"both points of all four FCs must be authenticated");std::cout<<"PASS actual R5B rounding with four FC GKR parents, signed half-ties and 16 authenticated matrix terminal claims\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
