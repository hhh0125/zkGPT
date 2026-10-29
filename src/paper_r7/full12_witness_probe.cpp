#include "r6_adapter.hpp"
#include "execution_target.hpp"
#include "master_layout.hpp"
#include <sys/prctl.h>
#include <sys/resource.h>
#include <iostream>
#include <fstream>
#include <malloc.h>
int main(){try{
 using namespace zkgpt::r7;zkgpt::r1::init();crypto::need(prctl(PR_SET_DUMPABLE,0)==0,"private process isolation");
 const auto t=executionTarget(12);auto c=master3::trustedContext(12);
 std::cerr<<"FULL12_WITNESS_BEGIN blocks="<<c.plan.shape.layer_count<<" master_actual="<<c.plan.master.actual<<" legacy_commitment=disabled\n";
 auto memory=[](const char*stage){std::ifstream in("/proc/self/status");std::string line;while(std::getline(in,line))if(line.rfind("VmSize:",0)==0 || line.rfind("VmRSS:",0)==0)std::cerr<<stage<<" "<<line<<'\n';};memory("FULL12_BEFORE_TRIM");malloc_trim(0);memory("FULL12_AFTER_TRIM");
 R7WitnessStore w;{master3::QuietStdout quiet;w.generateR6(c,t.manifest_path);}crypto::need(w.legacyCommitmentAbsent(),"legacy commitment forbidden");
 std::cerr<<"FULL12_WITNESS_VALIDATED_NO_PRIVATE_EXPORT\n";return 0;
}catch(const std::exception&e){std::cerr<<"FULL12_WITNESS_INCOMPLETE "<<e.what()<<'\n';return 2;}}
