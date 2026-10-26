#include "model_queries.hpp"
#include "models.hpp"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <filesystem>
namespace r1=zkgpt::r1;
int main(int argc,char **argv){try{
 r1::init();r1::require(argc==2,"usage: r3_model_semantics_probe NEW_JSON");r1::require(!std::filesystem::exists(argv[1]),"probe destination already exists");std::ofstream o(argv[1]);r1::require(o.good(),"cannot open semantics evidence destination");LLM nn(1,12,64,768,3072);prover p;nn.create(p,false);nn.validateCurrentWitness(p);
 const auto &reg=nn.getWitnessRegistry();std::size_t exp=0,mismatch=0,pmaxdiff=0,rows=0,ln=0,ties=0;
 for(const auto &q:reg.expQueries()){const auto raw=r1::encode(p.val[0][q.t_witness_index]);std::uint32_t t=0;for(unsigned b=0;b<4;++b)t|=std::uint32_t(raw[b])<<(8*b);for(unsigned b=4;b<32;++b)r1::require(!raw[b],"actual t domain");const auto &E=p.val[0][q.E_witness_index];r1::require(E==Fr(r1::expValue(t,r1::ExpProfile::LegacyClampOne)),"actual clamp Exp mismatch");mismatch+=E!=Fr(r1::expValue(t));++exp;}
 const auto base=reg.expQueries().front().t_witness_index-10-2*12*32-2*6336;
 for(std::size_t l=1;l<p.C.circuit.size();++l)if(p.C.circuit[l].ty==layerType::SOFTMAX_1){for(unsigned h=0;h<12;++h)for(unsigned i=1;i<=32;++i){auto actual=convert(p.val[l-1][h*528+i*(i-1)/2]);for(unsigned j=1;j<i;++j)actual=std::max(actual,convert(p.val[l-1][h*528+i*(i-1)/2+j]));const auto expected=std::max(actual,static_cast<__int128>(0));r1::require(p.val[0][base+10+384+h*32+i-1]==Fr(expected),"actual pmax semantics changed");pmaxdiff+=actual<0;++rows;}}
 for(const auto &q:reg.constraints()){
  if(q.kind==WitnessKind::LAYER_NORM && q.name.size()>2 && q.name.substr(q.name.size()-2)=="_B")for(std::size_t i=0;i<q.count;++i){auto B=convert(p.val[0][q.val0_offset+i]),s=convert(p.val[0][q.val0_offset+q.count+i]);r1::require(B>=1 && s>=1 && B>=s*s-s+1 && B<=s*s+s,"actual LN sqrt changed");++ln;}
  if(q.kind==WitnessKind::ROUNDING && (q.name.find("_term1")!=std::string::npos || q.name.find("_term2")!=std::string::npos))for(std::size_t i=0;i<q.count;++i)ties+=p.val[0][q.val0_offset+i].isZero();
 }
 r1::require(exp==6336 && mismatch==4556 && rows==384 && ln==64,"R2 actual semantics baseline counts");
 o<<"{\"blocks\":1,\"Exp_checked\":"<<exp<<",\"strict_unclamped_mismatches\":"<<mismatch<<",\"pmax_rows\":"<<rows<<",\"all_negative_rows_pmax_differs_true_max\":"<<pmaxdiff<<",\"positive_LN_sqrt_rows\":"<<ln<<",\"round_slack_zero_coordinates_including_padding\":"<<ties<<",\"paper_equivalent\":false}";
 o.close();r1::require(!o.fail(),"failed to save semantics evidence");
 std::cout<<"PASS actual legacy numerical profile; pmax/true-max differing rows="<<pmaxdiff<<std::endl;return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
