import subprocess
Path('src/paper_r7/full12_witness_probe.cpp').write_text(r'''#include "r6_adapter.hpp"
#include "execution_target.hpp"
#include "master_layout.hpp"
#include <sys/prctl.h>
#include <sys/resource.h>
#include <iostream>
int main(){try{
 using namespace zkgpt::r7;zkgpt::r1::init();crypto::need(prctl(PR_SET_DUMPABLE,0)==0,"private process isolation");
 const auto t=executionTarget(12);auto c=master3::trustedContext(12);
 std::cerr<<"FULL12_WITNESS_BEGIN blocks="<<c.plan.shape.layer_count<<" master_actual="<<c.plan.master.actual<<" legacy_commitment=disabled\n";
 R7WitnessStore w;w.generateR6(c,t.manifest_path);crypto::need(w.legacyCommitmentAbsent(),"legacy commitment forbidden");
 std::cerr<<"FULL12_WITNESS_VALIDATED_NO_PRIVATE_EXPORT\n";return 0;
}catch(const std::exception&e){std::cerr<<"FULL12_WITNESS_INCOMPLETE "<<e.what()<<'\n';return 2;}}
''')
p=Path('src/paper_r7/CMakeLists.txt');p.write_text(p.read_text()+'\nadd_executable(r7_full12_witness_probe full12_witness_probe.cpp)\ntarget_link_libraries(r7_full12_witness_probe PRIVATE paper_r7)\n')
subprocess.run(['cmake','-S','.','-B','build/release'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)
stage=Path('reports/R7-FULL12-20261029-v1')
with (stage/'witness-probe-build.log').open('xb') as out:r=subprocess.run(['cmake','--build','build/release','--target','r7_full12_witness_probe','-j4'],stdout=out,stderr=subprocess.STDOUT)
print(r.returncode,(stage/'witness-probe-build.log').read_text()[-1500:]);assert r.returncode==0
