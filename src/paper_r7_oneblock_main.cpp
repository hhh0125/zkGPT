#include "paper_r7_wire.hpp"
#include "paper_r7_native_masked.hpp"
#include "paper_r7_source_mle.hpp"
#include "paper_r7_oneblock_plan.hpp"
#include "paper_r7_classified_commitments.hpp"
#include "paper_r7_oneblock_vectors.hpp"
#include "paper_r7_oneblock_gkr.hpp"
#include "paper_r7_oneblock_lookup.hpp"
#include "paper_r7_oneblock_model.hpp"
#include "paper_r7_fs_io.hpp"
#include "paper_r7_execution_target.hpp"
#ifdef R7_FS_FULL12
#include "paper_r7_full12_model.hpp"
#endif
#include "paper_r2_r6_model.hpp"
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
#if !defined(R7_CLASSIFIED_V2) || !defined(R7_UNIFIED_INPUT1) || !defined(R7_SINGLE_HYRAX) || !defined(R7_FS_ONEBLOCK)
#error This entrypoint requires Single Hyrax + Unified Input + FS
#endif
namespace zkgpt::r7::oneblock {
using PublicEntries=std::vector<std::pair<std::size_t,Fr>>;
PublicEntries trustedPublicEntries(const R7Statement&s,const R7Context&context){
 const auto&plan=context.plan;
 need(s.y.size()==plan.output.size(),"Oneblock public output shape");
 PublicEntries entries;
 entries.reserve(s.y.size()+plan.constants.size()+plan.reserved_zero.size());
 for(std::size_t i=0;i<s.y.size();++i){
  need(plan.output[i]<plan.master.actual,"Oneblock public Y source bounds");
  entries.emplace_back(plan.output[i],s.y[i]);
 }
 for(auto i:plan.constants){need(i<plan.master.actual,"Oneblock public one source bounds");entries.emplace_back(i,Fr(1));}
 for(auto i:plan.reserved_zero){need(i<plan.master.actual,"Oneblock public zero source bounds");entries.emplace_back(i,Fr(0));}
 return entries;
}
}
using namespace zkgpt::r7;using namespace oneblock;using namespace crypto;namespace r6=zkgpt::r6;namespace r2=zkgpt::r2;
namespace {
#ifdef R7_FS_FULL12
constexpr auto liveSuite=zkgpt::r7::fs::singleFull12Suite;
#else
constexpr auto liveSuite=zkgpt::r7::fs::singleSuite;
#endif
constexpr auto coverageAccepted=zkgpt::r7::fs::verifiedStatus;
int fd(const char*n){auto p=std::getenv(n);need(p && *p,"Oneblock missing independent FD");return std::stoi(p);}
Bytes publicStatement(int f){struct stat st{};auto flags=fcntl(f,F_GETFL);need(f>2 && fstat(f,&st)==0 && S_ISREG(st.st_mode) && st.st_uid==getuid() && st.st_size>0 && std::uint64_t(st.st_size)<=R7Wire::maxStatementBytes && flags>=0 && (flags&O_ACCMODE)==O_RDONLY,"Oneblock independent public statement FD invalid");Bytes b(st.st_size);std::size_t at=0;while(at<b.size()){auto n=pread(f,b.data()+at,b.size()-at,at);if(n<0 && errno==EINTR)continue;need(n>0,"Oneblock public statement read");at+=n;}return b;}
void output(int f,const std::string&s){std::size_t at=0;while(at<s.size()){auto n=::write(f,s.data()+at,s.size()-at);need(n>0,"Oneblock result write");at+=n;}}
// Coverage is verifier-local state. There is no peer-supplied coverage bitmap.
class Tracked final:public IO,public EvidenceSink {
 IO&base_;std::function<std::uint64_t()>frames_;const Coverage&plan_;std::set<std::string>expected_,done_;std::map<std::string,std::pair<std::uint64_t,std::string>>evidence_;std::string last_,attack_;bool changed_=false;mutable bool saved_=false;
 bool authenticated_=false;
 public:Tracked(IO&b,const Coverage&p,std::string attack,std::function<std::uint64_t()>frames):base_(b),frames_(std::move(frames)),plan_(p),attack_(std::move(attack)){for(auto&r:p.required)expected_.insert(r.id);}
 bool prover()const override{return base_.prover();}std::string session()const override{return base_.session();}Digest prefix()const override{return base_.prefix();}
 Bytes message(const std::string&label,const Bytes&v={})override{last_=label;if(!prover() || attack_.empty() || changed_)return base_.message(label,v);auto b=v;if(prover() && !changed_){
  if(attack_=="wrong-commitment" && label=="SingleHyrax/rows/0"){Reader rows{b};auto n=rows.number();Writer out;out.number(n);for(std::size_t j=0;j<n;++j){auto a=rows.point();if(!j)a+=parameters(1).scalar;out.point(a);}rows.end();b=out.b;changed_=true;}
  if(attack_=="missing-authentication" && (label=="UnifiedInput/all-claims-fixed" || label=="CategoryInput/all-claims-fixed")){close(1);throw std::runtime_error("Classified deliberate missing authentication");}
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
Fr evaluateOracle(const VectorOracle&v,const std::vector<Fr>&q){bool stream=true;if(const auto*p=std::getenv("ZKGPT_SOURCE_MLE_STREAM")){std::string text(p);need(text=="0" || text=="1","source MLE mode");stream=text=="1";}SourceMLEMetrics m;auto y=sourceMLE(v.padded,v.value,q,stream,&m);std::cerr<<"SOURCE_MLE {\"streamed\":"<<(stream?"true":"false")<<",\"padded\":"<<v.padded<<",\"weight_peak_bytes\":"<<m.weight_peak_bytes<<",\"values_read\":"<<m.values_read<<",\"Fr_multiplications\":"<<m.multiplications<<",\"Fr_additions\":"<<m.additions<<",\"weights_wall\":"<<m.weights_wall<<",\"dot_wall\":"<<m.dot_wall<<"}\n";return y;}
}
int main(int argc,char**argv){std::unique_ptr<Tracked>tracked;try{
 need(prctl(PR_SET_DUMPABLE,0)==0,"Oneblock process isolation");
#ifndef R7_FS_FULL12
 alarm(21600);
#endif
 native_masked::ScopedMode native_mode(native_masked::configuredMode());
 r1::init();auto started=std::chrono::steady_clock::now();
 need(argc>=4 && argc<=5,"FS usage prove|verify PROOF_PATH CONTEXT [P_ATTACK]");
 #ifdef R7_FS_FULL12
 auto publicArtifacts=buildR6PublicArtifacts(12);
#else
 auto publicArtifacts=buildR6PublicArtifacts();
#endif
 auto plan=discover(*publicArtifacts);auto&ctx=plan.context;
 auto layout=classified::makeCategoryLayout(ctx,*publicArtifacts);
 auto layout_id=classified::identity(layout);
 plan.required.erase(std::remove_if(plan.required.begin(),plan.required.end(),[](auto&r){return r.family=="OBJECT_COMMITMENT" || r.family=="OBJECT_SOURCE" || r.family.rfind("MASTER_",0)==0;}),plan.required.end());
 for(auto&r:plan.required)if(r.family=="MODEL_MASTER_LINK")r.id="model/C_W_model-to-classified-input";
 // Remove external model authority only in this explicitly different statement
 // scope. Native W mappings, all arithmetic and all parameter links remain.
 plan.required.erase(std::remove_if(plan.required.begin(),plan.required.end(),[](auto&r){return r.family=="MODEL_AUTHORITY" || r.family=="MODEL_RERANDOMIZATION";}),plan.required.end());
 plan.required.push_back({"model/frozen-numerical-profile","NUMERICAL_PROFILE",ctx.plan.circuit_id,0,0});
 plan.required.push_back({"model/weights-bound-in-C_U","MODEL_WITNESS_BINDING",single::modelScope,ctx.plan.sources.back().spec.actual,0});
 plan.required.push_back({"single/commitment","SINGLE_HYRAX_COMMITMENT",layout_id,ctx.plan.master.actual,0});
 plan.required.push_back({"single/direct-logical-domain","SINGLE_HYRAX_DIRECT_LOGICAL_DOMAIN",layout_id,ctx.plan.master.actual,0});
 for(auto&[id,v]:layout.views){plan.required.push_back({"classified/view/"+id,"CLASSIFIED_VIEW",id,v.padded,0});}
 plan.required.push_back({"unified-input/sumcheck","UNIFIED_INPUT_SUMCHECK",layout_id,ctx.plan.master.actual,0});
 plan.required.push_back({"unified-input/classified-opening","UNIFIED_INPUT_OPENING",layout_id,ctx.plan.master.actual,0});
 plan.required.push_back({"single/hidden-opening","SINGLE_HYRAX_OPENING",layout_id,ctx.plan.master.actual,0});
#ifdef R7_COMPLETE_VERIFIER
 constexpr bool isP=false;
 need(argc==4,"FS verifier cannot import challenges/receipt/options");
#else
 constexpr bool isP=true;
#endif
 need(std::string(argv[1])==(isP?"prove":"verify"),"FS role/mode mismatch");
 std::string attack=isP && argc==5?argv[4]:"";
 [[maybe_unused]] const char*successStatus=isP?"FS_PROOF_CREATED_CANDIDATE":fs::verifiedStatus;
 auto statement_bytes=publicStatement(fd("R7_ONEBLOCK_STATEMENT_FD"));auto s=R7Wire::decodeStatement(statement_bytes);need(validateStatement(s,ctx).code==Code::Ok,"Oneblock invalid frozen public statement");
 // Each process independently owns the public statement. Test harness mutates
 // both public copies for wrong X/Y, rather than testing only byte mismatch.
 need(R7Wire::encodeStatement(s)==statement_bytes,"FS noncanonical external statement");
 const auto coverage_id=identity(plan);
 Writer instance;instance.text(liveSuite);instance.text(coverage_id);instance.text(layout_id);
 instance.text(classified::unifiedInputIdentity(layout,s.x));
 instance.text(single::modelScope);instance.raw(single::descriptor(classified::singlePlan(layout,s.x)));
instance.raw(statement_bytes);if(native_masked::enabled())instance.text(native_masked::suite);
 fs::Binding fsBinding{
#ifdef R7_FS_FULL12
 native_masked::enabled()?"Full12NativeMasked":"Full12SingleHyrax",
#else
 native_masked::enabled()?"OneblockNativeMasked":"OneblockSingleHyrax",
#endif
 argv[3],fs::generatorIdentity({},true),instance.b};
#ifdef R7_COMPLETE_VERIFIER
 fs::FsVerifierIO pipe(argv[2],fsBinding);
#else
 fs::FsProverIO pipe(argv[2],fsBinding);
#endif
 tracked=std::make_unique<Tracked>(pipe,plan,attack,[&pipe]{return pipe.frames();});auto&io=*tracked;struct EvidenceGuard{Tracked*t;~EvidenceGuard(){try{t->save(false);}catch(...){}}} guard{tracked.get()};SessionRandom random;Random*rng=isP?&random:nullptr;
 Writer binding;binding.text(liveSuite);binding.text(coverage_id);binding.raw(statement_bytes);bindPublic(io,"Oneblock/statement-native-plan",binding.b);
 std::vector<stage2::Phase>phases;auto timed=[&](const std::string&id,const auto&fn){auto t=std::chrono::steady_clock::now();[[maybe_unused]] auto cpuStarted=processCPU();auto bytes=pipe.proofBytes(),calls=metrics().msm_calls;std::cerr<<"PHASE_BEGIN "<<id<<std::endl;fn();phases.push_back({id,std::chrono::duration<double>(std::chrono::steady_clock::now()-t).count(),pipe.proofBytes()-bytes,metrics().msm_calls-calls});struct rusage phaseUsage{};getrusage(RUSAGE_SELF,&phaseUsage);phases.back().peak_rss_kib=phaseUsage.ru_maxrss;std::ifstream statm("/proc/self/statm");std::uint64_t pages=0,resident=0;if(statm>>pages>>resident)phases.back().rss_kib=resident*std::uint64_t(sysconf(_SC_PAGESIZE))/1024;std::cerr<<"PHASE_COMPLETE "<<id<<'\n';
#ifdef R7_FS_FULL12
 std::cerr<<"PHASE_CPU {\"name\":\""<<id<<"\",\"cpu_seconds\":"<<processCPU()-cpuStarted<<"}\n";
#endif
};
#include "paper_r7_classified_controller.inc"
 }catch(const std::exception&e){std::cerr<<"REJECT_OR_INCOMPLETE "<<e.what()<<'\n';return 2;}}
