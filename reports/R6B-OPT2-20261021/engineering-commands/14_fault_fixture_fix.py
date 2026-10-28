from pathlib import Path
import os,subprocess
r=Path.cwd();d=r/'reports/R6B-OPT2-20261021';l=r/'logs/R6B-OPT2-20261021';p=r/'src/main_opt2_allocation_tests.cpp';s=p.read_text();s=s.replace('int main(){try{initPairing','int main(int argc,char**){try{const bool diagnostic_mode=argc>1;if(diagnostic_mode)setenv("ZKGPT_MEMORY_DIAGNOSTICS","1",1);else unsetenv("ZKGPT_MEMORY_DIAGNOSTICS");initPairing')
a=s.index(' setenv("ZKGPT_MEMORY_DIAGNOSTICS","1",1);watched=');b=s.index(' std::cout<<"PASS actualoneMasterrequestremoved',a)
block=s[a:b].replace(' setenv("ZKGPT_MEMORY_DIAGNOSTICS","1",1);','').replace('unsetenv("ZKGPT_MEMORY_DIAGNOSTICS");','')
s=s[:a]+s[b:];marker='auto&p=fixture.proverInstance();';s=s.replace(marker,marker+'\n if(diagnostic_mode){p.commitInput(p.gens,1,true);auto expected=p.sharedInputWitness();'+block.replace('==copied','==expected')+'std::cout<<"PASS postpublicationdiagnosticbadallocsuppressed\\n";return 0;}')
s=s.replace(', postpublicationdiagnosticbadallocsuppressed\\n','\\n');p.write_text(s)
p=r/'src/CMakeLists.txt';p.write_text(p.read_text()+'\nadd_test(NAME opt2_postpublication_diagnostic_failure COMMAND opt2_allocation_tests --postpublish)\n')
os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
with (l/'fault-fixture-fix-build.log').open('x') as out:subprocess.run(['cmake','--build','build/release','-j','4'],stdout=out,stderr=subprocess.STDOUT,check=True,timeout=420)
with (l/'allocation-final.log').open('x') as out:
 for cmd in [['build/release/src/opt2_allocation_tests'],['build/release/src/opt2_allocation_tests','--postpublish']]:subprocess.run(cmd,stdout=out,stderr=subprocess.STDOUT,check=True,timeout=120)
print((l/'allocation-final.log').read_text()[-1400:])
