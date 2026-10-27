#include "hyrax_a3.hpp"
#include "hyrax_a3_driver.hpp"
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <chrono>
#include <cstdlib>
using namespace zkgpt::r7;
int main(int argc,char**argv){try{
 crypto::need(argc>=3 && argc<=4,"usage: R7_A3 MODE N [P_ATTACK]");
 crypto::need(prctl(PR_SET_DUMPABLE,0)==0 && prctl(PR_GET_DUMPABLE)==0,"R7 process memory isolation failed");alarm(120);zkgpt::r1::init();
 std::string mode=argv[1],number=argv[2];crypto::need(!number.empty() && number.size()<=4 && number.find_first_not_of("0123456789")==std::string::npos,"A3 numeric bound");auto n=std::stoul(number);
#ifdef R7_A3_VERIFIER
 crypto::need(argc==3,"A3 verifier cannot import prover challenge/receipt/options");constexpr bool prover=false;std::string attack;
#else
 constexpr bool prover=true;std::string attack=argc==4?argv[3]:"";
#endif
 auto start=std::chrono::steady_clock::now();crypto::PipeIO io(prover,mode+"/"+number,attack,hyrax_a3::suite);hyrax_a3::experiment(io,mode,n,attack);
 struct rusage usage{};getrusage(RUSAGE_SELF,&usage);auto m=crypto::metrics();auto a=hyrax_a3::measurement();
 if(!prover){if(const char*raw=std::getenv("R7_PUBLIC_AUDIT_FD")){std::string value=raw;crypto::need(!value.empty() && value.size()<=4 && value.find_first_not_of("0123456789")==std::string::npos,"A3 audit FD");int fd=std::stoi(value);struct stat st{};crypto::need(fd>2 && fstat(fd,&st)==0 && S_ISREG(st.st_mode) && st.st_uid==getuid(),"A3 audit requires launcher-owned regular file");const auto&b=io.publicTranscript();std::size_t at=0;while(at<b.size()){auto count=write(fd,b.data()+at,b.size()-at);crypto::need(count>0,"A3 audit write failed");at+=std::size_t(count);}}}
 std::cerr<<"{\"status\":\"LOCAL_CRYPTO_TESTED\",\"backend\":\""<<hyrax_a3::suite<<"\",\"role\":\""<<(prover?"P":"V")<<"\",\"mode\":\""<<mode<<"\",\"seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<",\"cpu_user_seconds\":"<<usage.ru_utime.tv_sec+usage.ru_utime.tv_usec/1e6<<",\"cpu_system_seconds\":"<<usage.ru_stime.tv_sec+usage.ru_stime.tv_usec/1e6<<",\"proof_bytes_sent\":"<<io.proofBytes()<<",\"peak_rss_kib\":"<<usage.ru_maxrss<<",\"vector_msm_calls\":"<<m.msm_calls<<",\"vector_msm_terms\":"<<m.msm_terms<<",\"max_vector_msm\":"<<m.max_msm<<",\"guard_payload_bytes\":"<<a.guard_payload_bytes<<",\"core_payload_bytes\":"<<a.core_payload_bytes<<",\"bound_public_input_bytes\":"<<a.public_payload_bytes<<",\"guard_wall_seconds\":"<<a.guard_seconds<<",\"core_wall_seconds\":"<<a.core_seconds<<",\"honest_interactive_only\":true,\"joint_extraction\":\"UNPROVED\",\"oneblock_status\":\"INCOMPLETE\",\"may_enter_stage2\":false}\n";return 0;
}catch(const std::exception&e){std::cerr<<"REJECT "<<e.what()<<'\n';return 2;}}
