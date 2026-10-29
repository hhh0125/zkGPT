#include "crypto.hpp"
#include <mcl/window_method.hpp>
#include <iostream>
#include <chrono>
#include <array>
using namespace zkgpt::r7::crypto;
int main(int argc,char**argv){try{r1::init();unsigned mode=argc>1?std::stoul(argv[1]):0;need(mode<4,"benchmark mode");auto p=parameters(1);mcl::fp::WindowMethod<G1>g,h;auto setup=processCPU();g.init(p.scalar,Fr::getBitSize(),8);h.init(p.hiding,Fr::getBitSize(),8);auto setupCPU=processCPU()-setup;
 // Deterministic, full-width public scalars. Production entropy is untouched.
 std::array<Fr,256>a,b;for(unsigned i=0;i<a.size();++i){auto d=r1::hash(Bytes{std::uint8_t(i),17,93});a[i].setLittleEndianMod(d.data(),d.size());d=r1::hash(Bytes{std::uint8_t(i),73,29});b[i].setLittleEndianMod(d.data(),d.size());}a[0]=0;b[1]=0;a[2]=Fr(-1);b[3]=Fr(-1);
 auto evaluate=[&](unsigned m,const Fr&v,const Fr&r){G1 out,x,y;if(m==0){std::vector<G1>bases{p.scalar,p.hiding};std::vector<Fr>s{v,r};G1::mulVec(out,bases.data(),s.data(),2);}else if(m==1){G1::mul(x,p.scalar,v);G1::mul(y,p.hiding,r);G1::add(out,x,y);}else if(m==2){g.mul(x,v);h.mul(y,r);G1::add(out,x,y);}else{std::array<G1,2>bases{p.scalar,p.hiding};std::array<Fr,2>s{v,r};G1::mulVec(out,bases.data(),s.data(),2);}return out;};
 for(unsigned i=0;i<a.size();++i)need(evaluate(0,a[i],b[i])==evaluate(1,a[i],b[i]) && evaluate(0,a[i],b[i])==evaluate(2,a[i],b[i]) && evaluate(0,a[i],b[i])==evaluate(3,a[i],b[i]),"scalarCommit full-width equivalence");
 auto started=std::chrono::steady_clock::now();auto cpu=processCPU();G1 checksum;checksum.clear();for(unsigned i=0;i<8192;++i){MSMTrace trace(2);checksum+=evaluate(mode,a[i%256],b[i%256]);}
 std::cout<<"{\"mode\":"<<mode<<",\"iterations\":8192,\"cpu_seconds\":"<<processCPU()-cpu<<",\"wall_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<",\"setup_cpu_seconds\":"<<setupCPU<<",\"equivalence_cases\":256,\"point\":\""<<hex(r1::hash(r1::encode(checksum)))<<"\"}\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
