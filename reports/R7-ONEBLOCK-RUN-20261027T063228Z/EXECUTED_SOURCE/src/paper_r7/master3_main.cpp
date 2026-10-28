#include "master_bridge.hpp"
#include "stage2_fixture.hpp"
#include "stage2_sumcheck.hpp"
#include "interactive.hpp"
#include <sys/prctl.h>
#include <sys/resource.h>
#include <unistd.h>
#include <chrono>
#include <optional>
#include <iostream>
using namespace zkgpt::r7;using namespace master3;
namespace {
std::string env(const char*n){auto p=std::getenv(n);need(p && *p,"M3 missing independently supplied configuration");return p;}
class AttackIO final:public IO {PipeIO&io_;std::string attack_;bool done_=false;public:AttackIO(PipeIO&i,std::string a):io_(i),attack_(std::move(a)){}bool prover()const override{return io_.prover();}std::string session()const override{return io_.session();}Digest prefix()const override{return io_.prefix();}
 Bytes message(const std::string&label,const Bytes&b={})override{auto out=b;auto wirelabel=label;if(prover() && !done_){bool change=false;
 if(label=="M3/master-identity" && (attack_=="wrong-master" || attack_=="layout")){if(out.size()>100)out[100]^=1;change=true;}
 if(label.find("source-and-child")!=std::string::npos && attack_=="indices"){Reader r{out};r.text();r.text();r.text();r.text();r.number();r.number();r.number();out[r.at]^=1;change=true;}
 if(label.find("A3/statement")!=std::string::npos && attack_=="old-suite"){out[4]^=1;change=true;}
 if(label.find("/evaluation/commitment")!=std::string::npos && attack_=="handle"){auto h=decodeHandle(out);h.role+="/substituted";out=encode(h);change=true;}
 if(label.find("/A3/final/z1-z2")!=std::string::npos && attack_=="encoding"){std::fill(out.begin(),out.begin()+32,0xff);change=true;}
 if(label.find("A3/knowledge/masks")!=std::string::npos && attack_=="truncate"){out.pop_back();change=true;}
 if(label=="M3/master-identity" && attack_=="order"){wirelabel="M3/wrong-order";change=true;}
 done_=change;}
 auto incoming=io_.message(wirelabel,out);return prover()?b:incoming;}
 Fr coin(const std::string&label)override{auto c=io_.coin(label);if(prover() && !done_ && attack_=="challenge" && label.find("A3/reduce/0/c")!=std::string::npos){c+=Fr(1);done_=true;}return c;}
};
Hidden exchange(IO&io,const std::string&role,const Fr&value,Random*rng){Hidden h;Bytes b;if(io.prover()){h=hide(io.session(),role,value,*rng);b=encode(h.handle);}b=io.message(role+"/commitment",b);h.handle=decodeHandle(b);validate(h.handle,io.session(),role);return h;}
PC operand(IO&io,const std::string&role,std::size_t n,const PCSecret*s){auto b=io.message(role+"/PC",io.prover()?encode(s->public_pc):Bytes{});auto pc=decodePC(b);need(pc.spec.role==role && pc.spec.actual==n && pc.spec.padded==n,"M3 operand role/shape");return pc;}
}
int main(int argc,char**argv){try{
 need(prctl(PR_SET_DUMPABLE,0)==0,"M3 process isolation");alarm(300);need(argc>=3 && argc<=4,"M3 usage MODE N [P_ATTACK]");
#ifdef R7_M3_VERIFIER
 constexpr bool prover=false;need(argc==3,"M3 V cannot import prover challenge/receipt/options");
#else
 constexpr bool prover=true;
#endif
 r1::init();auto start=std::chrono::steady_clock::now();auto mode=std::string(argv[1]);auto textN=std::string(argv[2]);need(textN=="4096" || textN=="65536" || textN=="1048576" || textN=="16777216","M3 scale whitelist");auto N=std::stoull(textN);bool projection=mode!="s3-opening";need(mode=="s3-opening" || mode=="s3-fc" || mode=="s3-fc-local" || mode.rfind("s3-gather-",0)==0,"M3 unknown mode");auto attack=prover && argc==4?std::string(argv[3]):"";auto context=trustedContext();auto layout=trustedLayout(context);auto expected=makeSpec(layout,N,projection);auto publicBytes=stage2::readOwnedFD(std::stoi(env("R7_M3_PUBLIC_FD")));need(hex(r1::hash(publicBytes))==env("R7_M3_PUBLIC_SHA"),"M3 unapproved descriptor source");auto master=decodeMaster(publicBytes,expected);
 std::optional<MappedValues>cache;Secret secret;std::optional<stage2::PrivateFixture>fixture;std::optional<stage2::TrustedRegistry>approved;
 if(prover){cache.emplace(std::stoi(env("R7_M3_VALUES_FD")));secret.public_pc=master;auto blinds=stage2::readOwnedFD(std::stoi(env("R7_M3_BLINDS_FD")),true);Reader r{blinds};for(std::size_t i=0;i<master.spec.rows;++i)secret.blinds.push_back(r.scalar());r.end();secret.value=[&](std::size_t i){return cache->at(projection?layout.projection.at(i):i);};if(attack=="wrong-row-secret")secret.blinds[0]+=Fr(1);if(mode=="s3-fc" || mode=="s3-fc-local")fixture=stage2::decodePrivateFixture(stage2::readOwnedFD(std::stoi(env("R7_M3_FC_PRIVATE_FD")),true));}
 else if(mode=="s3-fc" || mode=="s3-fc-local")approved=stage2::TrustedRegistry::load(std::stoi(env("R7_M3_CW_FD")),env("R7_M3_CW_SHA"));
 PipeIO pipe(prover,mode+"/"+textN,"",master3::suite);AttackIO io(pipe,attack);OSRandom rng;Random*random=prover?&rng:nullptr;std::vector<stage2::Phase>phases;auto timed=[&](const std::string&name,const auto&f){auto t=std::chrono::steady_clock::now();auto bytes=pipe.proofBytes(),calls=metrics().msm_calls;f();phases.push_back({name,std::chrono::duration<double>(std::chrono::steady_clock::now()-t).count(),pipe.proofBytes()-bytes,metrics().msm_calls-calls});};
 bindPublic(io,"M3/master-identity",encodeMaster(master));std::optional<RegisteredMaster>registered;if(registrationSupported(master.spec))timed("master-registration",[&]{registered.emplace(RegisteredMaster::establish(io,master,prover?&secret:nullptr));});
 if(mode=="s3-opening")for(unsigned k=0;k<2;++k)timed("master-opening-"+std::to_string(k),[&]{std::vector<Fr>point;for(unsigned j=0;j<r1::log2exact(N);++j)point.push_back(io.coin("M3/query/"+std::to_string(k)+"/"+std::to_string(j)));Fr value=prover?evaluate(secret,point):Fr(0);if(prover && attack=="wrong-claim")value+=Fr(1);auto h=exchange(io,"M3/"+std::to_string(k)+"/evaluation",value,random);openMaster(io,master,{point,h.handle},prover?&secret:nullptr,prover?&h:nullptr,random,"query"+std::to_string(k));});
 else if(mode.rfind("s3-gather-",0)==0){auto map=testMap(mode.substr(10));PCSecret child;if(prover){std::vector<Fr>v=map.bias;for(auto&t:map.terms)v[t.child]+=t.coefficient*secret.value(t.source);if(attack=="wrong-gather")v[0]+=Fr(1);child=commit(PCSpec::make(map.id,16,16),v,rng);}auto pc=operand(io,map.id,16,prover?&child:nullptr);timed("bridge",[&]{bindChild(io,*registered,pc,map,prover?&secret:nullptr,prover?&child:nullptr,random,attack);});}
 else{
  auto cw=prover?stage2::RegisteredCW::prover(io,fixture->primary,fixture->weight,rng):stage2::RegisteredCW::verifier(io,*approved);PCSecret activation,output;SumcheckWitness witness;
  if(prover){auto a=fixture->activation;if(attack=="self-consistent")a[0]+=Fr(1);witness.factors={a,fixture->weight.values};witness.claim=r1::dot(a,fixture->weight.values);activation=commit(PCSpec::make("S2/activation/0",1024,1024),a,rng);output=commit(PCSpec::make("S2/output",1,1),{witness.claim},rng);}
  auto aPC=operand(io,"S2/activation/0",1024,prover?&activation:nullptr),yPC=operand(io,"S2/output",1,prover?&output:nullptr);std::vector<PC>factors{aPC,cw.approved().pc};std::vector<const PCSecret*>secrets;if(prover)secrets={&activation,&fixture->weight};timed("FC-sumcheck-A3",[&]{stage2::committedA3Sumcheck(io,cw,factors,yPC,secrets,prover?&output:nullptr,prover?&witness:nullptr,random);});std::cerr<<"REAL_FC_ARITHMETIC_VERIFIED_BEFORE_SOURCE_BRIDGE\n";
  if(mode=="s3-fc")for(unsigned k=0;k<3;++k)timed("source-bridge-"+std::to_string(k),[&]{const auto&pc=k==0?aPC:k==1?cw.approved().pc:yPC;const PCSecret*s=prover?(k==0?&activation:k==1?&fixture->weight:&output):nullptr;bindChild(io,*registered,pc,fcMap(k),prover?&secret:nullptr,s,random,attack);});
 }
 bindPublic(io,"M3/LOCAL_CRYPTO_TESTED/complete",Bytes{'d','o','n','e'});if(!prover)stage2::writeOwnedFD(std::stoi(env("R7_PUBLIC_AUDIT_FD")),pipe.publicTranscript());struct rusage u{};getrusage(RUSAGE_SELF,&u);auto m=metrics();std::cerr<<"{\"status\":\"LOCAL_CRYPTO_TESTED\",\"N\":"<<N<<",\"actual\":"<<master.spec.actual<<",\"R\":"<<master.spec.rows<<",\"C\":1024,\"A3_folded_n\":1024,\"master_identity\":\""<<identity(master)<<"\",\"scope\":\""<<master.spec.scope<<"\",\"full_knowledge_and_tail\":\""<<(registered?"VERIFIED_IN_THIS_SESSION":"INCOMPLETE_TRANSPORT_BUDGET")<<"\",\"all_R6_sources\":\"INCOMPLETE\",\"witness_generation_included\":false,\"seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<",\"cpu_user_seconds\":"<<u.ru_utime.tv_sec+u.ru_utime.tv_usec/1e6<<",\"cpu_system_seconds\":"<<u.ru_stime.tv_sec+u.ru_stime.tv_usec/1e6<<",\"P_wire_bytes\":"<<pipe.proofBytes()<<",\"peak_rss_kib\":"<<u.ru_maxrss<<",\"MSM_calls\":"<<m.msm_calls<<",\"MSM_terms\":"<<m.msm_terms<<",\"max_MSM\":"<<m.max_msm<<",\"standalone_scalar_multiplications\":\"NOT_COUNTED\",\"oneblock_status\":\"INCOMPLETE\",\"production_gate\":\"BLOCKED_CRYPTOGRAPHIC_DESIGN\",\"may_enter_stage2\":false,\"phases\":[";for(std::size_t i=0;i<phases.size();++i){auto&p=phases[i];if(i)std::cerr<<',';std::cerr<<"{\"name\":\""<<p.name<<"\",\"seconds\":"<<p.seconds<<",\"P_wire_bytes\":"<<p.p_wire_bytes<<",\"MSM_calls\":"<<p.msm_calls<<'}';}std::cerr<<"]}\n";return 0;
}catch(const std::exception&e){std::cerr<<"REJECT "<<e.what()<<'\n';return 2;}}
