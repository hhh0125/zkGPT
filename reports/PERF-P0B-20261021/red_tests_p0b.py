from pathlib import Path
import os,subprocess,json
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(r);os.sched_setaffinity(0,[0,1,2,3]);d=r/'reports/PERF-P0B-20261021';logs=r/'logs/PERF-P0B-20261021'
old=(d/'mapped_input.p0a.cpp').read_text()
def function(signature):
 begin=old.index(signature);at=old.index('{',begin);depth=1;end=at+1
 while depth:
  depth+=(old[end]=='{')-(old[end]=='}');end+=1
 return old[begin:end]
ref='namespace p0b_reference {\nusing namespace zkgpt::r1;\n'+'\n'.join(function(x) for x in ['Fr eqIndex(','Fr spanKernel(','Fr kernelValue('])+'\n}\n'
body=(d/'tests_mapped_kernel.body.cpp').read_text();assert '@@REFERENCE@@' in body
Path('src/paper_r1/tests_mapped_kernel.cpp').write_text(body.replace('@@REFERENCE@@',ref))
p=Path('src/paper_r1/CMakeLists.txt');s=p.read_text();assert 'p0b_kernel_tests' not in s
p.write_text(s+'\nadd_executable(p0b_kernel_tests tests_mapped_kernel.cpp)\ntarget_link_libraries(p0b_kernel_tests PRIVATE paper_r1)\nadd_test(NAME p0b_mapped_kernel COMMAND p0b_kernel_tests)\n')
for name,args in [('05-red-configure',['cmake','-S','.','-B','build/release']),('06-red-build',['cmake','--build','build/release','--target','p0b_kernel_tests','-j4'])]:
 p=subprocess.run(args,capture_output=True,text=True);(logs/(name+'.log')).write_text(p.stdout+p.stderr);assert p.returncode==0,(name,(p.stdout+p.stderr)[-2500:])
p=subprocess.run(['build/release/src/paper_r1/p0b_kernel_tests'],capture_output=True,text=True,timeout=120);(logs/'07-red-test.log').write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,flush=True)
assert p.returncode!=0 and 'P0B fast path counters not active' in p.stderr and 'MATH_PASS' in p.stdout
with (d/'progress.md').open('a') as f:f.write('Task2 RED: exact-field/span/fullkernel/dense/bias/bounds math passes on P0A; only expected fastpath-counter assertion fails. No productionkernel optimization yet.\n')
