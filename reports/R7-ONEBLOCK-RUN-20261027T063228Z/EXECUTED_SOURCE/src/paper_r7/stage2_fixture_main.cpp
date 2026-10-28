#include "stage2_fixture.hpp"
#include "r6_adapter.hpp"
#include <sys/prctl.h>
#include <sys/resource.h>
#include <unistd.h>
#include <fcntl.h>
#include <chrono>
#include <cstdio>
#include <iostream>
using namespace zkgpt::r7;
namespace s2=zkgpt::r7::stage2;
namespace {
struct Silence{int save;Silence(){std::fflush(stdout);save=dup(1);int sink=open("/dev/null",O_WRONLY);crypto::need(save>=0 && sink>=0 && dup2(sink,1)>=0,"S2 witness output isolation");close(sink);}~Silence(){std::fflush(stdout);dup2(save,1);close(save);}};
}
int main(int argc,char**argv){try{
 crypto::need(argc==5,"S2 fixture requires private/public/alternate/bad-tail output FDs");crypto::need(prctl(PR_SET_DUMPABLE,0)==0,"S2 fixture process isolation");alarm(180);zkgpt::r1::init();auto start=std::chrono::steady_clock::now();R7WitnessStore store;R7Context context;R7WitnessStore::FCContraction values;
 {Silence quiet;checkManifest("reports/R6A/oneblock-model-manifest-v1.txt",s2::manifestPin);context=buildR6Context(s2::manifestPin);auto out=R7OneblockProver{}.run(context,"reports/R6A/oneblock-model-manifest-v1.txt",store);crypto::need(out.result.code==Code::Incomplete,"S2 real R6 witness result");values=store.firstFCContraction(context);}
 const auto witnessSeconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();crypto::OSRandom rng;s2::PrivateFixture f;f.activation=values.activation;f.output=values.output;f.weight=crypto::commit(crypto::PCSpec::make(s2::componentId,1024,1024),values.weight,rng);f.primary=s2::registry(f.weight.public_pc);auto alt=values.weight;alt[0]+=Fr(1);f.alternate_weight=crypto::commit(crypto::PCSpec::make(std::string(s2::componentId)+"/alternate",1024,1024),alt,rng);f.alternate=s2::registry(f.alternate_weight.public_pc);
 auto badTail=values.weight;badTail[1000]+=Fr(1);f.bad_tail_weight=crypto::commit(crypto::PCSpec::make(s2::componentId,1024,1024),badTail,rng);f.bad_tail=s2::registry(f.bad_tail_weight.public_pc);
 s2::writeOwnedFD(std::stoi(argv[1]),s2::encodePrivateFixture(f),true);s2::writeOwnedFD(std::stoi(argv[2]),s2::encodeRegistry(f.primary));s2::writeOwnedFD(std::stoi(argv[3]),s2::encodeRegistry(f.alternate));s2::writeOwnedFD(std::stoi(argv[4]),s2::encodeRegistry(f.bad_tail));struct rusage u{};getrusage(RUSAGE_SELF,&u);
 std::cout<<"{\"fixture\":\"TRUSTED_TEST_ONLY_NOT_MODEL_AUTHENTICATION\",\"registry_digest\":\""<<s2::registryDigest(f.primary)<<"\",\"alternate_digest\":\""<<s2::registryDigest(f.alternate)<<"\",\"C_W_model\":\"INCOMPLETE\",\"PC_total_N\":1024,\"A3_folded_n\":32,\"witness_generation_seconds\":"<<witnessSeconds<<",\"setup_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<",\"peak_rss_kib\":"<<u.ru_maxrss<<",\"private_material_exported\":false}\n";return 0;
}catch(...){std::cerr<<"REJECT S2 private fixture generation failed; private diagnostics suppressed\n";return 2;}}
