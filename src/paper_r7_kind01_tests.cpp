#include "paper_r7_oneblock_lookup.hpp"
#include "paper_r7_product_polynomial.hpp"
#ifndef KIND01_REFERENCE
#include "paper_r7_product_kernels.hpp"
#include "paper_r7_lookup_prepare.hpp"
#endif
#include "paper_r7_oneblock_vectors.hpp"
#include "paper_r7_fs_io.hpp"
#include "paper_r7_fs_test_fixture.hpp"
#include "paper_r7_perf6_options.hpp"
#include "paper_r7_unified_input.hpp"
#include "paper_r2_range_split.hpp"
#include <cstring>
#include <random>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <unistd.h>
#include <sys/resource.h>
using namespace zkgpt::r7;
using namespace oneblock;
using namespace crypto;
namespace {
using Clock=std::chrono::steady_clock;
double seconds(Clock::time_point t){return std::chrono::duration<double>(Clock::now()-t).count();}
struct Observed:IO,EvidenceSink {
 IO&base;std::string attack;bool hit=false;std::set<std::string>evidence;
 std::set<std::string>messages;std::map<std::string,unsigned>alphas;
 std::size_t message_count=0,sigma_count=0,recurrence_count=0,coefficient_count=0,alpha_count=0;
 double sigma_seconds=0;Clock::time_point last_statement{},sigma_start{};bool sigma_pending=false;
 void finishSigma(){if(sigma_pending){sigma_seconds+=seconds(sigma_start);sigma_pending=false;}}
 Observed(IO&b,std::string a={}):base(b),attack(std::move(a)){}
 bool prover()const override{return base.prover();}
 std::string session()const override{return base.session();}
 Digest prefix()const override{return base.prefix();}
 Fr coin(const std::string&s)override{
  finishSigma();if(s.find("/batch-alpha/")!=std::string::npos)++alpha_count;
#ifndef KIND01_REFERENCE
  if(s.find("/product/01/")!=std::string::npos){
   auto layer=s.substr(0,s.find("/sumcheck"));
   if(s.find("/batch-alpha/")!=std::string::npos){
    need(messages.count(layer+"/sumcheck/statement"),"alpha before fixed pair of carries");
    need(++alphas[layer]<=2,"alpha regenerated within layer");
   }
   if(s.find("/sumcheck/round/")!=std::string::npos){
    auto prefix=s.substr(0,s.find('/',s.find("/sumcheck/round/")+16));
    for(unsigned k=0;k<4;++k)need(messages.count(prefix+"/c/"+std::to_string(k)+"/commitment"),"round coin before four aggregate coefficients");
    need(alphas[layer]==2,"missing per-layer batch coefficients");
   }
   if(s.size()>5 && s.substr(s.size()-5)=="/fold"){
    auto prefix=s.substr(0,s.size()-5)+"/sumcheck";
    for(unsigned k=0;k<5;++k)need(messages.count(prefix+"/endpoint/"+std::to_string(k)+"/commitment"),"fold before all five endpoints");
    need(alphas[s.substr(0,s.size()-5)]==2,"layer missing fixed alpha pair");
   }
  }
  if(s=="r7/fixture/gamma" || s=="r7/fixture/tau")for(auto&object:{"fixture/address","fixture/value","r7/fixture/read","r7/fixture/final"})
   need(messages.count(std::string("Oneblock/object/")+object+"/PC"),"fingerprint coin before sources");
#endif
  return base.coin(s);
 }
 Bytes message(const std::string&s,const Bytes&b={})override{
  finishSigma();++message_count;
  if(s.find("/product/01/")!=std::string::npos){
   if(s.size()>10 && s.substr(s.size()-10)=="/statement")last_statement=Clock::now();
   if(s.size()>12 && s.substr(s.size()-12)=="/first-masks"){++sigma_count;sigma_start=last_statement;if(s.find("/recurrence/")!=std::string::npos)++recurrence_count;}
   if(s.find("/sumcheck/round/")!=std::string::npos && s.find("/c/")!=std::string::npos && s.find("/commitment")!=std::string::npos)++coefficient_count;
  }
  auto out=base.message(s,b);
  if(s.find("/product/01/")!=std::string::npos && s.size()>10 && s.substr(s.size()-10)=="/responses")sigma_pending=true;
  messages.insert(s);
#ifndef KIND01_REFERENCE
  if(s.find("/product/01/")!=std::string::npos && s.find("/sumcheck/round/")!=std::string::npos)
   need(s.find("/instance/")==std::string::npos,"two independent coefficient/recurrence paths remain");
#endif
  if(!prover() && !hit && !attack.empty() && s==(attack.find("#")!=std::string::npos?attack.substr(0,attack.find("#")):attack)){if(s.find("/leaf/fingerprints/statement")!=std::string::npos){
    Reader r{out};r.text();r.raw(32);auto handles=r.number();for(std::size_t i=0;i<handles;++i){auto bytes=r.number();r.raw(bytes);}need(r.number()==2,"expected dual leaf rows");auto offset=r.at+(attack.back()=='1'?handles*32:0);auto value=r1::encode(Fr(2));std::copy(value.begin(),value.end(),out.begin()+offset);
   }else{auto h=decodeHandle(out);h.point+=scalarCommit(Fr(1),Fr(0));out=encode(h);}hit=true;}
  return out;
 }
 void checkedRelation(const std::string&s)override{need(evidence.insert(s).second,"duplicate coverage");}
};
struct Run {
 std::map<std::string,std::size_t>opens;std::map<std::string,std::vector<std::vector<Fr>>>points;
 std::vector<std::function<void()>>pending;std::size_t claims=0;
 std::string bad_source;
 VectorOracle oracle(IO&io,std::string id,std::size_t actual,std::size_t padded,Value values,Random*rng){
  auto original=values;
  if(io.prover() && id==bad_source)values=[original](std::size_t i){return original(i)+Fr(1);};
  auto out=exchangeVector(io,id,actual,padded,values,rng);auto open=out.open;
  // Compute the whole memory proof consistently from the original source,
  // while its actual PC commits to a different vector. Deferred opening must
  // reject this even though the multiset and both leaf fingerprints agree.
  if(io.prover() && id==bad_source)out.value=original;
  out.open=[this,open,id](IO&live,const Query&q,const Hidden*h,Random*r,std::string role){
   ++opens[id];++claims;points[id].push_back(q.point);Hidden saved;if(h)saved=*h;
   pending.push_back([open,&live,q,saved,r,role]{open(live,q,live.prover()?&saved:nullptr,r,role);});
  };return out;
 }
 void memory(IO&io,std::size_t n,std::size_t actual,Random*rng){
  auto av=[actual](std::size_t i){return Fr(i<actual?i%4:0);};
  auto vv=[actual](std::size_t i){return Fr(i<actual?3*(i%4):0);};
  auto a=oracle(io,"fixture/address",actual,n,io.prover()?Value(av):Value{},rng);
  auto v=oracle(io,"fixture/value",actual,n,io.prover()?Value(vv):Value{},rng);
  VectorFactory f=[this,rng](IO&live,std::string id,std::size_t a,std::size_t p,Value val){return oracle(live,id,a,p,val,rng);};
  hiddenMemory(io,a,v,{Fr(0),Fr(3),Fr(6),Fr(9)},f,rng,"r7/fixture");
  for(auto&f:pending)f();
 }
};
// Test-only replay: identical random bytes, transcript header and public fixture.
struct ReplayRandom:Random {
 OSRandom os;Bytes data;std::size_t at=0;bool replay=false;
 void fill(std::uint8_t*p,std::size_t n)override{
  if(replay){need(at+n<=data.size(),"random replay exhausted");std::copy_n(data.data()+at,n,p);at+=n;}
  else{os.fill(p,n);data.insert(data.end(),p,p+n);}
 }
};
struct TapeIO:IO {
 bool p;fs::CanonicalTranscript transcript{Bytes{1,7,6},Bytes{9,2}};
 std::vector<std::pair<std::string,Bytes>>events;std::size_t at=0;
 TapeIO(bool prover):p(prover){}
 bool prover()const override{return p;}
 std::string session()const override{return std::string(64,'e');}
 Digest prefix()const override{return transcript.prefix();}
 Fr coin(const std::string&s)override{return transcript.coin(s);}
 Bytes message(const std::string&s,const Bytes&b={})override{
  Bytes out=b;if(p)events.emplace_back(s,b);else{need(at<events.size() && events[at].first==s,"event replay role/order");out=events[at++].second;}
  transcript.message(s,out);return out;
 }
};

struct Counted {
 Fr value=Fr(0);inline static std::uint64_t mul=0,add=0;
 Counted()=default;Counted(int x):value(x){} Counted(const Fr&x):value(x){}
 friend Counted operator*(const Counted&a,const Counted&b){++mul;return Counted(a.value*b.value);}
 friend Counted operator+(const Counted&a,const Counted&b){++add;return Counted(a.value+b.value);}
 friend Counted operator-(const Counted&a,const Counted&b){++add;return Counted(a.value-b.value);}
 Counted&operator+=(const Counted&b){++add;value+=b.value;return *this;}
};
void rangeSplitTest(){
 using namespace zkgpt::r2;
 for(auto pair:std::vector<std::pair<std::size_t,std::vector<std::size_t>>>{{5,{5}},{1048576,{1048576}},{1048577,{1048577}},{1572864,{1572864}},{4194305,{4194304,1}},{4875264,{4194304,680960}},{5309952,{4194304,1115648}},{7864320,{7864320}},{8370944,{8370944}},{8388608,{8388608}}}){
  need(rangeSplitSizes(pair.first,true)==pair.second,"large split sizes");need(rangeSplitSizes(pair.first,false)==std::vector<std::size_t>{pair.first},"baseline split disabled");
 }
 std::vector<r1::InputSpan>spans{{13,3000000,Fr(1)},{5000000,1875264,Fr(-3)}};
 auto parts=splitRangeSpans(spans,{4194304,680960});need(parts.size()==2 && parts[0].size()==2 && parts[1].size()==1,"span crossing shape");
 need(parts[0][1].offset==5000000 && parts[0][1].count==1194304 && parts[1][0].offset==6194304 && parts[1][0].count==680960 && parts[1][0].bias==Fr(-3),"span coverage/bias");
 bool rejected=false;try{(void)splitRangeSpans(spans,{4194304,680959});}catch(...){rejected=true;}need(rejected,"omitted source element accepted");
 std::cout<<"PASS large range split threshold, exact contiguous coverage and bias\n";
}
void arithmeticTest(){
 std::mt19937_64 gen(20261117);auto random=[&]{Fr x=0;for(unsigned k=0;k<9;++k)x=x*Fr(2147483647)+Fr(gen()%2147483647);return x;};
 std::size_t checks=0;
 for(unsigned trial=0;trial<100;++trial){
  const auto n=std::size_t(2)<<(trial%7);std::vector<std::vector<Fr>>data(5,std::vector<Fr>(n));
  for(auto&v:data)for(auto&x:v)x=random();std::array<Fr,2>alpha{random(),random()};if(trial%9==0)alpha={Fr(0),Fr(1)};if(trial%11==0)for(auto&x:data[1])x=Fr(0);
  std::vector<Fr>rho(r1::log2exact(n));for(auto&x:rho)x=random();
  if(trial%4==0)rho[0]=Fr(0);if(trial%4==1)rho[0]=Fr(1);
  // A1 must work even for completely arbitrary E. A2/A3 use real eq factors.
  for(unsigned structured=0;structured<2;++structured){
   auto f=data;if(structured)f[0]=r1::weights(rho);
   for(std::size_t size=n,round=0;size>1;size/=2,++round){
    std::vector<Value>factors;for(unsigned k=0;k<5;++k)factors.push_back([&,k](std::size_t i){return f[k][i];});
    std::array<std::vector<Fr>,2>base;
    for(unsigned b=0;b<2;++b)base[b]=detail::productPolynomial(size,{factors[0],factors[1+2*b],factors[2+2*b]},{},true);
    std::array<Fr,4>expected;for(unsigned k=0;k<4;++k)expected[k]=alpha[0]*base[0][k]+alpha[1]*base[1][k];
    auto load=[&](unsigned k,std::size_t i){return f[k][i];};
    auto c=polynomial::bespoke<Fr>(size,load,alpha);need(c==expected,"bespoke coefficient mismatch");
    if(structured)for(bool combined:{false,true})need(polynomial::equality<Fr>(size,load,alpha,rho[round],combined)==expected,"equality coefficient mismatch");
    for(unsigned point=0;point<4;++point){auto x=random();Fr value=0;for(int k=3;k>=0;--k)value=value*x+c[k];Fr direct=0;
     for(std::size_t j=0;j<size/2;++j){std::array<Fr,5>v;for(unsigned k=0;k<5;++k)v[k]=f[k][2*j]+x*(f[k][2*j+1]-f[k][2*j]);direct+=v[0]*(alpha[0]*v[1]*v[2]+alpha[1]*v[3]*v[4]);}
     need(value==direct,"polynomial evaluation mismatch");++checks;
    }
    // Compare the actual contiguous fold routine independently against all five folds.
    auto challenge=trial%8==0?Fr(1):trial%5==0?Fr(0):trial%5==1?Fr(1):random();std::vector<std::vector<Fr>>next(5);
    for(unsigned b=0;b<2;++b){std::vector<Fr>child(2*size);for(std::size_t i=0;i<size;++i){child[2*i]=f[1+2*b][i];child[2*i+1]=f[2+2*b][i];}
     std::vector<std::vector<Fr>>fold(3);detail::foldProductInputs({f[0],child},challenge,fold);
     for(unsigned k=0;k<3;++k){auto index=k?2*b+k:0;for(std::size_t j=0;j<size/2;++j)need(fold[k][j]==f[index][2*j]+challenge*(f[index][2*j+1]-f[index][2*j]),"contiguous fold mismatch");next[index]=fold[k];}
    }f=std::move(next);
   }
  }
  need(polynomial::weights(rho)==r1::weights(rho),"recursive weights mismatch");
 }
 for(unsigned mode=1;mode<=3;++mode){
  const std::size_t n=128,p=n/2;std::vector<std::vector<Fr>>f(5,std::vector<Fr>(n));for(auto&v:f)for(auto&x:v)x=random();
  const std::array<Counted,2>alpha{Counted(random()),Counted(random())};Counted::mul=Counted::add=0;
  auto load=[&](unsigned k,std::size_t i){return Counted(f[k][i]);};
  if(mode==1)(void)polynomial::bespoke<Counted>(n,load,alpha);else (void)polynomial::equality<Counted>(n,load,alpha,Counted(2),mode==3);
  auto m=mode==1?16*p:(mode==2?14:12)*p+(mode==2?20:12),a=mode==1?18*p:(mode==2?13:15)*p+(mode==2?10:7);
  need(Counted::mul==m && Counted::add==a,"kernel operation count mismatch");
  std::cout<<"COUNTED mode="<<mode<<" pairs="<<p<<" mul="<<Counted::mul<<" add_or_sub="<<Counted::add<<'\n';
 }
 std::cout<<"PASS arithmetic seed=20261117 evaluation_checks="<<checks<<" coefficients/five-folds/weights BN254 Fr\n";
}
#ifndef KIND01_REFERENCE
void lookupPrepareTest(unsigned width=32768){
 R7Context context;
 const auto actual=width-9;
 const auto spec=r1::PCSpec::make("fixture",actual,width,r1::Layout::ColumnMajor);
 context.plan.sources={{spec,{{0,actual,Fr(0)}}},{spec,{{actual,actual,Fr(0)}}}};
 context.plan.ranges={{0,"range-1",32},{0,"range-2",13},{0,"range-3",13}};
 context.plan.exps={{0,1,"exp",r1::ExpProfile::Unclamped}};
 const std::array<unsigned,7> input{0,8190,8191,8192,2097151,1,512};
 const Value base=[&](std::size_t j){return j<actual?Fr(input[j%input.size()]):Fr(j-actual);};
 detail::NativeWorkers serial(1),parallel(4);
 auto a=classified::prepareLookupWithWorkers(context,base,serial);
 auto b=classified::prepareLookupWithWorkers(context,base,parallel);
 need(a.size()==b.size(),"lookup prepared helper count");
 for(const auto& [id,values]:a)need(*values==*b.at(id),("lookup helper parallel reference "+id).c_str());

 const auto verifyRange=[&](const std::string& role,unsigned bits,const auto& value){
  for(unsigned h=0;h*9<bits;++h){
   const auto digits=b.at(role+"/digit/"+std::to_string(h));
   const auto memory=role+"/memory/"+std::to_string(h);
   const auto read=b.at(memory+"/read");
   const auto final=b.at(memory+"/final");
   std::vector<unsigned> counts(final->size());
   for(unsigned i=0;i<width;++i){
    const auto digit=(value(i)>>(h*9))&(counts.size()-1);
    need((*digits)[i]==Fr(digit) && (*read)[i]==Fr(counts[digit]++),"lookup digit/read ordinal");
   }
   for(unsigned j=0;j<counts.size();++j)need((*final)[j]==Fr(counts[j]),"lookup final count");
  }
 };
 auto raw=[&](std::size_t j){return j<actual?input[j%input.size()]:0u;};
 verifyRange("r7/range-1",32,raw);
 verifyRange("r7/exp/range/t",32,raw);
 verifyRange("r7/exp/range/x",13,[&](std::size_t j){return std::min(8191u,raw(j));});
 verifyRange("r7/exp/range/selector",1,[&](std::size_t j){return unsigned(j<actual && raw(j)<8191);});
 verifyRange("r7/exp/range/delta",32,[&](std::size_t j){auto t=raw(j);return j>=actual?0u:t<8191?8190-t:t-8191;});
 auto read=b.at("r7/exp/indexed/read"),final=b.at("r7/exp/indexed/final");
 auto native=b.at("r7/exp/native-E");std::vector<unsigned> counts(final->size());
 for(unsigned i=0;i<width;++i){
  const auto j=std::min(8191u,raw(i));
  need((*read)[i]==Fr(counts[j]++) && (*native)[i]==Fr(i<actual?i:1048576),"lookup indexed read/native E");
 }
 for(unsigned j=0;j<counts.size();++j)need((*final)[j]==Fr(counts[j]),"lookup indexed final");
 std::cout<<"PASS Lookup Range/Exp digit, read, final and padding at one and four workers\n";
}
void parallelArithmeticTest(){
 const std::size_t n=65536;
 detail::NativeWorkers serial(1), parallel(4);
 std::vector<Fr> rho(r1::log2exact(n),Fr(13));
 rho[0]=0;rho[1]=1;
 std::vector<std::vector<Fr>> data(5,std::vector<Fr>(n));
 data[0]=r1::weights(rho);
 for(unsigned k=1;k<5;++k)for(std::size_t j=0;j<n;++j)data[k][j]=Fr((k*61+j*117)%193);
 const std::array<Fr,2> alpha{Fr(3),Fr(14)};
 auto original=data;
 const auto load=[&](unsigned k,std::size_t i){return original[k][i];};
 const Expression expression{3,{{Fr(1),{0,1,2}}}};
 for(std::size_t width=n,round=0;width>1;width/=2,++round){
  auto current=[&](unsigned k,std::size_t i){return data[k][i];};
  auto optimized=detail::batchProductCoefficients(width,current,alpha,&rho[round],parallel);
  need(optimized==polynomial::equality<Fr>(width,current,alpha,rho[round],false),"parallel Equality coefficients");
  need(detail::batchProductCoefficients(width,current,alpha,nullptr,parallel)==polynomial::bespoke<Fr>(width,current,alpha),"parallel ordinary cubic coefficients");
  std::vector<Value> factors;
  for(unsigned k=0;k<3;++k)factors.emplace_back([&,k](std::size_t j){return data[k][j];});
  const auto expected=detail::expressionPolynomial(expression,width,factors,{},true);
  const auto actual=detail::singleProductCoefficients(width,current,parallel);
  need(std::vector<Fr>(actual.begin(),actual.end())==expected,"parallel standalone coefficients");
  const Fr challenge=round%3==0?Fr(0):round%3==1?Fr(1):Fr(137);
  auto next=data;
  detail::foldNativeProduct(width,challenge,next,round==0,load,parallel);
  detail::foldNativeProduct(width,challenge,data,round==0,load,serial);
  need(next==data,"parallel Product fold mismatch");
 }
 std::array<std::vector<Fr>,2> child{original[1],original[2]};
 for(std::size_t width=n;width>1;width/=2){
  std::array<std::vector<Fr>,2> parents{std::vector<Fr>(width/2),std::vector<Fr>(width/2)};
  detail::productTreeParents(width/2,{child[0].data(),child[1].data()},{parents[0].data(),parents[1].data()},parallel);
  for(unsigned k=0;k<2;++k)for(std::size_t j=0;j<width/2;++j)
   need(parents[k][j]==child[k][2*j]*child[k][2*j+1],"parallel Product tree node mismatch");
  child=std::move(parents);
 }
 std::cout<<"PASS native Product tree, round coefficients and folds at one and four workers\n";
}
void kernelBenchmark(std::size_t n){
 need(n>=65536 && n<=(1u<<22) && !(n&(n-1)),"kernelbench power-of-two domain bounds");
 std::vector<Fr> rho(r1::log2exact(n),Fr(13));
 const std::array<Fr,2> alpha{Fr(7),Fr(13)};
 const Fr challenge=137;
 std::vector<std::vector<Fr>> data(5,std::vector<Fr>(n));
 data[0]=r1::weights(rho);
 for(unsigned k=1;k<5;++k)for(std::size_t j=0;j<n;++j)data[k][j]=Fr((k*61+j*117)%193);
 const auto load=[&](unsigned k,std::size_t i){return data[k][i];};
 const auto expected=polynomial::equality<Fr>(n,load,alpha,rho.front(),true);
 const std::array<Fr,3> expected_leaf{r1::dot(data[0],data[1]),r1::dot(data[0],data[2]),r1::dot(data[0],data[3])};
 const auto lookup_n=std::min(n,std::size_t(1)<<18);
 const auto spec=r1::PCSpec::make("bench",lookup_n,lookup_n,r1::Layout::ColumnMajor);
 R7Context lookup;
 lookup.plan.sources={{spec,{{0,lookup_n,Fr(0)}}},{spec,{{lookup_n,lookup_n,Fr(0)}}}};
 lookup.plan.ranges={{0,"range",32}};
 lookup.plan.exps={{0,1,"exp",r1::ExpProfile::Unclamped}};
 const Value base=[&](std::size_t j){return Fr(j%10943);};
 std::cout<<"kernel,width,workers,trial,wall_seconds\n";
 for(unsigned workers:{1u,4u}){
  detail::NativeWorkers pool(workers);
  for(unsigned trial=1;trial<=3;++trial){
   const auto report=[&](const char* kernel,std::size_t w,Clock::time_point start){
    std::cout<<kernel<<','<<w<<','<<workers<<','<<trial<<','<<seconds(start)<<'\n';
   };
   auto start=Clock::now();
   const auto poly=detail::batchProductCoefficients(n,load,alpha,&rho.front(),pool);
   report("product-polynomial-equality",n,start);
   need(poly==expected,"benchmark Product round differs");
   std::array<std::vector<Fr>,2> parents{std::vector<Fr>(n/2),std::vector<Fr>(n/2)};
   start=Clock::now();
   detail::productTreeParents(n/2,{data[1].data(),data[2].data()},{parents[0].data(),parents[1].data()},pool);
   report("product-tree-parent-level",n,start);
   need(parents[0][10]==data[1][20]*data[1][21] && parents[1][10]==data[2][20]*data[2][21],"benchmark Product tree differs");
   std::vector<std::vector<Fr>> folded(5);
   start=Clock::now();
   detail::foldNativeProduct(n,challenge,folded,true,load,pool);
   report("product-first-fold",n,start);
   start=Clock::now();
   detail::foldNativeProduct(n/2,challenge,folded,false,load,pool);
   report("product-in-place-fold",n/2,start);
   need(folded[4].size()==n/4,"benchmark Product fold shape");
   start=Clock::now();
   const auto dots=detail::productLeafDots<3>(data[0],[&](unsigned k,std::size_t j){return data[k+1][j];},pool);
   report("product-leaf-evaluation",n,start);
   need(dots==expected_leaf,"benchmark Product Leaf Endpoint");
   start=Clock::now();
   const auto prepared=classified::prepareLookupWithWorkers(lookup,base,pool);
   report("lookup-range-exp-prepare",lookup_n,start);
   need(prepared.count("r7/exp/indexed/final"),"benchmark Lookup indexed helper missing");
  }
 }
}
#endif
void setOptions(int mask){
 for(unsigned i=0;i<3;++i){const char*name=std::array<const char*,3>{"ZKGPT_PERF6_A2","ZKGPT_PERF6_C2","ZKGPT_PERF6_B2"}[i];
  if(mask<0)unsetenv(name);else setenv(name,mask&(1<<i)?"1":"0",1);
 }
}
void configTest(){
 setOptions(-1);auto x=perf6::options();need(x.a2 && x.c2 && x.b2,"default options");
 for(int mask=0;mask<8;++mask){setOptions(mask);x=perf6::options();need(x.a2==bool(mask&1) && x.c2==bool(mask&2) && x.b2==bool(mask&4),"independent toggles");}
 for(const auto*name:{"ZKGPT_PERF6_A2","ZKGPT_PERF6_C2","ZKGPT_PERF6_B2"})for(const auto*bad:{"","2","-1","true","01"," 1","0 "}){
  setOptions(-1);setenv(name,bad,1);bool rejected=false;
  try{(void)perf6::options();}catch(const std::exception&e){rejected=std::string(e.what()).find(name)!=std::string::npos;}
  need(rejected,"invalid environment value accepted");
 }
 setOptions(-1);need(!perf6::messages,"initial message scope");{perf6::ScopedMessages a(true);need(perf6::messages,"message scope enable");{perf6::ScopedMessages b(false);need(!perf6::messages,"message nested disable");}need(perf6::messages,"message nested restore");}need(!perf6::messages,"message outer restore");
 std::cout<<"PASS config defaults, 8 toggle combinations, 21 invalid values, nested scopes\n";
}
void standaloneFixture(IO&io,std::size_t n,Random*rng){
 auto values=std::make_shared<std::vector<Fr>>(n);Fr product=1;
 for(std::size_t i=0;i<n;++i){(*values)[i]=Fr(i%7+1);product*=(*values)[i];}
 auto root=exchangeHidden(io,"independent/root",io.prover()?product:Fr(0),rng);
 LeafAuthenticator auth=[values](IO&live,const Query&q,const Hidden*h,Random*r,std::string role){
  Hidden claim;if(live.prover())claim=*h;else claim.handle=q.evaluation;
  equalHidden(live,claim,r1::mle(*values,q.point),r,role+"/expected");
 };
 hiddenProduct(io,n,root,{},auth,rng,"independent/product",io.prover()?values:nullptr);
}
void unifiedFixture(IO&io,std::size_t n,Random*rng){
 std::vector<Fr>a(n),b(n);Fr sum=0;
 for(std::size_t i=0;i<n;++i){a[i]=Fr(i+2);b[i]=Fr(i%3+1);sum+=a[i]*b[i];}
 auto initial=exchangeHidden(io,"unified/initial",io.prover()?sum:Fr(0),rng);
 unified::Factors factors;if(io.prover())factors=[&](std::size_t k,const unified::Consume&consume){need(k==0,"unit segment");consume([&](std::size_t i){return a[i];},[&](std::size_t i){return b[i];});};
 auto end=unified::reduce(io,n,{{0,n}},initial,factors,[&](const std::vector<Fr>&point){return r1::mle(b,point);},rng,"unified/test");
 equalHidden(io,end.endpoints[0],r1::mle(a,end.point),rng,"unified/input-authentication");
}
void equivalenceTest(){
 for(const auto n:{1u,2u,4u,16u,128u}){
  setenv("ZKGPT_PRODUCT_POLY","0",1);setOptions(0);ReplayRandom random;TapeIO reference(true);Run fixture;fixture.memory(reference,n,n>2?n-1:n,&random);standaloneFixture(reference,n,&random);unifiedFixture(reference,n,&random);
  const auto prefix=reference.prefix(),coins=reference.transcript.challengeDigest();const auto count=reference.transcript.coins();
  for(unsigned experiment=0;experiment<4;++experiment){setenv("ZKGPT_PRODUCT_POLY",std::to_string(experiment).c_str(),1);for(int mask=-1;mask<8;++mask){
   setOptions(mask);random.at=0;random.replay=true;TapeIO prover(true);Run p;p.memory(prover,n,n>2?n-1:n,&random);standaloneFixture(prover,n,&random);unifiedFixture(prover,n,&random);
   need(random.at==random.data.size(),"random consumption changed");
   need(prover.events==reference.events && prover.prefix()==prefix && prover.transcript.challengeDigest()==coins && prover.transcript.coins()==count,"optimization changed messages/challenges");
   TapeIO verifier(false);verifier.events=reference.events;Run v;v.memory(verifier,n,n>2?n-1:n,nullptr);standaloneFixture(verifier,n,nullptr);unifiedFixture(verifier,n,nullptr);
   need(verifier.at==reference.events.size() && verifier.prefix()==prefix && verifier.transcript.challengeDigest()==coins,"cross-config verification changed transcript");
  }
  }
  std::cout<<"PASS exact message bytes and challenges independent/batched/unified n="<<n<<" challenges="<<count<<'\n';
 }
 setOptions(-1);
}

using zkgpt::r7::test::SeededRandom;
using zkgpt::r7::test::FixedFixtureProver;

void productBench(std::size_t n,const std::string&path,bool verifyOnly){
 auto base=std::make_shared<std::vector<Fr>>(n);std::mt19937_64 inputs(20261117);
 for(auto&value:*base){do{value=Fr(0);for(unsigned k=0;k<9;++k)value=value*Fr(2147483647)+Fr(inputs()%2147483647);}while(value==Fr(0) || value==Fr(-1));}
 std::array<Fr,2>products{Fr(1),Fr(1)};for(auto&x:*base){products[0]*=x+Fr(1);products[1]*=x;}
 auto protocol=[&](IO&io,Random*rng){
  std::array<Hidden,2>roots;for(unsigned b=0;b<2;++b)roots[b]=exchangeHidden(io,"bench/root/"+std::to_string(b),io.prover()?products[b]:Fr(0),rng);
  BatchedLeafAuthenticator auth=[&](IO&live,const std::vector<Fr>&point,const std::array<Hidden,2>&leaves,Random*random,std::string role){
   const auto f=r1::mle(*base,point);for(unsigned b=0;b<2;++b)equalHidden(live,leaves[b],f+Fr(b==0?1:0),random,role+"/expected/"+std::to_string(b));
  };
  hiddenBatchedProduct(io,n,roots,io.prover()?base:nullptr,auth,rng,"bench/product");
 };
 fs::Binding binding{"Unit","product-polynomial-experiment",std::string(64,'c'),Bytes{20,26,11,17}};
 double pt=0,vt;std::uint64_t bytes=0;
 if(!verifyOnly){SeededRandom rng;auto start=Clock::now();FixedFixtureProver io(path,binding);protocol(io,&rng);io.finish();bytes=io.proofBytes();pt=seconds(start);}
 {auto start=Clock::now();fs::FsVerifierIO io(path,binding);protocol(io,nullptr);io.finish();vt=seconds(start);}
 std::cout<<std::setprecision(12)<<"POLY_BENCH {\"n\":"<<n<<",\"seed\":20261117,\"prover_seconds\":"<<pt<<",\"verifier_seconds\":"<<vt<<",\"proof_bytes\":"<<bytes<<",\"accept\":true}\n";
}

void check(bool b,const char*s){need(b,s);}
#ifndef KIND01_REFERENCE
void direct(const std::filesystem::path&dir,std::size_t n,const std::string&fault){
 auto path=(dir/("direct-"+std::to_string(n)+"-"+fault+".proof")).string();
 fs::Binding binding{"Unit","kind01-direct",std::string(64,'b'),Bytes{4,5,6}};
 const bool malicious=!fault.empty() && fault!="nonzero";
 auto base=std::make_shared<std::vector<Fr>>(n);for(std::size_t i=0;i<n;++i)base->at(i)=Fr(fault=="nonzero"?int(i%5)+2:int(i%5)-1);
 std::array<Fr,2>products{Fr(1),Fr(1)};for(auto&f:*base){products[0]*=f+Fr(1);products[1]*=f;}
 // Independent sequential product, also authenticated by the original reducer.
 auto protocol=[&](IO&io,Random*rng){
  std::array<Hidden,2>roots;for(unsigned b=0;b<2;++b){auto value=products[b];if(fault=="root"+std::to_string(b) || fault=="cancel")value+=b==0?Fr(1):Fr(-1);roots[b]=exchangeHidden(io,"direct/root/"+std::to_string(b),io.prover()?value:Fr(0),rng);}
  BatchedLeafAuthenticator auth=[&](IO&live,const std::vector<Fr>&point,const std::array<Hidden,2>&leaves,Random*random,std::string role){
   auto f=r1::mle(*base,point);std::vector<Hidden>w{leaves[0],leaves[1]};
   for(unsigned b=0;b<2;++b){auto expected=f+Fr(b==0?1:0);if(fault=="leaf"+std::to_string(b))expected+=Fr(1);w.push_back(publicHidden(live,role+"/expected/"+std::to_string(b),expected));}
   Relations rel;rel.role=role+"/leaf-relations";for(auto&h:w)rel.handles.push_back(h.handle);rel.linear={{Fr(1),Fr(0),Fr(-1),Fr(0)},{Fr(0),Fr(1),Fr(0),Fr(-1)}};sigma(live,rel,live.prover()?&w:nullptr,random);
  };
  hiddenBatchedProduct(io,n,roots,io.prover()?base:nullptr,auth,rng,"direct/product");
  if(!malicious)for(unsigned b=0;b<2;++b){
   auto values=std::make_shared<std::vector<Fr>>(*base);if(b==0)for(auto&x:*values)x+=Fr(1);
   LeafAuthenticator old=[&,values](IO&live,const Query&q,const Hidden*h,Random*r,std::string name){Hidden claim;if(live.prover())claim=*h;else claim.handle=q.evaluation;equalHidden(live,claim,r1::mle(*values,q.point),r,name+"/expected");};
   hiddenProduct(io,n,roots[b],{},old,rng,"direct/old/"+std::to_string(b),io.prover()?values:nullptr);
  }
 };
 std::size_t witnessed_alpha_coins=0;
 bool rejected=false;try{OSRandom rng;{fs::FsProverIO wire(path,binding);Observed io(wire);try{protocol(io,&rng);}catch(...){witnessed_alpha_coins=io.alpha_count;throw;}witnessed_alpha_coins=io.alpha_count;wire.finish();}{fs::FsVerifierIO io(path,binding);protocol(io,nullptr);io.finish();}}catch(const std::exception&e){if(!malicious)throw;rejected=true;std::cout<<"REJECT semantic "<<fault<<" n="<<n<<": "<<e.what()<<'\n';}
 check(rejected==malicious,"false direct Product relation accepted");
 if(n>1){check(witnessed_alpha_coins>=2,"negative relation failed before true transcript alpha challenges");std::cout<<"TRANSCRIPT alpha_coins="<<witnessed_alpha_coins<<" fault="<<fault<<'\n';}
 if(!malicious)std::cout<<"PASS independent old/new roots n="<<n<<'\n';
}
#endif
}
int main(int argc,char**argv){try{
 r1::init();std::string mode=argc>1?argv[1]:"shared";std::size_t n=argc>2?std::stoull(argv[2]):8,actual=argc>3 && mode!="productbench" && mode!="productverify"?std::stoull(argv[3]):n;
 if(mode=="productbench" || mode=="productverify"){need(argc==4,"productbench/productverify n proof-path");productBench(n,argv[3],mode=="productverify");return 0;}
#ifndef KIND01_REFERENCE
 if(mode=="kernelbench"){kernelBenchmark(n);return 0;}
#endif
 if(mode=="rangesplit"){rangeSplitTest();return 0;}
 if(mode=="arithmetic"){arithmeticTest();
#ifndef KIND01_REFERENCE
 parallelArithmeticTest();
 lookupPrepareTest();
 lookupPrepareTest(8192);
#endif
 return 0;}
 if(mode=="config"){configTest();return 0;}
 if(mode=="equivalence"){equivalenceTest();return 0;}
 if(mode=="identity"){
  Writer parameters;parameters.text(crypto::suite);parameters.text(r1::kModulus);auto p=crypto::parameters(1);parameters.point(p.scalar);parameters.point(p.hiding);
  auto g=hyrax_a3::generators(4096);parameters.number(g.size());for(auto&x:g)parameters.point(x);
  std::ofstream out("reports/kind01-20261116/public-parameters.bin",std::ios::binary);out.write(reinterpret_cast<const char*>(parameters.b.data()),parameters.b.size());check(bool(out),"parameter artifact write");
  std::cout<<"PARAMETERS {\"fs_generator_identity\":\""<<fs::generatorIdentity()<<"\",\"public_parameter_bytes_sha256\":\""<<hex(r1::hash(parameters.b))<<"\",\"generators\":4096}\n";return 0;
 }
 auto directory=std::filesystem::path("reports/kind01-tests")/(std::to_string(getpid())+"-"+std::to_string(Clock::now().time_since_epoch().count()));std::filesystem::create_directories(directory);
 auto path=(directory/"memory.proof").string();fs::Binding binding{"Unit","kind01-memory",std::string(64,'a'),Bytes{1,2,3}};
 if(mode=="verify"){
  check(argc==5,"verify requires external proof path");fs::FsVerifierIO wire(argv[4],binding);Observed io(wire);Run verifier;verifier.memory(io,n,actual,nullptr);wire.finish();
  std::cout<<"PASS independent-process verifier n="<<n<<" actual="<<actual<<" proof_bytes="<<wire.proofBytes()<<'\n';return 0;
 }
 if(mode=="range" || mode=="exp"){
  auto protocol=[&](IO&io,Random*rng){Run run;VectorFactory factory=[&](IO&live,std::string id,std::size_t a,std::size_t p,Value val){return run.oracle(live,id,a,p,val,rng);};
   if(mode=="range"){
    Value value=[actual](std::size_t i){return Fr(i<actual?i%4:0);};auto q=run.oracle(io,"integration/range",actual,n,io.prover()?value:Value{},rng);hiddenRange(io,q,2,factory,rng,"r7/integration/range");
   }else{
    Value index=[actual](std::size_t i){return Fr(i<actual?std::array<unsigned,4>{0,8190,8191,10000}[i%4]:0);};
    Value value=[actual](std::size_t i){return Fr(i<actual?r1::expValue(std::array<unsigned,4>{0,8190,8191,10000}[i%4]):0);};
    auto a=run.oracle(io,"integration/index",actual,n,io.prover()?index:Value{},rng);auto v=run.oracle(io,"integration/exp",actual,n,io.prover()?value:Value{},rng);
    hiddenExp(io,a,v,r1::ExpProfile::Unclamped,factory,rng,"r7/integration/exp");
   }
   for(auto&open:run.pending)open();
  };
  OSRandom rng;{fs::FsProverIO wire(path,binding);Observed io(wire);protocol(io,&rng);wire.finish();}
  {fs::FsVerifierIO wire(path,binding);Observed io(wire);protocol(io,nullptr);wire.finish();}
  std::cout<<"PASS integration "<<mode<<'\n';return 0;
 }
 OSRandom rng;Run p,v;double pt,vt;std::uint64_t bytes;std::size_t pm=0,vm=0,ps=0,vs=0,recurrences=0,coefficients=0,alpha_coins=0;double pst=0,vst=0;
 {auto start=Clock::now();fs::FsProverIO wire(path,binding);Observed io(wire);p.memory(io,n,actual,&rng);io.finishSigma();pm=io.message_count;ps=io.sigma_count;pst=io.sigma_seconds;recurrences=io.recurrence_count;coefficients=io.coefficient_count;alpha_coins=io.alpha_count;wire.finish();bytes=wire.proofBytes();pt=seconds(start);}
 {auto start=Clock::now();fs::FsVerifierIO wire(path,binding);Observed io(wire);v.memory(io,n,actual,nullptr);io.finishSigma();vm=io.message_count;vs=io.sigma_count;vst=io.sigma_seconds;wire.finish();vt=seconds(start);
  for(unsigned k=0;k<3;++k){auto prefix="fixture/product/"+std::to_string(k);check(io.evidence.count(prefix+"/root"),"missing independent root coverage");}
 }
#ifndef KIND01_REFERENCE
 auto depth=r1::log2exact(n);auto rounds=depth*(depth-1)/2;
 check(coefficients==4*rounds && recurrences==rounds && alpha_coins==2*depth,"aggregate protocol message counts mismatch");
#endif
 struct rusage usage{};getrusage(RUSAGE_SELF,&usage);
 std::cout<<"BATCH_STATS {\"messages_P\":"<<pm<<",\"messages_V\":"<<vm<<",\"sigma_P\":"<<ps<<",\"sigma_V\":"<<vs<<",\"sigma_seconds_P\":"<<pst<<",\"sigma_seconds_V\":"<<vst<<",\"recurrences\":"<<recurrences<<",\"coefficient_commitments\":"<<coefficients<<",\"alpha_coins\":"<<alpha_coins<<",\"peak_rss_kib_process_P_and_V\":"<<usage.ru_maxrss<<"}\n";
 std::cout<<"RESULT {\"n\":"<<n<<",\"actual\":"<<actual<<",\"prover_seconds\":"<<pt<<",\"verifier_seconds\":"<<vt<<",\"proof_bytes\":"<<bytes<<",\"claims\":"<<p.claims<<",\"address_mle\":"<<p.opens["fixture/address"]<<",\"value_mle\":"<<p.opens["fixture/value"]<<",\"read_mle\":"<<p.opens["r7/fixture/read"]<<",\"proof\":\""<<path<<"\"}\n";
 if(mode=="shared"){
  check(p.opens["fixture/address"]==1 && p.opens["fixture/value"]==1 && p.opens["r7/fixture/read"]==1,"Kind01 must authenticate A/V/R once at shared point");
  check(v.claims==4,"batch must defer exactly A,V,R,final claims");
 }
 if(mode=="negative"){
  std::vector<std::string>attacks{"r7/fixture/root/0/handle","r7/fixture/root/1/handle",
   "r7/fixture/product/01/leaf/address/handle","r7/fixture/product/01/leaf/value/handle","r7/fixture/product/01/leaf/read/handle"};
  auto level="r7/fixture/product/01/level/1/sumcheck";
  for(unsigned k=0;k<4;++k)attacks.push_back(std::string(level)+"/endpoint/"+std::to_string(k+1)+"/commitment");
  attacks.push_back(std::string(level)+"/round/0/c/0/commitment");
  attacks.push_back(std::string(level)+"/round/0/derived");
  for(unsigned b=0;b<2;++b){attacks.push_back("r7/fixture/product/01/level/1/instance/"+std::to_string(b)+"/carry");attacks.push_back(std::string(level)+"/terminal/product/"+std::to_string(b)+"/commitment");}
  attacks.push_back("r7/fixture/product/01/leaf/fingerprints/statement#0");attacks.push_back("r7/fixture/product/01/leaf/fingerprints/statement#1");
  for(auto&attack:attacks){fs::FsVerifierIO wire(path,binding);Observed io(wire,attack);Run bad;bool rejected=false;try{bad.memory(io,n,actual,nullptr);wire.finish();}catch(const std::exception&e){rejected=true;std::cout<<"REJECT verifier "<<attack<<": "<<e.what()<<'\n';}check(io.hit && rejected,"tamper accepted or target not reached");}
  for(auto&source:{std::string("fixture/address"),std::string("fixture/value"),std::string("r7/fixture/read")}){
   auto badpath=(directory/(source=="fixture/address"?"bad-A.proof":source=="fixture/value"?"bad-V.proof":"bad-R.proof")).string();Run bad;bad.bad_source=source;bool rejected=false;
   try{OSRandom random;{fs::FsProverIO wire(badpath,binding);Observed io(wire);bad.memory(io,n,actual,&random);wire.finish();}
    {fs::FsVerifierIO wire(badpath,binding);Observed io(wire);Run verifier;verifier.memory(io,n,actual,nullptr);wire.finish();}
   }catch(const std::exception&e){rejected=true;std::cout<<"REJECT source mismatch "<<source<<": "<<e.what()<<'\n';}check(rejected,"source commitment mismatch accepted");
  }
#ifndef KIND01_REFERENCE
  for(auto size:{1u,2u,4u,8u,16u}){direct(directory,size,"");direct(directory,size,"nonzero");}
  for(auto size:{1u,8u})for(auto fault:{"root0","root1","leaf0","leaf1","cancel"})direct(directory,size,fault);
#endif
 }
 std::cout<<"PASS "<<mode<<'\n';return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
