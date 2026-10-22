#include "models.hpp"
#include "gkr_proof_builder.hpp"
#include "gkr_verifier.hpp"
#include "paper_r1/exp.hpp"
#include <fstream>
#include <iostream>
#include <chrono>
namespace r1=zkgpt::r1;
int main(int argc,char **argv){try{
 r1::init();if(argc<3)throw std::runtime_error("usage: r2_model_probe BLOCKS REPORT [gkr]");
 int blocks=std::stoi(argv[1]);r1::require(blocks>=1 && blocks<=12,"block count");
 auto start=std::chrono::steady_clock::now();LLM nn(blocks,12,64,768,3072);prover p;nn.create(p,false);
 nn.validateCurrentWitness(p);const auto &registry=nn.getWitnessRegistry();
 std::size_t endpoints=0,fc=0,mismatches=0,range_values=0;
 for(unsigned i=1;i<p.C.size;++i){const auto &l=p.C.circuit[i];endpoints+=(l.bit_length_u[0]>=0)+(l.bit_length_v[0]>=0);if(l.ty==layerType::FCONN){endpoints+=2;++fc;}}
 std::string examples;std::size_t tail=0;
 for(const auto &q:registry.expQueries()){
  const auto &t=p.val[0].at(q.t_witness_index),&e=p.val[0].at(q.E_witness_index);
  auto raw=r1::encode(t);std::uint32_t index=0;for(unsigned i=0;i<4;++i)index|=std::uint32_t(raw[i])<<(8*i);
  for(unsigned i=4;i<32;++i)r1::require(!raw[i],"model t exceeds u32");
  if(index>=8191)++tail;
  if(e!=Fr(r1::expValue(index))){if(mismatches<8){if(!examples.empty())examples+=",";examples+="{\"t_index\":"+std::to_string(q.t_witness_index)+",\"E_index\":"+std::to_string(q.E_witness_index)+",\"t\":"+std::to_string(index)+",\"actual_E\":\""+e.getStr()+"\",\"unclamped_E\":"+std::to_string(r1::expValue(index))+"}";}++mismatches;}
 }
 for(const auto &r:registry.constraints())range_values+=r.count;
 bool gkr_ok=false;std::size_t verified=0;std::string error;
 if(argc>3 && std::string(argv[3])=="gkr"){
  GeneratorDomain d{"zkGPT/main",std::uint32_t(std::size_t(1)<<(p.cc.l-p.cc.l/2))};
  GKRProofBuilder b(&p,p.C,registry.shape(),d);b.build();std::vector<GKRInputTerminalClaim>claims;
  gkr_ok=verifyGKRWithClaims(p.C,b.publicStatement(),b.proof(),claims,&error);verified=claims.size();
  if(!gkr_ok)throw std::runtime_error(error);
 }
 auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
 std::ofstream out(argv[2]);r1::require(bool(out),"report file");
 out<<"{\"blocks\":"<<blocks<<",\"profile\":\"existing-clamp1-not-paper-approved\",\"forward\":true,\"witness\":true,\"layers\":"<<p.C.size<<",\"actual_witness\":"<<p.val[0].size()<<",\"padded_witness\":"<<(std::size_t(1)<<p.cc.l)<<",\"range_regions\":"<<registry.constraints().size()<<",\"range_values\":"<<range_values<<",\"exp_queries\":"<<registry.expQueries().size()<<",\"tail_queries\":"<<tail<<",\"unclamped_exp_mismatches\":"<<mismatches<<",\"examples\":["<<examples<<"],\"gkr_attempted\":"<<(argc>3?"true":"false")<<",\"gkr_accepted\":"<<(gkr_ok?"true":"false")<<",\"reconstructed_terminals\":"<<endpoints<<",\"verified_terminals\":"<<verified<<",\"fconn_layers\":"<<fc<<",\"seconds\":"<<seconds<<",\"integrated_paper_proof_accepted\":false}";
 std::cout<<"MODEL blocks="<<blocks<<" terminals="<<endpoints<<" Exp-mismatches="<<mismatches<<" GKR="<<gkr_ok<<" seconds="<<seconds<<'\n';
 return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
