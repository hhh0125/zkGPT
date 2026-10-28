from pathlib import Path
import os,sys,json,subprocess,shlex
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(r);os.sched_setaffinity(0,[0,1,2,3]);d=r/'reports/PERF-P0B-20261021';logs=r/'logs/PERF-P0B-20261021'
p=r/'src/paper_r1/tests_mapped_kernel.cpp';s=p.read_text();old='{auto p=randomPoint(rng,12,7);const auto old=p;const EqWeights cache(p);p[0]+=Fr(1);check(cache.at(0)==p0b_reference::eqIndex(old,0),"cache must own point values");}'
new='''{std::vector<Fr> p(12,Fr(2));const auto old=p;const EqWeights cache(p);p[0]+=Fr(1);
 check(cache.point()==old,"cache coordinate snapshot changed after source mutation");
 check(cache.at(0)==p0b_reference::eqIndex(old,0)&&cache.at(0)!=p0b_reference::eqIndex(p,0),"nonzero weight must use owned point values");
 check(cache.above(0,0)==p0b_reference::eqIndex(old,0),"above must use owned point values");}
 {auto c=randomPoint(rng,7,20),m=randomPoint(rng,7,21);
  auto before=p0b_test_hits;compareSpan(c,0,m,2,64);
  check(p0b_test_hits.contiguous==before.contiguous+1&&p0b_test_hits.fallback==before.fallback,"exactly32 dyadic blocks must use contiguous path");
  before=p0b_test_hits;compareSpan(c,0,m,2,66);
  check(p0b_test_hits.fallback==before.fallback+1&&p0b_test_hits.contiguous==before.contiguous,"33 dyadic blocks must fall back to original DP");
 }'''
assert old in s or new in s;p.write_text(s.replace(old,new))
(d/'INDEPENDENT_REVIEW.md').write_text('''Fresh read-only source review: no Critical or Important findings. Exact DP body, Batch/dense prover kernel/tail/prove/verify suffix unchanged. Dyadic identity, zero factors, unequal dimensions, LSB split indexing, owned cache lifetime sound. Two Minor test gaps corrected: nonvacuous owned-coordinate mutation plus nonzero weight, explicit count64/count66 offsets0/2 tests distinguishing32-block dyadic path from33-block fallback. Equality-cache counter counts table capacity only, not total allocations/RSS. Reviewer did not run tests or remote executables; all measured evidence remains separately logged. No realoneblock conclusion.\n''')
def build(name,args):
 p=subprocess.run(args,capture_output=True,text=True);(logs/(name+'.log')).write_text(shlex.join(args)+'\n'+p.stdout+p.stderr);assert p.returncode==0,(p.stdout+p.stderr)[-2000:];print(name,'PASS',flush=True)
sys.path.insert(0,str(r/'reports/R6B-OPT2-20261021'));import r6b_resources
sys.path.insert(0,str(d));from oneblock_resource_policy import check
ns={};exec(compile((d/'oneblock_supervisor.py').read_text(),'bounded test supervisor','exec'),ns);ns['r']=d;ns['logs']=logs
def run(name,args,mode=None):
 row=r6b_resources.snapshot(r);row['WARN_other_user_large_jobs']=check(row,8);(d/(name+'-resource.json')).write_text(json.dumps(row,indent=2))
 env={'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1:abort_on_error=1:quarantine_size_mb=64'} if mode=='asan' else {'UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'} if mode=='ubsan' else None
 ns['run'](name,args,timeout=300,mem_gib=None if mode=='asan' else 8,rss_gib=8,env=env,disk_gib=64,reserve_gib=32)
build('22b-review-fix-release-build',['cmake','--build','build/release','--target','p0b_kernel_tests','-j4'])
run('23-review-fix-release',['build/release/src/paper_r1/p0b_kernel_tests'])
common=['c++','-O1','-g','-fno-omit-frame-pointer','-std=c++17','-pthread','-DMCLBN_NO_AUTOLINK','-DMCL_NO_AUTOLINK','-DMCL_USE_LLVM=1','-Isrc/paper_r1','-I3rd/mcl/include']
runtime=['build/release/lib/libmclbn256.so.1.74','build/release/lib/libmcl.so.1.74','-Wl,-rpath,'+str(r/'build/release/lib')]
for mode,san in [('asan','address'),('ubsan','undefined')]:
 binary=d/mode/'kernel-tests';build('24-review-fix-'+mode+'-build',common+['-fsanitize='+san,'src/paper_r1/tests_mapped_kernel.cpp',str(d/mode/'libpaper_r1.a')]+runtime+['-o',str(binary)])
 run('25-review-fix-'+mode,[str(binary)],mode)
print('REVIEW_TEST_STRENGTHENING_PASS',flush=True)

