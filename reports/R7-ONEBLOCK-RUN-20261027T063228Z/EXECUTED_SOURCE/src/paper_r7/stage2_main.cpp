#include "stage2_driver.hpp"
#include <sys/prctl.h>
#include <sys/resource.h>
#include <unistd.h>
#include <chrono>
#include <optional>
#include <cstdlib>
#include <iostream>
using namespace zkgpt::r7;
namespace s2=zkgpt::r7::stage2;
namespace {std::string env(const char*name){auto v=std::getenv(name);crypto::need(v && *v,"S2 missing independent registry/private fixture configuration");return v;}}
int main(int argc,char**argv){try{
 crypto::need(argc>=3 && argc<=4,"usage: R7_S2 MODE K [P_ATTACK]");crypto::need(prctl(PR_SET_DUMPABLE,0)==0 && prctl(PR_GET_DUMPABLE)==0,"S2 process isolation failed");alarm(180);zkgpt::r1::init();auto start=std::chrono::steady_clock::now();std::string mode=argv[1],number=argv[2];crypto::need(number=="1" || number=="2" || number=="4","S2 K must be1/2/4");auto k=std::stoul(number);
#ifdef R7_S2_VERIFIER
 constexpr bool prover=false;crypto::need(argc==3,"S2 verifier cannot import prover challenge/receipt/options");std::string attack;
#else
 constexpr bool prover=true;std::string attack=argc==4?argv[3]:"";
#endif
 std::optional<s2::TrustedRegistry>trusted;std::optional<s2::PrivateFixture>fixture;
 if(prover)fixture=s2::decodePrivateFixture(s2::readOwnedFD(std::stoi(env("R7_STAGE2_PRIVATE_FD")),true));
 else trusted=s2::TrustedRegistry::load(std::stoi(env("R7_STAGE2_REGISTRY_FD")),env("R7_STAGE2_REGISTRY_DIGEST"));
 crypto::PipeIO io(prover,mode+"/"+number,attack,s2::suite);auto stats=s2::experiment(io,prover?nullptr:&*trusted,prover?&*fixture:nullptr,mode,k,attack);
 if(!prover){if(const char*fd=std::getenv("R7_PUBLIC_AUDIT_FD"))s2::writeOwnedFD(std::stoi(fd),io.publicTranscript());}
 struct rusage u{};getrusage(RUSAGE_SELF,&u);auto m=crypto::metrics();auto a=hyrax_a3::measurement();
 std::cerr<<"{\"status\":\"LOCAL_CRYPTO_TESTED\",\"role\":\""<<(prover?"P":"V")<<"\",\"mode\":\""<<mode<<"\",\"K\":"<<k<<",\"registry_digest\":\""<<stats.registry_digest<<"\",\"seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<",\"cpu_user_seconds\":"<<u.ru_utime.tv_sec+u.ru_utime.tv_usec/1e6<<",\"cpu_system_seconds\":"<<u.ru_stime.tv_sec+u.ru_stime.tv_usec/1e6<<",\"proof_bytes_sent\":"<<io.proofBytes()<<",\"peak_rss_kib\":"<<u.ru_maxrss<<",\"vector_msm_calls\":"<<m.msm_calls<<",\"vector_msm_terms\":"<<m.msm_terms<<",\"max_vector_msm\":"<<m.max_msm<<",\"guard_payload_bytes\":"<<a.guard_payload_bytes<<",\"core_payload_bytes\":"<<a.core_payload_bytes<<",\"witness_generation_included\":false,\"cached_real_R6_fixture\":true,\"C_W_model\":\"INCOMPLETE\",\"scalar_multiplications\":\"NOT_COUNTED\",\"joint_extraction\":\"UNPROVED\",\"oneblock_status\":\"INCOMPLETE\",\"may_enter_stage2\":false,\"phases\":[";
 bool first=true;auto emit=[&](const s2::Phase&p){if(!first)std::cerr<<',';first=false;std::cerr<<"{\"name\":\""<<p.name<<"\",\"wall_seconds\":"<<p.seconds<<",\"P_wire_bytes\":"<<p.p_wire_bytes<<",\"vector_msm_calls\":"<<p.msm_calls<<",\"PC_total_N\":"<<p.pc_total_N<<",\"A3_folded_n\":"<<p.folded_n<<'}';};
 for(auto&p:stats.phases)emit(p);for(auto&p:s2::openingMeasurements())emit(p);std::cerr<<"]}\n";return 0;
}catch(const std::exception&e){std::cerr<<"REJECT "<<e.what()<<'\n';return 2;}}
