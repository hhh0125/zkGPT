#include "source_plan.hpp"
#include "models.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>

using namespace route_b::production;
namespace {
std::filesystem::path origins_path;
// Test-only provenance capture. The public builder's original access sequence
// is retained BEFORE generic subset mutation. No expected-plan function is used.
struct Writer {
  std::ofstream out;std::array<unsigned char,65536> buffer{};std::size_t used=0;
  explicit Writer(const std::filesystem::path&p):out(p,std::ios::binary){require(bool(out),"cannot create raw gate evidence");}
  void flush(){out.write(reinterpret_cast<const char*>(buffer.data()),used);used=0;require(bool(out),"raw gate evidence write failure");}
  void u64(std::uint64_t v){for(unsigned j=0;j<8;++j){if(used==buffer.size())flush();buffer[used++]=static_cast<unsigned char>(v>>(8*j));}}
  void u32(std::uint32_t v){for(unsigned j=0;j<4;++j){if(used==buffer.size())flush();buffer[used++]=static_cast<unsigned char>(v>>(8*j));}}
};
}
extern "C" void __real__ZN14layeredCircuit10initSubsetEv(layeredCircuit*);
extern "C" void __wrap__ZN14layeredCircuit10initSubsetEv(layeredCircuit*C) {
  require(!origins_path.empty(),"origin capture path unset");
  Writer w(origins_path);w.u64(0x31304749524f4252ULL);w.u64(C->size-1);
  for(unsigned i=1;i<C->size;++i){const auto &l=C->circuit[i];w.u64(i);w.u64(static_cast<unsigned>(l.ty));w.u64(l.size);w.u64(l.zero_start_id);
    for(unsigned port=0;port<2;++port){std::size_t count=0;
      if(l.ty==layerType::FCONN)count=port?l.ori_id_v.size():l.ori_id_u.size();
      else {if(!port)for(auto&g:l.uni_gates)if(!g.lu)++count;for(auto&g:l.bin_gates)if(!(port?g.getLayerIdV(i):g.getLayerIdU(i)))++count;}
      w.u64(count);
      if(l.ty==layerType::FCONN)for(auto x:port?l.ori_id_v:l.ori_id_u)w.u32(x);
      else {if(!port)for(auto&g:l.uni_gates)if(!g.lu)w.u32(g.u);for(auto&g:l.bin_gates)if(!(port?g.getLayerIdV(i):g.getLayerIdU(i)))w.u32(port?g.v:g.u);}
    }
  }w.flush();w.out.close();__real__ZN14layeredCircuit10initSubsetEv(C);
}
int main(int argc,char**argv){try{
  require(argc==2,"usage: route_b_source_plan_emit NEW_OUTPUT_DIRECTORY");
  const std::filesystem::path out=argv[1];require(!std::filesystem::exists(out),"refuse to overwrite output directory");std::filesystem::create_directories(out);origins_path=out/"gate-origins.bin";
  initPairing(mcl::BN254);auto deployment=TrustedDeployment::full12(Mode::BridgeV1);
  LLM n(12,12,64,768,3072);const auto C=n.buildPublicCircuit();
  const auto l=rebuildProductionLayout(deployment,C,n.getWitnessRegistry());
  {std::ofstream f(out/"layout.json");writeLayout(f,l,C);require(bool(f),"layout write failed");}
  for(auto mode:{Mode::BridgeV1,Mode::LassoV2}){deployment.mode=mode;auto p=buildExpectedPlan(deployment,l,C,n.getWitnessRegistry());
    std::ofstream f(out/(mode==Mode::BridgeV1?"bridge.json":"lasso.json"));writePlan(f,p);require(bool(f),"plan write failed");
    std::cout<<"EMIT mode="<<static_cast<unsigned>(mode)<<" sources="<<p.sources.size()<<" obligations="<<p.obligations.size()<<'\n';
  }
  std::cout<<"EMIT full12 real public circuit; no private model values; no inference; no proof; challenges=0\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
