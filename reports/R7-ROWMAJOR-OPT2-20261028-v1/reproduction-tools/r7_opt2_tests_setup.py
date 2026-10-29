from pathlib import Path
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd().resolve()==R
P=R/'src/paper_r7'
(P/'opt2_kernel_tests.cpp').write_text(r'''#include "classified_commitments.hpp"
#include <iostream>
#include <chrono>
using namespace zkgpt::r7;using namespace oneblock;using namespace classified;
namespace zkgpt::r7::oneblock::classified {
struct AuthenticationTestAccess {
 static void setup(Commitments&c,std::size_t n){
  c.roots_.resize(6);for(std::size_t k=0;k<6;++k){c.roots_[k].spec={"test/"+std::to_string(k),n,n,false,false};c.roots_[k].value=[k](std::size_t j){return Fr(1+j%29+k);};}
  for(unsigned i=0;i<48;++i){Claim a;a.id="test/"+std::to_string(i);a.padded=n;for(unsigned k=0;k<r1::log2exact(n);++k)a.point.push_back(Fr(3+(i+k)%37));a.only_root=i<24?int(i%6):-1;
   a.map=[i,n](std::size_t j)->Located{if(i<24)return {{i%6,std::uint32_t(j)},Fr(0),false};if(j>=n-16)return {{},Fr(7),true};return {{std::uint32_t((j/(n/8)+i)%6),std::uint32_t(j)},Fr(int(i%5)-2),false};};
   a.map_digest=c.mapDigest(a);c.claims_.push_back(std::move(a));
  }
 }
 static void run(Commitments&c,bool bench){
  std::vector<Fr>lambda;for(unsigned i=0;i<c.claims_.size();++i)lambda.push_back(Fr(i+2));
  auto start=std::chrono::steady_clock::now();auto cpu=processCPU();Fr checksum=0;
  for(unsigned pass=0;pass<(bench?2u:1u);++pass)for(unsigned k=0;k<6;++k){auto got=c.kernel(k,lambda);if(!bench){std::vector<Fr>want(got.size(),Fr(0));for(unsigned i=0;i<c.claims_.size();++i){auto&a=c.claims_[i];auto w=r1::weights(a.point);for(std::size_t j=0;j<a.padded;++j){auto l=a.map(j);if(!l.constant && l.coordinate.root==k)want[l.coordinate.index]+=lambda[i]*w[j];}}need(want==got,"kernel dense reference mismatch");}checksum+=got[0];}
  std::cout<<"{\"test\":\"kernel\",\"mode\":\"reference\",\"n\":"<<c.roots_[0].spec.padded<<",\"wall_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<",\"cpu_seconds\":"<<processCPU()-cpu<<",\"checksum\":\""<<checksum.getStr()<<"\"}\n";
 }
};
}
struct Tape:IO {bool prover()const override{return true;}std::string session()const override{return std::string(64,'a');}Digest prefix()const override{return {};}Bytes message(const std::string&,const Bytes&b={})override{return b;}Fr coin(const std::string&)override{return Fr(3);}};
int main(int argc,char**argv){try{r1::init();bool bench=argc>1 && std::string(argv[1])=="bench";Layout l;l.native=std::string(64,'a');l.roots={{"C_aux",1,1,true,false}};l.base={{0,0}};Prepared p;SessionRandom rng;Tape t;Commitments c(t,l,[](std::size_t){return Fr(1);},&p,&rng);AuthenticationTestAccess::setup(c,bench?65536:256);AuthenticationTestAccess::run(c,bench);return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
''')
(P/'opt2_msm_bench.cpp').write_text(r'''#include "crypto.hpp"
#include <mcl/window_method.hpp>
#include <iostream>
#include <chrono>
#include <array>
using namespace zkgpt::r7::crypto;
int main(int argc,char**argv){try{r1::init();unsigned mode=argc>1?std::stoul(argv[1]):0;need(mode<3,"benchmark mode");auto p=parameters(1);mcl::fp::WindowMethod<G1>g,h;auto setup=processCPU();g.init(p.scalar,Fr::getBitSize(),8);h.init(p.hiding,Fr::getBitSize(),8);auto setupCPU=processCPU()-setup;
 // Deterministic, full-width public scalars. Production entropy is untouched.
 std::array<Fr,256>a,b;for(unsigned i=0;i<a.size();++i){auto d=r1::hash(Bytes{std::uint8_t(i),17,93});a[i].setLittleEndianMod(d.data(),d.size());d=r1::hash(Bytes{std::uint8_t(i),73,29});b[i].setLittleEndianMod(d.data(),d.size());}a[0]=0;b[1]=0;a[2]=Fr(-1);b[3]=Fr(-1);
 auto evaluate=[&](unsigned m,const Fr&v,const Fr&r){G1 out,x,y;if(m==0){std::vector<G1>bases{p.scalar,p.hiding};std::vector<Fr>s{v,r};G1::mulVec(out,bases.data(),s.data(),2);}else if(m==1){G1::mul(x,p.scalar,v);G1::mul(y,p.hiding,r);G1::add(out,x,y);}else{g.mul(x,v);h.mul(y,r);G1::add(out,x,y);}return out;};
 for(unsigned i=0;i<a.size();++i)need(evaluate(0,a[i],b[i])==evaluate(1,a[i],b[i]) && evaluate(0,a[i],b[i])==evaluate(2,a[i],b[i]),"scalarCommit full-width equivalence");
 auto started=std::chrono::steady_clock::now();auto cpu=processCPU();G1 checksum;checksum.clear();for(unsigned i=0;i<8192;++i)checksum+=evaluate(mode,a[i%256],b[i%256]);
 std::cout<<"{\"mode\":"<<mode<<",\"iterations\":8192,\"cpu_seconds\":"<<processCPU()-cpu<<",\"wall_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<",\"setup_cpu_seconds\":"<<setupCPU<<",\"equivalence_cases\":256,\"point\":\""<<hex(r1::hash(r1::encode(checksum)))<<"\"}\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
''')
cm=P/'CMakeLists.txt';cm.write_text(cm.read_text()+'''
add_executable(r7_opt2_kernel_tests opt2_kernel_tests.cpp)
target_link_libraries(r7_opt2_kernel_tests PRIVATE paper_r7)
add_test(NAME r7_opt2_kernel_dense COMMAND r7_opt2_kernel_tests)
add_executable(r7_opt2_msm_bench opt2_msm_bench.cpp)
target_link_libraries(r7_opt2_msm_bench PRIVATE paper_r7)
''')
print('DIRECTED TESTS WRITTEN')
