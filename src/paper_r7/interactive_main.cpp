#include "interactive.hpp"
#include "framework.hpp"
#include <sys/prctl.h>
#include <sys/resource.h>
#include <unistd.h>
#include <iostream>
#include <chrono>
using namespace zkgpt::r7;
int main(int argc,char**argv){try{
 crypto::need(argc>=3 && argc<=4,"usage: R7_LOCAL MODE N [P_ATTACK]");
 crypto::need(prctl(PR_SET_DUMPABLE,0)==0 && prctl(PR_GET_DUMPABLE)==0,"R7 process memory isolation failed");alarm(300);zkgpt::r1::init();
 std::string mode=argv[1],number=argv[2];crypto::need(!number.empty() && number.size()<=4 && number.find_first_not_of("0123456789")==std::string::npos,"R7 numeric bound");auto n=std::stoul(number);crypto::need(n<=1024,"R7 interactive requested size bound");
#ifdef R7_INTERACTIVE_VERIFIER
 crypto::need(argc==3,"R7 verifier cannot import prover challenge/receipt/options");constexpr bool prover=false;std::string attack;
#else
 constexpr bool prover=true;std::string attack=argc==4?argv[3]:"";
#endif
 auto start=std::chrono::steady_clock::now();crypto::PipeIO io(prover,mode+"/"+number,attack);Result result;
 if(prover)result=R7OneblockProver{}.runLocalInteractive(io,mode,n,attack);else result=R7OneblockVerifier{}.verifyLocalInteractive(io,mode,n);
 crypto::need(result.code==Code::Ok,"R7 local protocol incomplete");struct rusage usage{};getrusage(RUSAGE_SELF,&usage);auto m=crypto::metrics();
 std::cerr<<"{\"status\":\"LOCAL_CRYPTO_TESTED\",\"role\":\""<<(prover?"P":"V")<<"\",\"mode\":\""<<mode<<"\",\"seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<",\"proof_bytes_sent\":"<<io.proofBytes()<<",\"peak_rss_kib\":"<<usage.ru_maxrss<<",\"vector_msm_calls\":"<<m.msm_calls<<",\"vector_msm_terms\":"<<m.msm_terms<<",\"max_vector_msm\":"<<m.max_msm<<",\"honest_interactive_only\":true,\"joint_extraction\":\"PARTIAL\",\"oneblock_status\":\"INCOMPLETE\",\"may_enter_stage2\":false}\n";return 0;
}catch(const std::exception&e){std::cerr<<"REJECT "<<e.what()<<'\n';return 2;}}
