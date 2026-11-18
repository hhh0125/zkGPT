#include "paper_r2_model_queries.hpp"
#include "paper_r1_wire.hpp"
#include <iostream>
using namespace zkgpt;
int main(){try{
 r1::init();WitnessRegistry r;r.setShape({32,1,12,64,768,3072});r.setR5BEngineeringCandidate(true);r.setR6Engineering(true);
 r.addRange(WitnessKind::ROUNDING,"input",0,3,64,true);
 r.addRange(WitnessKind::ROUNDING,"next",3,2,64,true);
 r.addRange(WitnessKind::LAYER_NORM,"layer_norm_0_sigma",40,2,64,false);
 r.addExpQueries(0,50,60,3);r.addExpQueries(1,53,63,2);
 r.recordParameter(70,3);r.recordParameter(73,2);r.recordConstants(20,10);r.recordOutput(90,4);
 layeredCircuit c;c.circuit.resize(1);c.circuit[0].size=100;c.circuit[0].bit_length=7;
 auto p=r2::finalizeModelQueries(c,r,r1::ExpProfile::LegacyClampOne);
 r1::require(p.sources.size()==5 && p.ranges.size()==2,"range/source order");
 r1::require(p.sources[0].spans.size()==2 && p.sources[0].spans[1].bias==Fr(-1),"sigma positive query");
 auto&s=p.sources[1];Fr half=1;for(unsigned i=1;i<64;++i)half*=Fr(2);
 r1::require(s.spec.actual==5 && s.spec.padded==8 && s.spans.size()==1 && s.spans[0].bias==half,"signed merge and padding");
 r1::require(p.exps[0].index_child==2 && p.exps[0].value_child==3 && p.sources[2].spans.size()==1 && p.sources[2].spec.actual==5,"Exp mapping");
 r1::require(p.sources.back().spans.size()==1 && p.sources.back().spec.actual==5,"W merge");
 r1::require(p.constants==std::vector<std::size_t>{20} && p.reserved_zero.size()==9 && p.reserved_zero.back()==29 && p.output==std::vector<std::size_t>({90,91,92,93}),"public positions");
 std::cout<<"PASS incremental metadata, signed bias, sigma constraint, Exp pairing, parameter merge, public constants/output\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
