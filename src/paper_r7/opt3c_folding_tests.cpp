#define main classified_fixture_main
#include "classified_crypto_tests.cpp"
#undef main
#include "unified_input.hpp"
#include "public_k.hpp"
#include "fs_io.hpp"
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
namespace kFault {enum Mode{normal,shortIO,earlyEOF,corrupt,writeError};thread_local Mode mode=normal;thread_local unsigned reads=0,writes=0;thread_local std::uint64_t dirty=0,maxDirty=0;}
// Fix ONLY the FS header entropy draw in this test binary. Production uses
// getrandom unchanged; this is not linked into any prover/verifier executable.
thread_local bool fixedHeaderNonce=false;
extern "C" ssize_t getrandom(void*p,size_t n,unsigned flags){if(fixedHeaderNonce){need(n==32,"OPT3C test header nonce length");for(std::size_t j=0;j<n;++j)static_cast<unsigned char*>(p)[j]=std::uint8_t(j+17);fixedHeaderNonce=false;return n;}return ::syscall(SYS_getrandom,p,n,flags);}
extern "C" ssize_t pread(int fd,void*p,size_t n,off_t at){auto call=kFault::reads++;if(kFault::mode==kFault::shortIO && call==0){errno=EINTR;return -1;}if(kFault::mode==kFault::earlyEOF)return 0;if(kFault::mode==kFault::shortIO)n=std::min(n,size_t(37));auto result=::syscall(SYS_pread64,fd,p,n,at);if(kFault::mode==kFault::corrupt && result>0)static_cast<unsigned char*>(p)[0]^=1;return result;}
extern "C" ssize_t pwrite(int fd,const void*p,size_t n,off_t at){auto call=kFault::writes++;if(kFault::mode==kFault::shortIO && call==0){errno=EINTR;return -1;}if(kFault::mode==kFault::writeError && call>0){errno=EIO;return -1;}if(kFault::mode==kFault::shortIO || kFault::mode==kFault::writeError)n=std::min(n,size_t(37));auto result=::syscall(SYS_pwrite64,fd,p,n,at);if(result>0){kFault::dirty+=result;kFault::maxDirty=std::max(kFault::maxDirty,kFault::dirty);}return result;}
extern "C" int fdatasync(int fd){auto result=::syscall(SYS_fdatasync,fd);if(result==0)kFault::dirty=0;return result;}
namespace ui=zkgpt::r7::oneblock::unified;
namespace fs=zkgpt::r7::fs;
namespace zkgpt::r7::oneblock::classified {
struct AuthenticationTestAccess {static const AuthMetrics&metrics(const Commitments&c){return c.auth_metrics_;}};
}
void kernelCallContract(){
 auto l=small();Prepared data;data["unit/native-E"]=std::make_shared<std::vector<Fr>>(8,Fr(7));
 Tape p(true);SessionRandom rng;Commitments pcs(p,l,[](std::size_t j){return Fr(j+1);},&data,&rng);
 pcs.constrainBaseTails();auto view=pcs.view("unit/native-E");std::vector<Fr>q{Fr(3),Fr(5),Fr(7)};
 auto h=exchangeHidden(p,"opt3c/value",Fr(7),&rng);view.open(p,{q,h.handle},&h,&rng,"opt3c/query");
 pcs.authenticateUnifiedInput({Fr(11),Fr(13)});
 const auto&m=AuthenticationTestAccess::metrics(pcs);
 need(m.kernel_calls==l.roots.size(),"OPT3C regression: public K rebuilt after initialization");
 std::cout<<"OPT3C_KERNEL_CALL_CONTRACT roots="<<l.roots.size()<<" calls="<<m.kernel_calls<<'\n';
}
void states(){
 for(unsigned mode=0;mode<4;++mode)for(unsigned variant=0;variant<3;++variant){
  auto p=ui::pack({{1,16384},{2,2},{3,16},{2,8},{3,1}},4);
  std::vector<std::vector<Fr>>a(p.segments.size()),b(a.size());std::vector<Fr>da(p.domain,Fr(0)),db(da.size());
  for(std::size_t k=0;k<a.size();++k){auto s=p.segments[k];a[k].resize(s.padded);b[k].resize(s.padded);for(std::size_t j=0;j<s.padded;++j){a[k][j]=Fr(int(j%71)-29+int(k));b[k][j]=k && j+3<s.padded?Fr(int((j*7+k)%79)-39):Fr(0);da[s.offset+j]=a[k][j];db[s.offset+j]=b[k][j];}}
  std::vector<unsigned>calls(a.size());ui::KBudget cap=mode==0?ui::KBudget{8ull<<20,0}:mode==1?ui::KBudget{0,8ull<<20}:mode==2?ui::KBudget{128,16384}:ui::KBudget{0,0};
  ui::PublicK state(p.segments,[&](std::size_t k){++calls[k];return b[k];},cap,"state-test");
  ui::Factors reference=[&](std::size_t k,const ui::Consume&c){c([&,k](std::size_t j){return a[k].at(j);},[&,k](std::size_t j){return b[k].at(j);});};
  ui::Factors optimized=[&](std::size_t k,const ui::Consume&c){state.withRoot(k,"state-test",[&](const Value&v){c([&,k](std::size_t j){return a[k].at(j);},v);});};
  std::vector<Fr>point;std::size_t current=p.domain;
  while(current>1){
   // Compare every local coordinate BEFORE each round against the independent
   // dense global state, even after unequal roots collide at high selector bits.
   for(std::size_t k=0;k<a.size();++k)state.withRoot(k,"state-test",[&](const Value&v){auto want=b[k];std::size_t length=want.size();for(auto r:point){if(length==1)break;for(std::size_t z=0;z<length/2;++z)want[z]=want[2*z]*(Fr(1)-r)+want[2*z+1]*r;length/=2;}for(std::size_t j=0;j<length;++j)need(v(j)==want[j],"OPT3C local K state differs before/after challenge");});
   std::array<Fr,3>ev{Fr(0),Fr(0),Fr(0)};for(unsigned x=0;x<3;++x)for(std::size_t j=0;j<current/2;++j)ev[x]+=(da[2*j]*(Fr(1)-Fr(x))+da[2*j+1]*Fr(x))*(db[2*j]*(Fr(1)-Fr(x))+db[2*j+1]*Fr(x));
   std::array<Fr,3>want;want[0]=ev[0];want[2]=(ev[2]-Fr(2)*ev[1]+ev[0])/Fr(2);want[1]=ev[1]-want[0]-want[2];
   auto old=ui::quadraticRound(p.domain,p.segments,point,reference),got=ui::quadraticRoundFolded(p.domain,p.segments,point,optimized);need(old.coefficients==want && got.coefficients==want,"OPT3C degree2 differs from dense/reference");
   need(Fr(2)*got.coefficients[0]+got.coefficients[1]+got.coefficients[2]==ev[0]+ev[1],"OPT3C recurrence");
   Fr r=variant==0?Fr(0):variant==1?Fr(1):Fr(-3-int(point.size()));state.advance("state-test",r);point.push_back(r);
   for(std::size_t j=0;j<current/2;++j){da[j]=da[2*j]*(Fr(1)-r)+da[2*j+1]*r;db[j]=db[2*j]*(Fr(1)-r)+db[2*j+1]*r;}current/=2;
  }
  need(state.endpoint("state-test",point)==db[0],"OPT3C final K(r) differs from dense");
  if(mode==0)for(auto c:calls)need(c==1,"OPT3C RAM rebuilt kernel");
  if(mode==1)need(state.metrics().file_roots==a.size() && state.metrics().file_read_bytes>0 && state.metrics().file_write_bytes>0,"OPT3C file path was not exercised");
  if(mode>=2)need(state.metrics().fallback_rebuild_calls>0,"OPT3C budget fallback was not counted");
  rejects([&]{state.withRoot(0,"foreign",[](const Value&){});},"OPT3C foreign session accepted");auto wrong=point;wrong.back()+=Fr(1);rejects([&]{state.endpoint("state-test",wrong);},"OPT3C wrong endpoint point accepted");
 }
 std::cout<<"OPT3C_EXACT_STATES_DENSE_COEFFICIENTS_AND_BUDGETS_TESTED\n";
}
struct FixedRandom:Random {std::uint64_t counter=0;void fill(std::uint8_t*p,std::size_t n)override{while(n){Writer w;w.text("opt3c-test-only-random");w.number(counter++,8);auto d=r1::hash(w.b);auto take=std::min(n,d.size());std::copy_n(d.data(),take,p);p+=take;n-=take;}}};
Bytes readFile(const std::string&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void fsMessages(){
 char name[]="build/opt3c-fs-XXXXXX";auto dir=mkdtemp(name);need(dir,"OPT3C FS test directory");Bytes first;Digest prefix{};
 fs::Binding binding{"OneblockUnified","opt3c-exact-equivalence",std::string(64,'a'),{2,7,1,8}};
 for(unsigned mode=0;mode<4;++mode){auto path=std::string(dir)+"/proof-"+std::to_string(mode);setenv("ZKGPT_K_REFERENCE",mode==0?"1":"0",1);setenv("ZKGPT_K_RAM_BYTES",mode==2?"0":mode==3?"64":"1048576",1);setenv("ZKGPT_K_FILE_BYTES",mode==2?"1048576":"0",1);
  {fixedHeaderNonce=true;fs::FsProverIO p(path,binding);FixedRandom rng;auto l=small();Prepared data;data["unit/native-E"]=std::make_shared<std::vector<Fr>>(8,Fr(7));Commitments pcs(p,l,[](std::size_t j){return Fr(j+1);},&data,&rng);pcs.constrainBaseTails();auto view=pcs.view("unit/native-E");std::vector<Fr>q{Fr(3),Fr(5),Fr(7)};auto h=exchangeHidden(p,"opt3c/value",Fr(7),&rng);view.open(p,{q,h.handle},&h,&rng,"opt3c/query");pcs.authenticateUnifiedInput({Fr(11),Fr(13)});p.finish();if(!mode)prefix=p.prefix();else need(prefix==p.prefix(),"OPT3C SHA256 transcript changed");}
  auto bytes=readFile(path);if(!mode)first=bytes;else need(bytes==first,"OPT3C canonical proof messages or FS challenges changed");
  {fs::FsVerifierIO v(path,binding);auto l=small();Commitments pcs(v,l,{},nullptr,nullptr);pcs.constrainBaseTails();auto view=pcs.view("unit/native-E");std::vector<Fr>q{Fr(3),Fr(5),Fr(7)};auto h=exchangeHidden(v,"opt3c/value",Fr(0),nullptr);view.open(v,{q,h.handle},nullptr,nullptr,"opt3c/query");pcs.authenticateUnifiedInput({Fr(11),Fr(13)});v.finish();need(v.prefix()==prefix,"OPT3C verifier transcript/EOF");}
 }
 unsetenv("ZKGPT_K_REFERENCE");unsetenv("ZKGPT_K_RAM_BYTES");unsetenv("ZKGPT_K_FILE_BYTES");std::cout<<"OPT3C_EXACT_FS_PROOF_TRANSCRIPT_AND_ROOT_OPENINGS_TESTED bytes="<<first.size()<<'\n';
}
void fileFaults(){
 for(auto mode:{kFault::shortIO,kFault::earlyEOF,kFault::corrupt,kFault::writeError}){
  kFault::mode=mode;kFault::reads=kFault::writes=0;
  auto run=[&]{ui::PublicK state({{0,16384}},[](std::size_t){std::vector<Fr>v(16384);for(std::size_t j=0;j<v.size();++j)v[j]=Fr(j+3);return v;},{0,1ull<<20},"fault-test");
   state.withRoot(0,"fault-test",[](const Value&v){for(std::size_t j=0;j<16384;++j)need(v(j)==Fr(j+3),"OPT3C short I/O changed scalar");});
   need(state.metrics().file_roots==1,"OPT3C file fault path unavailable");state.advance("fault-test",Fr(3));state.withRoot(0,"fault-test",[](const Value&v){for(std::size_t j=0;j<8192;++j)need(v(j)==Fr(2*j+6),"OPT3C short I/O fold changed scalar");});};
  if(mode==kFault::shortIO)run();else rejects(run,"OPT3C corrupt/EOF/partial-write file accepted");
  kFault::mode=kFault::normal;
 }
 std::cout<<"OPT3C_FILE_EINTR_SHORT_IO_EOF_CORRUPTION_WRITE_FAILURE_TESTED\n";
}
void stateMachine(){ui::PublicK state({{8,1}},[](std::size_t){return std::vector<Fr>{Fr(7)};},{1024,0},"lifecycle");state.withRoot(0,"lifecycle",[](const Value&){});rejects([&]{state.endpoint("lifecycle",{});},"OPT3C premature virtual-domain endpoint accepted");std::vector<Fr>point;for(unsigned t=0;t<4;++t){state.withRoot(0,"lifecycle",[](const Value&){});state.advance("lifecycle",Fr(3));point.push_back(Fr(3));}state.endpoint("lifecycle",point);rejects([&]{state.advance("lifecycle",Fr(3));},"OPT3C post-terminal advance accepted");std::cout<<"OPT3C_TERMINAL_STATE_MACHINE_TESTED\n";}
void writeWindow(){kFault::dirty=kFault::maxDirty=0;ui::PublicK state({{0,1u<<20}},[](std::size_t){return std::vector<Fr>(1u<<20,Fr(7));},{0,64ull<<20},"write-window");state.withRoot(0,"write-window",[](const Value&v){need(v(0)==Fr(7),"OPT3C write window value");});need(state.metrics().file_roots==1 && kFault::maxDirty<=16ull<<20,"OPT3C unbounded dirty page-cache write burst");std::cout<<"OPT3C_BOUNDED_FILE_WRITE_WINDOW_TESTED max_dirty="<<kFault::maxDirty<<'\n';}
void failedState(){ui::PublicK state({{0,2},{2,2}},[](std::size_t){return std::vector<Fr>{Fr(2),Fr(5)};},{1024,0},"failed-state");state.withRoot(0,"failed-state",[](const Value&){});rejects([&]{state.advance("failed-state",Fr(3));},"OPT3C incomplete round accepted");rejects([&]{state.withRoot(0,"failed-state",[](const Value&){});},"OPT3C partially advanced state reused after failure");std::cout<<"OPT3C_FAILED_STATE_CANNOT_BE_REUSED_TESTED\n";}
int main(int argc,char**argv){try{r1::init();if(argc==1 || std::string(argv[1])=="contract")kernelCallContract();if(argc==1 || std::string(argv[1])=="states")states();if(argc==1 || std::string(argv[1])=="fs")fsMessages();if(argc==1 || std::string(argv[1])=="io")fileFaults();if(argc==1 || std::string(argv[1])=="lifecycle")stateMachine();if(argc==1 || std::string(argv[1])=="write-window")writeWindow();if(argc==1 || std::string(argv[1])=="failed-state")failedState();return 0;}catch(const std::exception&e){std::cerr<<"OPT3C_FAILED "<<e.what()<<'\n';return 1;}}
