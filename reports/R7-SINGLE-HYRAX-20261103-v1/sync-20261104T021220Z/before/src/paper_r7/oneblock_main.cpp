#include "oneblock_plan.hpp"
#include "classified_commitments.hpp"
#include "oneblock_vectors.hpp"
#include "oneblock_gkr.hpp"
#include "oneblock_lookup.hpp"
#include "oneblock_model.hpp"
#include "oneblock_bindings.hpp"
#include "oneblock_public.hpp"
#include "interactive.hpp"
#include "fs_io.hpp"
#ifdef R7_FS_FULL12
#include "full12_model.hpp"
#include "execution_target.hpp"
#endif
#include "paper_r2/r6_model.hpp"
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cerrno>
#include <unistd.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <algorithm>
using namespace zkgpt::r7;using namespace oneblock;using namespace crypto;namespace r6=zkgpt::r6;namespace r2=zkgpt::r2;
namespace {
#ifdef R7_FS_ONEBLOCK
#ifdef R7_FS_FULL12
#ifdef R7_UNIFIED_INPUT1
constexpr auto liveSuite=zkgpt::r7::fs::unifiedFull12Suite;
#else
constexpr auto liveSuite=zkgpt::r7::fs::full12Suite;
#endif
#else
#ifdef R7_UNIFIED_INPUT1
constexpr auto liveSuite=zkgpt::r7::fs::unifiedSuite;
#else
constexpr auto liveSuite=zkgpt::r7::fs::suite;
#endif
#endif
constexpr auto coverageAccepted=zkgpt::r7::fs::verifiedStatus;
#elif defined(R7_CLASSIFIED_V2)
#ifdef R7_UNIFIED_INPUT1
constexpr auto liveSuite="R7-CLASSIFIED-PROTOCOL1-UNIFIED-INTERACTIVE-v1";
#else
constexpr auto liveSuite=classified::suite;
#endif
constexpr auto liveProfile=PipeIO::Profile::ClassifiedV2;
constexpr auto coverageAccepted="ONEBLOCK_INTERACTIVE_ACCEPT";
#else
constexpr auto liveSuite="R7-ONEBLOCK-INDEPENDENT-INTERACTIVE-v1";
constexpr auto liveProfile=PipeIO::Profile::OneblockV1;
constexpr auto coverageAccepted="ONEBLOCK_INTERACTIVE_ACCEPT";
#endif
int fd(const char*n){auto p=std::getenv(n);need(p && *p,"Oneblock missing independent FD");return std::stoi(p);}
Bytes publicStatement(int f){struct stat st{};auto flags=fcntl(f,F_GETFL);need(f>2 && fstat(f,&st)==0 && S_ISREG(st.st_mode) && st.st_uid==getuid() && st.st_size>0 && std::uint64_t(st.st_size)<=R7Wire::maxStatementBytes && flags>=0 && (flags&O_ACCMODE)==O_RDONLY,"Oneblock independent public statement FD invalid");Bytes b(st.st_size);std::size_t at=0;while(at<b.size()){auto n=pread(f,b.data()+at,b.size()-at,at);if(n<0 && errno==EINTR)continue;need(n>0,"Oneblock public statement read");at+=n;}return b;}
void output(int f,const std::string&s){std::size_t at=0;while(at<s.size()){auto n=::write(f,s.data()+at,s.size()-at);need(n>0,"Oneblock result write");at+=n;}}
// Coverage is verifier-local state. There is no peer-supplied coverage bitmap.
class Tracked final:public IO,public EvidenceSink {
 IO&base_;std::function<std::uint64_t()>frames_;const Coverage&plan_;std::set<std::string>expected_,done_;std::map<std::string,std::pair<std::uint64_t,std::string>>evidence_;std::string last_,attack_;bool changed_=false;mutable bool saved_=false;
#ifdef R7_CLASSIFIED_V2
 bool authenticated_=false;
#else
 bool authenticated_=true;
#endif
 public:Tracked(IO&b,const Coverage&p,std::string attack,std::function<std::uint64_t()>frames):base_(b),frames_(std::move(frames)),plan_(p),attack_(std::move(attack)){for(auto&r:p.required)expected_.insert(r.id);}
 bool prover()const override{return base_.prover();}std::string session()const override{return base_.session();}Digest prefix()const override{return base_.prefix();}
 Bytes message(const std::string&label,const Bytes&v={})override{last_=label;if(!prover() || attack_.empty() || changed_)return base_.message(label,v);auto b=v;if(prover() && !changed_){
  if(attack_=="wrong-commitment" && label=="Classified/root/1/PC"){auto pc=decodeVector(b);pc.points[0]+=parameters(1).scalar;b=encodeVector(pc);changed_=true;}
  if(attack_=="missing-authentication" && (label=="Classified/all-claims-fixed" || label=="UnifiedInput/all-claims-fixed")){close(1);throw std::runtime_error("Classified deliberate missing authentication");}
  if(attack_=="model-response" && label=="oneblock/model/register/0/responses"){auto x=r1::decodeFr(Bytes(b.begin(),b.begin()+32));auto c=r1::encode(x+Fr(1));std::copy(c.begin(),c.end(),b.begin());changed_=true;}
  if(attack_=="truncated" && label=="oneblock/model/register/0/responses"){b.pop_back();changed_=true;}
  if(attack_=="missing-constraint" && label.find("Oneblock/GKR/")!=std::string::npos){close(1);throw std::runtime_error("Oneblock deliberate missing constraint");}
 }auto result=base_.message(label,b);return prover()?v:result;}
 Fr coin(const std::string&s)override{last_="coin/"+s;return base_.coin(s);}
 void checkedRelation(const std::string&id)override{if(prover())return;need(expected_.count(id),("Oneblock unknown coverage evidence ID: "+id).c_str());done_.insert(id);evidence_[id]={frames_(),last_};}
 void authenticationComplete(){authenticated_=true;}
 void complete()const{need(authenticated_,"Classified authentication barrier incomplete");need(done_==expected_,("Oneblock uncovered mandatory relations: "+std::to_string(expected_.size()-done_.size())).c_str());}
 void save(bool accepted)const{if(prover() || saved_)return;std::ostringstream out;writeCoverage(out,plan_);auto raw=out.str();auto marker=raw.find("\"status\":\"DISCOVERED_NOT_CRYPTOGRAPHICALLY_VERIFIED\"");need(marker!=std::string::npos,"coverage schema");raw.replace(marker,std::string("\"status\":\"DISCOVERED_NOT_CRYPTOGRAPHICALLY_VERIFIED\"").size(),std::string("\"status\":\"")+(accepted?coverageAccepted:"INCOMPLETE")+"\"");
 for(auto&id:done_){if(!accepted || !authenticated_)break;auto needle="\"id\":\""+id+"\"";auto at=raw.find(needle);need(at!=std::string::npos,"coverage ID absent");auto status=raw.find("\"status\":\"UNVERIFIED\"",at);need(status!=std::string::npos,"coverage status absent");raw.replace(status,21,"\"status\":\"VERIFIED\"");}
 raw.pop_back();raw.pop_back();raw+=",\"verified_count\":"+std::to_string(accepted && authenticated_?done_.size():0)+",\"session\":\""+session()+"\",\"evidence\":[";bool first=true;for(auto&[id,e]:evidence_){if(!first)raw+=',';first=false;raw+="{\"id\":\""+id+"\",\"through_frame\":"+std::to_string(e.first)+",\"last_checked_message\":\""+e.second+"\"}";}raw+="]}\n";output(fd("R7_ONEBLOCK_COVERAGE_FD"),raw);saved_=true;}
};
Value masterValue(R7WitnessStore&w){return [&w](std::size_t i){return i<w.baseMaster().size()?w.baseMaster()[i]:Fr(0);};}
std::pair<std::size_t,Fr>sourceIndex(const r2::ModelQuerySource&s,std::size_t i){for(auto&span:s.spans){if(i<span.count)return {span.offset+i,span.bias};i-=span.count;}throw std::runtime_error("Oneblock native source map overflow");}
Fr evaluateOracle(const VectorOracle&v,const std::vector<Fr>&q){auto w=r1::weights(q);Fr y=0;for(std::size_t i=0;i<v.padded;++i)y+=w[i]*v.value(i);return y;}
}
int main(int argc,char**argv){std::unique_ptr<Tracked>tracked;try{
 need(prctl(PR_SET_DUMPABLE,0)==0,"Oneblock process isolation");
#ifndef R7_FS_FULL12
 alarm(21600);
#endif
 r1::init();auto started=std::chrono::steady_clock::now();
#ifdef R7_FS_ONEBLOCK
 need(argc>=4 && argc<=5,"FS usage prove|verify PROOF_PATH CONTEXT [P_ATTACK]");
#else
 need(argc>=2 && argc<=3,"Oneblock usage setup|run [P_ATTACK]");
#endif
 #ifdef R7_FS_FULL12
 auto plan=discover(12);
#else
 auto plan=discover();
#endif
 auto&ctx=plan.context;
#ifdef R7_CLASSIFIED_V2
 auto layout=classified::makeLayout(ctx);
 plan.required.erase(std::remove_if(plan.required.begin(),plan.required.end(),[](auto&r){return r.family=="OBJECT_COMMITMENT" || r.family=="OBJECT_SOURCE" || r.family.rfind("MASTER_",0)==0;}),plan.required.end());
 for(auto&r:plan.required)if(r.family=="MODEL_MASTER_LINK")r.id="model/C_W_model-to-classified-input";
 for(auto&r:layout.roots){plan.required.push_back({"classified/root/"+r.id+"/commitment","CLASSIFIED_COMMITMENT",r.id,r.padded,0});if(r.registered)plan.required.push_back({"classified/root/"+r.id+"/physical-zero-tail",r.model?"CLASSIFIED_TRUSTED_ANCHOR_TAIL":"CLASSIFIED_ZERO_TAIL",r.id,r.padded,0});}
 for(auto&[id,v]:layout.views){plan.required.push_back({"classified/view/"+id,"CLASSIFIED_VIEW",id,v.padded,0});}
#ifdef R7_UNIFIED_INPUT1
 plan.required.push_back({"unified-input/sumcheck","UNIFIED_INPUT_SUMCHECK",classified::identity(layout),ctx.plan.master.actual,0});
 plan.required.push_back({"unified-input/classified-opening","UNIFIED_INPUT_OPENING",classified::identity(layout),ctx.plan.master.actual,0});
 for(auto&r:layout.roots)plan.required.push_back({"unified-input/root/"+r.id+"/opening","UNIFIED_CLASSIFIED_ROOT_OPENING",r.id,r.padded,0});
#else
 plan.required.push_back({"classified/authentication-barrier","CLASSIFIED_AUTHENTICATION",classified::identity(layout),ctx.plan.master.actual,0});
#endif
#endif
#ifdef R7_COMPLETE_VERIFIER
 constexpr bool isP=false;
#ifdef R7_FS_ONEBLOCK
 need(argc==4,"FS verifier cannot import challenges/receipt/options");
#else
 need(argc==2,"Oneblock verifier cannot import challenges/receipt/options");
#endif
#else
 constexpr bool isP=true;
#endif
 #ifdef R7_FS_ONEBLOCK
 need(std::string(argv[1])==(isP?"prove":"verify"),"FS role/mode mismatch");
 std::string mode="run",attack=isP && argc==5?argv[4]:"";
 [[maybe_unused]] const char*successStatus=isP?"FS_PROOF_CREATED_CANDIDATE":fs::verifiedStatus;
#else
 std::string mode=argv[1],attack=isP && argc==3?argv[2]:"";need(mode=="setup" || mode=="run","Oneblock unknown mode");
 [[maybe_unused]] const char*successStatus="ONEBLOCK_CLASSIFIED_INTERACTIVE_ACCEPT";
#endif
 if(mode=="setup"){need(isP && argc==2,"Oneblock only P creates public test claim");R7WitnessStore w;{master3::QuietStdout quiet;w.generateR6(ctx,"reports/R6A/oneblock-model-manifest-v1.txt");}auto s=makeStatement(ctx,w.publicX(),w.publicY(ctx));auto b=R7Wire::encodeStatement(s);output(fd("R7_ONEBLOCK_STATEMENT_OUT_FD"),std::string(b.begin(),b.end()));std::cerr<<"PUBLIC_X_Y_CLAIM_CREATED_NO_PRIVATE_WITNESS_EXPORT\n";return 0;}
 auto statement_bytes=publicStatement(fd("R7_ONEBLOCK_STATEMENT_FD"));auto s=R7Wire::decodeStatement(statement_bytes);need(validateStatement(s,ctx).code==Code::Ok,"Oneblock invalid frozen public statement");
 // Each process independently owns the public statement. Test harness mutates
 // both public copies for wrong X/Y, rather than testing only byte mismatch.
 #ifdef R7_FS_ONEBLOCK
 need(R7Wire::encodeStatement(s)==statement_bytes,"FS noncanonical external statement");
 #ifdef R7_FS_FULL12
 auto fsTrust=full12::loadTrust(ctx);
#else
 auto fsTrust=[](){master3::QuietStdout quiet;return model::loadTrust();}();
#endif
 Writer instance;instance.text(liveSuite);instance.text(identity(plan));instance.text(classified::identity(layout));
#ifdef R7_UNIFIED_INPUT1
 instance.text(classified::unifiedInputIdentity(layout,s.x));
#endif
#ifdef R7_FS_FULL12
 instance.text(full12::trustIdentity(fsTrust));
#else
 instance.text(model::trustIdentity(fsTrust));
#endif
instance.raw(statement_bytes);
 #ifdef R7_FS_FULL12
 std::vector<G1>extraGenerators;for(auto&t:fsTrust.chunks){auto&g=r1::generators(t.anchor.w.spec).g;extraGenerators.insert(extraGenerators.end(),g.begin(),g.end());}
 fs::Binding fsBinding{
#ifdef R7_UNIFIED_INPUT1
 "Full12Unified",
#else
 "Full12",
#endif
 argv[3],fs::generatorIdentity(extraGenerators),instance.b};
#else
 fs::Binding fsBinding{
#ifdef R7_UNIFIED_INPUT1
 "OneblockUnified",
#else
 "Oneblock",
#endif
 argv[3],fs::generatorIdentity(r1::generators(fsTrust.anchor.w.spec).g),instance.b};
#endif
#ifdef R7_COMPLETE_VERIFIER
 fs::FsVerifierIO pipe(argv[2],fsBinding);
#else
 fs::FsProverIO pipe(argv[2],fsBinding);
#endif
#else
 PipeIO pipe(isP,"oneblock-full",attack=="cross-session"?"session":"",liveSuite,liveProfile,isP?-1:fd("R7_PUBLIC_AUDIT_FD"));
#endif
 tracked=std::make_unique<Tracked>(pipe,plan,attack,[&pipe]{return pipe.frames();});auto&io=*tracked;struct EvidenceGuard{Tracked*t;~EvidenceGuard(){try{t->save(false);}catch(...){}}} guard{tracked.get()};SessionRandom random;Random*rng=isP?&random:nullptr;
 Writer binding;binding.text(liveSuite);binding.text(identity(plan));binding.raw(R7Wire::encodeStatement(s));bindPublic(io,"Oneblock/statement-native-plan",binding.b);
 std::vector<stage2::Phase>phases;auto timed=[&](const std::string&id,const auto&fn){auto t=std::chrono::steady_clock::now();[[maybe_unused]] auto cpuStarted=processCPU();auto bytes=pipe.proofBytes(),calls=metrics().msm_calls;std::cerr<<"PHASE_BEGIN "<<id<<std::endl;fn();phases.push_back({id,std::chrono::duration<double>(std::chrono::steady_clock::now()-t).count(),pipe.proofBytes()-bytes,metrics().msm_calls-calls});struct rusage phaseUsage{};getrusage(RUSAGE_SELF,&phaseUsage);phases.back().peak_rss_kib=phaseUsage.ru_maxrss;std::ifstream statm("/proc/self/statm");std::uint64_t pages=0,resident=0;if(statm>>pages>>resident)phases.back().rss_kib=resident*std::uint64_t(sysconf(_SC_PAGESIZE))/1024;std::cerr<<"PHASE_COMPLETE "<<id<<'\n';
#ifdef R7_FS_FULL12
 std::cerr<<"PHASE_CPU {\"name\":\""<<id<<"\",\"cpu_seconds\":"<<processCPU()-cpuStarted<<"}\n";
#endif
};
 #ifndef R7_CLASSIFIED_V2
 auto trust=[](){master3::QuietStdout quiet;return model::loadTrust();}();std::optional<model::Secret>modelSecret;model::Public modelPublic;
 timed("model-authority-registration",[&]{if(isP)executePrivateWitness([&]{master3::QuietStdout quiet;modelSecret=model::prepare(trust,io.session(),random);});modelPublic=model::registerModel(io,trust,isP?&*modelSecret:nullptr,rng);});checked(io,"model/checkpoint-exporter-profile");checked(io,"model/anchor-hiding-rerandomization");
 R7WitnessStore witness;std::optional<LLM>network;layeredCircuit publicGraph;const layeredCircuit*graph=nullptr;Value master;master3::Secret masterSecret;master3::Master pc;
 timed("frozen-witness-and-master-commitment",[&]{if(isP){{master3::QuietStdout quiet;witness.generateR6(ctx,"reports/R6A/oneblock-model-manifest-v1.txt");}graph=&witness.executionForR7().C;
  if(attack=="wrong-internal" || attack=="wrong-weight"){auto layout=master3::trustedLayout(ctx);auto it=std::find_if(layout.regions.begin(),layout.regions.end(),[&](auto&r){return attack=="wrong-internal"?r.id=="R6/witness/round_0_affine":r.kind=="W";});need(it!=layout.regions.end(),"Oneblock attack native region");witness.executionForR7().val[0].at(it->offset)+=Fr(1);}
  master=masterValue(witness);masterSecret=master3::commitMaster(master3::makeSpec(master3::trustedLayout(ctx),ctx.plan.master.padded),master);pc=masterSecret.public_pc;
 }else{master3::QuietStdout quiet;network.emplace(1,12,64,768,3072);r6::configure(*network);publicGraph=network->buildPublicCircuit();graph=&publicGraph;}
 auto b=io.message("Oneblock/BaseMaster/PC",isP?master3::encodeMaster(pc):Bytes{});pc=master3::decodeMaster(b,master3::makeSpec(master3::trustedLayout(ctx),ctx.plan.master.padded));});checked(io,"object/"+ctx.objects.front().id+"/commitment");
 masterB::RegistrationMetrics registration;std::optional<masterB::Registered>registered;timed("one-full-master-registration",[&]{registered.emplace(masterB::Registered::establish(io,pc,isP?&masterSecret:nullptr,"oneblock-full",registration));});checked(io,"master/registration");checked(io,"master/zero-tail");
 timed("public-X-Y-constants",[&]{auto entries=trustedPublicEntries(s,ctx);auto all=std::move(entries.x);all.insert(all.end(),entries.y.begin(),entries.y.end());all.insert(all.end(),entries.constants.begin(),entries.constants.end());publicBinding(io,*registered,isP?&masterSecret:nullptr,master,all,rng,"Oneblock/public");});for(auto id:{"public/X","public/Y","public/constants"})checked(io,id);
 std::optional<PendingGKR>pendingHolder;timed("all-native-GKR",[&]{pendingHolder.emplace(runHiddenGKR(io,*graph,isP?&witness.executionForR7():nullptr,ctx.plan.shape.sequence_length,rng));});auto&pending=*pendingHolder;std::cerr<<"ALL_NATIVE_GKR_REDUCED "<<pending.layers<<' '<<pending.matrices<<' '<<pending.terminals.size()<<' '<<pending.rounds<<'\n';
 std::map<std::string,WitnessObject>expected;for(auto&o:ctx.objects)expected.emplace(o.id,o);std::set<std::string>objects{ctx.objects.front().id};
 VectorFactory factory=[&](IO&live,std::string id,std::size_t actual,std::size_t padded,Value value){auto it=expected.find(id);need(it!=expected.end() && it->second.actual==actual && it->second.padded==padded && objects.insert(id).second,("Oneblock unknown/duplicate/wrong-shape auxiliary object: "+id).c_str());
  if(live.prover() && attack=="wrong-Auxiliary" && id.find("/read")!=std::string::npos){auto original=value;value=[original](std::size_t i){return original(i)+Fr(i==0?1:0);};attack="auxiliary-injected";}
  return exchangeVector(live,id,actual,padded,std::move(value),rng,[&](const VectorPC&){checked(live,"object/"+id+"/commitment");});};
 std::vector<VectorOracle>sources;timed("native-source-commitments",[&]{for(std::size_t j=0;j<ctx.plan.sources.size();++j){const auto&source=ctx.plan.sources[j];auto id="r7/"+source.spec.role;if(j+1==ctx.plan.sources.size()){VectorOracle v;v.id=id;v.actual=modelPublic.spec.actual;v.padded=modelPublic.spec.padded;if(isP)v.value=[&](std::size_t i){return modelSecret->value(i);};v.open=[&](IO&live,const Query&q,const Hidden*h,Random*r,std::string label){model::opening(live,trust,modelPublic,q,isP?&*modelSecret:nullptr,h,r,label);};sources.push_back(v);need(objects.insert(id).second,"Oneblock duplicate model source");checked(io,"object/"+id+"/commitment");}
 else{Value value;if(isP)value=[&,j](std::size_t i){const auto&source=ctx.plan.sources[j];if(i>=source.spec.actual)return Fr(0);auto [index,bias]=sourceIndex(source,i);return master(index)+bias;};sources.push_back(factory(io,id,source.spec.actual,source.spec.padded,value));}}});
 timed("native-Range-Exp-Lasso-Memory-Product",[&]{for(const auto&r:ctx.plan.ranges)hiddenRange(io,sources.at(r.child),r.bits,factory,rng,"r7/"+r.id);for(const auto&e:ctx.plan.exps)hiddenExp(io,sources.at(e.index_child),sources.at(e.value_child),e.profile,factory,rng,"r7/"+e.id);});
 std::vector<MappedMasterClaim>claims;std::set<std::string>terminals;
 for(auto&t:pending.terminals){const auto&l=graph->circuit.at(t.layer);const auto*indices=t.port?&l.ori_id_v:&l.ori_id_u;auto id="gkr/layer/"+std::to_string(t.layer)+"/terminal/"+(t.port?"v":"u")+"/"+std::to_string(t.matrix_point);need(terminals.insert(id).second,"Oneblock repeated GKR terminal");claims.push_back({id,t.actual,t.padded,[indices](std::size_t i){return indices->at(i);},t.value,t.point,Fr(0)});}
 std::set<std::string>nativeTerminals;for(auto&r:plan.required)if(r.family=="GKR_MASTER_TERMINAL")nativeTerminals.insert(r.id);need(terminals==nativeTerminals,"Oneblock native terminal omission/replacement");
 timed("all-source-opening-handles",[&]{for(std::size_t j=0;j<sources.size();++j){auto&source=sources[j];auto label="Oneblock/source-binding/"+std::to_string(j);std::vector<Fr>point;for(unsigned k=0;k<r1::log2exact(source.padded);++k)point.push_back(io.coin(label+"/query/"+std::to_string(k)));auto h=exchangeHidden(io,label+"/value",isP?evaluateOracle(source,point):Fr(0),rng);source.open(io,{point,h.handle},isP?&h:nullptr,rng,label+"/opening");auto&spec=ctx.plan.sources[j];claims.push_back({"mapped/"+spec.spec.role,source.actual,source.padded,[&,j](std::size_t i){return sourceIndex(ctx.plan.sources[j],i).first;},h,point,Fr(0),[&,j](std::size_t i){return sourceIndex(ctx.plan.sources[j],i).second;}});}});
 timed("all-terminals-and-sources-to-one-master",[&]{authenticateMappedClaims(io,*registered,isP?&masterSecret:nullptr,master,claims,rng,"Oneblock/common-Master");});
 for(auto&id:terminals)checked(io,id);for(auto&source:ctx.plan.sources)checked(io,"mapped/"+source.spec.role);for(auto id:{"gkr/input-aggregation","gkr/input-aggregation-terminal","master/hidden-openings","model/C_W_model-to-master"})checked(io,id);for(auto&r:trust.regions)checked(io,"model/region/"+r.name);
 need(objects.size()==expected.size(),"Oneblock uncreated native auxiliary object");for(auto&o:ctx.objects)if(o.kind!=ObjectKind::BaseMaster)checked(io,"object/"+o.id+"/source");if(isP)need(witness.legacyCommitmentAbsent(),"Oneblock legacy nonhiding PC forbidden");if(!isP)io.complete();io.coin("Oneblock/all-mandatory-relations-verified/ack");io.save(true);
 struct rusage usage{};getrusage(RUSAGE_SELF,&usage);auto m=metrics();std::cerr<<"{\"status\":\"ONEBLOCK_INTERACTIVE_ACCEPT\",\"required_count\":"<<plan.required.size()<<",\"coverage_sha256\":\""<<identity(plan)<<"\",\"master_id\":\""<<master3::identity(pc)<<"\",\"registered_rows\":"<<registration.verified_rows<<",\"registration_response_bytes\":"<<registration.response_payload_bytes<<",\"GKR_rounds\":"<<pending.rounds<<",\"seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<",\"wire_bytes\":"<<pipe.wireBytes()<<",\"P_wire_bytes\":"<<pipe.proofBytes()<<",\"frames\":"<<pipe.frames()<<",\"peak_rss_kib\":"<<usage.ru_maxrss<<",\"MSM_calls\":"<<m.msm_calls<<",\"MSM_terms\":"<<m.msm_terms<<",\"production_gate\":\"BLOCKED_CRYPTOGRAPHIC_DESIGN\",\"may_enter_stage2\":false,\"phases\":[";for(std::size_t i=0;i<phases.size();++i){if(i)std::cerr<<',';auto&p=phases[i];std::cerr<<"{\"name\":\""<<p.name<<"\",\"seconds\":"<<p.seconds<<",\"P_wire_bytes\":"<<p.p_wire_bytes<<",\"MSM_calls\":"<<p.msm_calls<<'}';}std::cerr<<"]}\n";return 0;

#else
#include "classified_controller.inc"
#endif
 }catch(const std::exception&e){std::cerr<<"REJECT_OR_INCOMPLETE "<<e.what()<<'\n';return 2;}}
