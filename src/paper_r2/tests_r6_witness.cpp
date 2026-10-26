#include "models.hpp"
#include "paper_r1/core.hpp"
#include <fstream>
#include <iostream>
using namespace zkgpt;
int main(){try{r1::init();LLM n(3,12,64,768,3072);n.setR6EngineeringCandidate();prover pr{};n.createFiniteWitness(pr);n.validateCurrentWitness(pr);r1::require(pr.cc.w==nullptr,"finite witness committed master unexpectedly");auto&e=n.blockConnections();r1::require(e.size()==3 && e[1].input==e[0].output && e[2].input==e[1].output,"finite block connection");
 for(auto&x:e){for(std::size_t j=0;j<32768;++j){auto v=convert(pr.val[0][x.output+j]);r1::require(fitsSignedBits(v,64),"finite accumulated output domain");if(j%1024>=768)r1::require(v==0,"finite output padding");}}
 pr.val[0][e[1].input]+=Fr(1);bool rejected=false;try{n.validateCurrentWitness(pr);}catch(...){rejected=true;}r1::require(rejected,"crossblock witness mutation accepted");std::cout<<"PASS real blocks0->1->2 numeric witness,ranges,padding,tampered boundary rejected; NO master commitment or proof\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
