#include "model_queries.hpp"
#include "model_resources.hpp"
#include "models.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
namespace r1=zkgpt::r1;
using namespace zkgpt::r2;
int main(int argc,char **argv){try{
 r1::init();r1::require(argc==2 && !std::filesystem::exists(argv[1]),"usage: r4_model_numerical_probe NEW_JSON");
 LLM nn(1,12,64,768,3072);nn.setR4NumericalCandidate();prover pr;nn.create(pr,false);nn.validateCurrentWitness(pr);
 const auto &reg=nn.getWitnessRegistry();auto p=planModelQueries(pr.C,reg,nn.parameterRegions(),r1::ExpProfile::LegacyClampOne);
 std::vector<Fr> master(pr.cc.w,pr.cc.w+p.master.padded);auto children=gatherModelQueries(p,master);
 std::size_t checks=0;
 for(const auto &r:p.ranges)for(std::size_t i=0;i<p.sources.at(r.child).spec.actual;++i){
  const auto bytes=r1::encode(children.at(r.child).at(i));
  for(unsigned bit=r.bits;bit<256;++bit)r1::require(!(bytes[bit/8]&(1U<<(bit%8))),"candidate actual shifted Range witness");
  ++checks;
 }
 for(const auto &q:reg.expQueries()){
  const auto t=convert(master.at(q.t_witness_index));
  r1::require(t>=0 && t<=0xffffffff && master.at(q.E_witness_index)==Fr(r1::expValue(std::uint32_t(t),r1::ExpProfile::LegacyClampOne)),"candidate complete indexed Exp witness");
 }
 auto find=[&](const std::string &name)->const RangeConstraint &{for(const auto &r:reg.constraints())if(r.name==name)return r;throw std::runtime_error(name);};
 const auto &m=find("softmax_0_pmax"),&s=find("softmax_0_maximum_selector"),&q=find("softmax_0_maximum_score"),&d=find("softmax_0_delta1");
 std::size_t rows=0,negative=0;
 for(unsigned h=0;h<12;++h)for(unsigned i=1;i<=32;++i){
  const auto start=h*528+i*(i-1)/2;auto maximum=convert(master.at(q.val0_offset+start));Fr sum=0,attained=0;
  for(unsigned j=0;j<i;++j){const auto at=start+j;maximum=std::max(maximum,convert(master.at(q.val0_offset+at)));sum+=master.at(s.val0_offset+at);attained+=master.at(s.val0_offset+at)*master.at(d.val0_offset+at);}
  r1::require(master.at(m.val0_offset+h*32+i-1)==Fr(maximum) && sum==Fr(1) && attained.isZero(),"candidate actual true row maximum");negative+=maximum<0;++rows;
 }
 auto resources=modelResources(pr.C,p);std::ofstream out(argv[1]);r1::require(bool(out),"probe output");
 out<<"{\"profile\":\""<<modelProfile(1,true)<<"\",\"circuit_id\":\""<<p.circuit_id<<"\",\"blocks\":1,\"master_actual\":"<<p.master.actual<<",\"master_padded\":"<<p.master.padded<<",\"registered_range_regions\":"<<reg.constraints().size()<<",\"complete_Range_values_checked\":"<<checks<<",\"Exp_values_checked\":"<<reg.expQueries().size()<<",\"maximum_rows_checked\":"<<rows<<",\"negative_maximum_rows\":"<<negative<<",\"expected_terminals\":"<<resources.terminals<<",\"prover_budget_bytes\":"<<resources.prover_peak_bytes<<",\"public_embedding_environment_set\":"<<(std::getenv("ZKGPT_PUBLIC_EMBEDDING")?"true":"false")<<",\"full_proof_verified\":false,\"paper_equivalent\":false}";
 out.close();r1::require(!out.fail(),"probe report write failed");
 std::cout<<"PASS actual candidate witness: rows="<<rows<<" negative="<<negative<<" Range="<<checks<<" Exp="<<reg.expQueries().size()<<std::endl;return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
