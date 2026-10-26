#include "r6_model.hpp"
#include "gkr_protocol.hpp"
#include <fstream>
#include <iostream>
using namespace zkgpt;
int main(int argc,char**argv){try{r1::init();unsigned blocks=argc>1?r2::parseModelBlocks(argv[1]):12;LLM n(blocks,12,64,768,3072);r6::configure(n);auto c=n.buildPublicCircuit();auto p=r2::planModelQueries(c,n.getWitnessRegistry(),n.parameterRegions(),r1::ExpProfile::LegacyClampOne);r6::auditGraph(c,n,p);
 if(blocks==1)r1::require(p.circuit_id=="1cf3bc83a9a9f289e540238255d467b7fa6e2801209aa00f07fe232c48635a9a","frozen R5B oneblock circuit changed");
 if(argc>2){std::ifstream existing(argv[2]);r1::require(!existing.good(),"refuse existing report");std::ofstream out(argv[2]);r6::writePlan(out,c,n,p);}
 auto &g=c.circuit[n.blockConnections().back().last_layer].uni_gates[0];g.sc=-1;bool rejected=false;try{r6::auditGraph(c,n,p);}catch(...){rejected=true;}r1::require(rejected,"accepts wrong residual branch coefficient");
 bool guarded=false;try{LLM old(12,12,64,768,3072);old.setR5BEngineeringCandidate();old.buildPublicCircuit();}catch(const std::exception&e){guarded=std::string(e.what()).find("blocks=1 only")!=std::string::npos;}r1::require(guarded,"old R5B full12 guard removed");std::cout<<"PASS actual graph,all block/FC/Range/Exp coordinates,public final output,zero metadata witness,residual tamper rejection and old guard; fingerprint="<<p.circuit_id<<"\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
