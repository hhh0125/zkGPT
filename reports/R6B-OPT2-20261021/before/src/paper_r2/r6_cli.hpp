#pragma once
#include "r6_model.hpp"
#include "r6_system.hpp"
#include "r6_trust.hpp"
#include "r5b_cli.hpp"
#include "paper_r1/wire.hpp"
#include "thread_joiner.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <set>
#include <chrono>
#include <sys/resource.h>
#include <unistd.h>
#include <sched.h>
#include <malloc.h>
namespace zkgpt::r6 {
struct Options{unsigned blocks=0,cpus=4;std::size_t memory=0,seconds=0,disk=64;bool plan_only=false,execute=false;std::string anchor,pin,manifest,permit,permit_pin;};
inline std::size_t number(const std::string&s,std::size_t cap){r1::require(!s.empty(),"R6 missing integer option");std::size_t n=0;for(char c:s){r1::require(c>='0'&&c<='9',"R6 invalid unsigned integer option");r1::require(std::size_t(c-'0')<=cap && n<=(cap-std::size_t(c-'0'))/10,"R6 option exceeds bound");n=n*10+c-'0';}r1::require(n>0 && n<=cap,"R6 option outside positive bound");return n;}
inline Options options(int argc,char**argv,int start){Options o;std::set<std::string>seen;for(int i=start;i<argc;++i){std::string f=argv[i];r1::require(seen.insert(f).second,"duplicate R6 option");auto v=[&](){r1::require(i+1<argc,"missing R6 option value");return std::string(argv[++i]);};
 if(f=="--blocks")o.blocks=r2::parseModelBlocks(v());else if(f=="--trusted-anchor")o.anchor=v();else if(f=="--anchor-sha256")o.pin=v();else if(f=="--model-manifest")o.manifest=v();else if(f=="--memory-gib")o.memory=number(v(),256);else if(f=="--max-seconds")o.seconds=number(v(),86400);else if(f=="--disk-reserve-gib")o.disk=number(v(),1024);else if(f=="--cpu-limit")o.cpus=number(v(),8);else if(f=="--run-permit")o.permit=v();else if(f=="--run-permit-sha256")o.permit_pin=v();else if(f=="--plan-only")o.plan_only=true;else if(f=="--execute-full12")o.execute=true;else if(f=="--numerical-profile")r1::require(v()==r5b::kProfile,"R6 numerical profile must remain frozen R5B");else throw std::invalid_argument("unknown R6 option: "+f);}
 r1::require(o.blocks==1 || o.blocks==12,"R6 explicit --blocks1|12 required");return o;}
inline void requireExecution(const Options&o){if(o.blocks==12)r1::require(o.execute && !o.permit.empty() && o.permit_pin.size()==64 && o.memory && o.seconds,"R6 full12 execution requires separately authorized SHA-pinned run permit and explicit memory/time limits");r1::require(!o.anchor.empty() && o.pin.size()==64 && !o.manifest.empty(),"R6 independently pinned external anchor and manifest required");}
inline void limits(Options&o,bool planning){if(!o.memory)o.memory=planning?8:16;if(!o.seconds)o.seconds=planning?240:3600;
 auto threads=std::to_string(o.cpus);setenv("ZKGPT_THREADS",threads.c_str(),1);setenv("MALLOC_ARENA_MAX","2",1);mallopt(M_ARENA_MAX,2);
 cpu_set_t available,chosen;CPU_ZERO(&available);CPU_ZERO(&chosen);r1::require(sched_getaffinity(0,sizeof(available),&available)==0,"R6 CPU affinity read failed");unsigned n=0;for(int i=0;i<CPU_SETSIZE && n<o.cpus;++i)if(CPU_ISSET(i,&available)){CPU_SET(i,&chosen);++n;}r1::require(n && sched_setaffinity(0,sizeof(chosen),&chosen)==0,"R6 CPU affinity limit failed");
 struct rlimit old;r1::require(getrlimit(RLIMIT_AS,&old)==0,"R6 inherited address-space limit unreadable");rlim_t cap=rlim_t(o.memory)<<30;r1::require(old.rlim_max==RLIM_INFINITY || old.rlim_max>=cap,"R6 inherited hard address-space limit is below requested budget");struct rlimit selected{cap,cap};r1::require(setrlimit(RLIMIT_AS,&selected)==0,"R6 address-space limit failed");alarm(o.seconds);
}
inline void newFile(const std::string&file){auto root=std::filesystem::canonical(std::filesystem::current_path()),target=std::filesystem::weakly_canonical(std::filesystem::absolute(file));auto relative=target.lexically_relative(root);r1::require(!relative.empty() && *relative.begin()!=".." && !std::filesystem::exists(target),"R6 output must be new and inside workspace");}
inline std::size_t capacityBytes(const layeredCircuit&c){std::size_t n=0;for(auto&l:c.circuit)n+=l.uni_gates.capacity()*sizeof(uniGate)+l.bin_gates.capacity()*sizeof(binGate)+(l.ori_id_u.capacity()+l.ori_id_v.capacity())*sizeof(u32);return n;}
struct Resources{std::size_t graph,witness,commit,prove,verify,serialize,disk;};
inline Resources resources(const layeredCircuit&c,const LLM&nn,const r2::ModelQueryPlan&p){auto shared=r2::modelResources(c,p);auto master=p.master.padded*sizeof(Fr),graph=capacityBytes(c);std::size_t initial=32768,layers=0,maxq=0;for(auto&r:nn.parameterRegions())if(r.name.rfind("fc_",0)==0)initial=std::max(initial,r.offset+r.count);for(std::size_t i=1;i<c.circuit.size();++i)layers+=(std::size_t(1)<<c.circuit[i].bit_length)*sizeof(Fr);for(auto&r:p.ranges)maxq=std::max(maxq,p.sources[r.child].spec.padded);
 auto val_capacity=std::max(2*initial*sizeof(Fr),master);auto matrix=p.sources.back().spec.actual*sizeof(std::int32_t);auto live=val_capacity+layers+matrix+graph;
 Resources r;r.graph=graph+512*1024*1024;r.witness=live+24*p.master.actual;r.commit=live+master+(std::size_t(2)<<30);
 r.prove=shared.lookup_prover_bytes+val_capacity+layers+matrix+2*master+3*graph+(std::size_t(2)<<30);r.prove=r.prove*5/4;
 r.verify=std::max(shared.verifier_peak_bytes,4*master+3*graph+maxq*128+4*shared.proof_upper_bytes);r.verify=r.verify*5/4;
 r.serialize=r.prove+4*shared.proof_upper_bytes;r.disk=4*shared.proof_upper_bytes+64*1024*1024;return r;}
inline void runtimeCheck(const Options&o){r1::require(effectiveRoom().memory>=(std::size_t(32)<<30) && effectiveRoom().pids>=64,"R6 shared-server available reserve below32GiB");r1::require(std::filesystem::space(".").available>=(o.disk<<30),"R6 disk reserve exhausted");}
inline void requirePermit(const Options&o,const r2::ModelQueryPlan&p,const std::string&mode){if(o.blocks!=12)return;auto b=r1::readFile(o.permit);r1::require(b.size()<8192 && r5b::digestHex(b)==o.permit_pin,"R6 independent run permit SHA256 mismatch");std::istringstream in(std::string(b.begin(),b.end()));auto next=[&](){std::string s;r1::require(bool(in>>s),"R6 truncated run permit");return s;};r1::require(next()=="ZKGPT-R6-RUN-PERMIT-v1","R6 run permit version");auto field=[&](const std::string&key,const std::string&expected){r1::require(next()==key && next()==expected,"R6 run permit mismatch: "+key);};field("mode",mode);field("blocks","12");field("profile-sha256",r5b::profileHash());field("circuit",p.circuit_id);field("anchor-sha256",o.pin);field("manifest-sha256",r5b::digestHex(r1::readFile(o.manifest)));field("binary-sha256",r5b::digestHex(r1::readFile("/proc/self/exe")));field("memory-gib",std::to_string(o.memory));field("wall-seconds",std::to_string(o.seconds));field("disk-reserve-gib",std::to_string(o.disk));field("cpu-limit",std::to_string(o.cpus));std::string tail;r1::require(!(in>>tail),"R6 trailing run permit data");}
inline void preflight(const Options&o,const Resources&r,const r2::ModelQueryPlan&p,const std::string&mode){requirePermit(o,p,mode);auto peak=mode=="prover"?std::max(r.prove,r.serialize):r.verify;r1::require(peak<=(o.memory<<30),"R6 actual-builder conservative peak exceeds selected budget");r1::require(peak+(std::size_t(32)<<30)<=effectiveRoom().memory,"R6 conservative peak plus32GiB reserve unavailable");runtimeCheck(o);}
inline void plan(const std::string&out,Options o){newFile(out);limits(o,true);LLM nn(o.blocks,12,64,768,3072);configure(nn);auto c=nn.buildPublicCircuit();auto p=r2::planModelQueries(c,nn.getWitnessRegistry(),nn.parameterRegions(),r1::ExpProfile::LegacyClampOne);std::ofstream f(out);writePlan(f,c,nn,p);}
}
