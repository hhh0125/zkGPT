#include "models.hpp"
#include "stats.hpp"
#include "host_safety.hpp"
#include "paper_r1/core.hpp"
#include <cstring>
#include <sstream>
#include <limits>
#include <new>
namespace r1=zkgpt::r1;
std::ostream& operator<<(std::ostream&,__int128_t);
std::pair<int,int> search(double);
class HostFixture:public LLM {
public:
 HostFixture():LLM(1,12,64,768,3072){total_in_size=total_para_size=total_relu_in_size=total_ave_in_size=total_max_in_size=0;}
 bool zeroCounterBytes()const{const volatile unsigned char *p=reinterpret_cast<const volatile unsigned char*>(&T);for(unsigned i=0;i<sizeof(T);++i)if(p[i])return false;return true;}
 std::vector<Fr> round(prover &pr,Fr input=Fr(1024)){
  SIZE=3;len=1;channel_out=1;pr.val.resize(3);val=pr.val.begin();
  // Reuse explicitly poisoned capacity: resize's empty Fr ctor must not suffice.
  val[0].assign(64,Fr(17));val[0].resize(1);val[0][0]=0;
  val[1].assign(1,input);i64 id=2;layer c;
  roundLayer(c,id,1.0f/1024);r1::require(id==3,"round layer count");
  for(auto &v:val[2])r1::require(v.isZero(),"round generated gate must vanish");
  r1::require(getWitnessRegistry().constraints().size()==4,"round Range count");return val[0];
 }
 std::vector<Fr> gelu(prover &pr,Fr input=Fr(std::numeric_limits<long long>::min())){
  SIZE=2;len=1;channel_out=1;q_offset=0;pr.val.resize(2);val=pr.val.begin();
  val[0].assign(1,input);i64 id=1;layer c;
  gelu_checker_layer1(c,id,1,-8,48,-8,217,-8,252,-8,615,-8,7,-8,3);return val[0];
 }
 void missing(prover &pr){SIZE=1;channel_out=channel_in=1;pr.val.resize(1);val=pr.val.begin();val[0].assign(1,Fr(0));readFconWeight(0,1,1,0);}
 void input(prover &pr){SIZE=1;total_in_size=32768;pr.val.resize(1);val=pr.val.begin();layer c;c.size=32768;calcInputLayer(c);}
 void counterOverflow(prover &pr){
  T=std::numeric_limits<i64>::max();SIZE=2;len=headnum=headdim=channel_in=1;setR4NumericalCandidate(false);
  pr.val.resize(2);val=pr.val.begin();val[0].assign(1,Fr(0));compute_e_table();i64 id=1;layer c;
  softmax_layer_1(c,id,5.0f/512,5.0f/512,5.0f/512,5.0f/512);
 }
 std::vector<Fr> ln(prover &pr,Fr input){
  SIZE=2;len=1;channel_in=1024;q_offset=0;pr.val.resize(2);val=pr.val.begin();val[0].assign(3072,Fr(0));
  val[0][0]=input;setR4NumericalCandidate();
  layer_norm_w_c[0]=1;layer_norm_w_e[0]=0;layer_norm_b_c[0]=1;layer_norm_b_e[0]=0;
  layer_norm_w_q_start[0]=1024;layer_norm_b_q_start[0]=2048;
  bool sparsity[1024];for(unsigned i=0;i<1024;++i)sparsity[i]=i<768;
  i64 id=1;layer c;ln_checker_layer1(c,id,0,-10,10,768,sparsity);validateWitnessRegistry(val[0]);return val[0];
 }
};
int main(int argc,char **argv){try{
 r1::init();r1::require(argc==2,"mode required");std::string mode=argv[1];
 if(mode=="counter"){
  alignas(HostFixture) unsigned char storage[sizeof(HostFixture)];volatile unsigned char *bytes=storage;for(unsigned i=0;i<sizeof(storage);++i)bytes[i]=0x55;
  auto *f=new(storage)HostFixture;r1::require(f->zeroCounterBytes(),"H01 counter must start at zero");f->~HostFixture();
  HostFixture overflow;prover p;bool rejected=false;try{overflow.counterOverflow(p);}catch(const std::overflow_error&){rejected=true;}r1::require(rejected,"H01 diagnostic overflow must reject");
 }else if(mode=="round"){
  HostFixture a,b;prover pa,pb;auto x=a.round(pa),y=b.round(pb);
  r1::require(x.size()==25 && x[1]==Fr(1) && x[21]==Fr(1),"round constant/output offsets");
  for(unsigned i=2;i<21;++i){r1::require(x[i].isZero(),"H02 poisoned reserved slot not zero");auto bytes=r1::encode(x[i]);for(auto b:bytes)r1::require(b==0,"canonical zero encoding");}
  r1::require(x==y,"repeated round layout must match");
 }else if(mode=="wide-gelu"){
  HostFixture f;prover p;auto x=f.gelu(p);Fr expected;expected.setStr("9223372036854775808");
  r1::require(x[12]==expected,"H10 abs(-2^63) must encode positive 2^63");
  r1::require(x[14]==expected+Fr(1),"H10 positive abs slack must retain upper u64 half");
 }else if(mode=="minimum"){
  const __int128 lo=-(__int128(1)<<126)-(__int128(1)<<126);
  r1::require(stats::abs_u128(lo)==(__uint128_t(1)<<127),"minimum absolute magnitude");
  std::ostringstream s;s<<lo;r1::require(s.str()=="-170141183460469231731687303715884105728","H11 signed minimum print");
 }else if(mode=="codec"){
  for(const char *text:{"0","9223372036854775808","-9223372036854775809","1267650600228229401496703205376","-1267650600228229401496703205376","170141183460469231731687303715884105727","-170141183460469231731687303715884105728"}){
   Fr expected;expected.setStr(text);auto integer=convert(expected);r1::require(host_safety::field(integer)==expected,"exact signed128 field embedding");
  }
 }else if(mode=="arithmetic"){
  const __int128 lo=-(__int128(1)<<126)-(__int128(1)<<126),hi=-(lo+1);
  auto rejects=[](auto f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}r1::require(rejected,"unsafe operation must reject before evaluation");};
  rejects([&]{host_safety::add(hi,1);});rejects([&]{host_safety::sub(lo,1);});rejects([&]{host_safety::mul(hi,2);});rejects([&]{host_safety::neg(lo);});
  rejects([&]{host_safety::divide(lo,-1);});rejects([&]{host_safety::divide(1,0);});
  rejects([&]{host_safety::power2(-1);});rejects([&]{host_safety::power2(127);});
  rejects([&]{host_safety::rounded(std::numeric_limits<double>::infinity());});rejects([&]{host_safety::rounded(std::numeric_limits<double>::quiet_NaN());});
  rejects([&]{host_safety::rounded(std::ldexp(1.0L,127));});
  r1::require(host_safety::rounded(2.5)==3 && host_safety::rounded(-2.5)==-3,"existing std::round tie behavior");
  r1::require(host_safety::truncated(-2.9)==-2 && host_safety::truncated(-std::ldexp(1.0L,127))==lo,"truncation and lower endpoint");
  r1::require(host_safety::divide(-7,2)==-3 && host_safety::add(hi,-1)==hi-1 && host_safety::mul(lo,1)==lo,"legal arithmetic boundaries");
  rejects([&]{host_safety::checkedSize(std::size_t(2147483647),1);});
 }else if(mode=="wide-ln"){
  HostFixture f;prover p;auto x=f.ln(p,Fr(4000001));Fr B;B.setStr("9424900712448589057");
  r1::require(x.at(4107)==B && x.at(4108)==Fr(3070000116LL),"legal u64 LN B and u32 root must retain exact integers");
 }else if(mode=="construct-boundaries"){
  auto rejects=[](auto f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}r1::require(rejected,"unsafe constructor input must reject");};
  const __int128 lo=-(__int128(1)<<126)-(__int128(1)<<126),hi=-(lo+1);
  {HostFixture f;prover p;rejects([&]{f.gelu(p,host_safety::field(lo));});}
  {HostFixture f;prover p;rejects([&]{f.round(p,host_safety::field(hi));});}
  {HostFixture f;prover p;rejects([&]{f.ln(p,host_safety::field(__int128(1)<<126));});}
  rejects([]{LLM invalid(-1,12,64,768,3072);});rejects([]{LLM invalid(1,std::numeric_limits<int>::max(),2,768,3072);});
 }else if(mode=="input-config"){
  unsetenv("ZKGPT_PUBLIC_EMBEDDING");
  for(const char *bad:{"garbage","-1","nan","inf","1e309","1.0suffix"}){
   setenv("ZKGPT_RANDOM_INPUT_ABS",bad,1);HostFixture f;prover p;bool rejected=false;try{f.input(p);}catch(const std::invalid_argument&){rejected=true;}r1::require(rejected,"invalid random input configuration must reject explicitly");
  }
 }else if(mode=="search-boundary"){
  const auto result=search(1e100);r1::require(result.first>=-10 && result.first<=10 && result.second>=1 && result.second<=800,"finite large scale must not return uninitialized search result");
  bool rejected=false;try{search(std::numeric_limits<double>::infinity());}catch(const std::invalid_argument&){rejected=true;}r1::require(rejected,"nonfinite search input must reject");
 }else if(mode=="missing"){
  setenv("ZKGPT_PARAMETER_DIR","/definitely-absent-r5a-parameters",1);setenv("ZKGPT_ALLOW_RANDOM_FC_WEIGHTS","1",1);
  HostFixture f;prover p;bool rejected=false;try{f.missing(p);}catch(const std::runtime_error&){rejected=true;}r1::require(rejected,"H14 debug flag cannot bypass missing weights");
 }else throw std::runtime_error("unknown mode");
 std::cout<<"PASS "<<mode<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
