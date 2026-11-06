from pathlib import Path
import json,os,shlex,subprocess,time,traceback
exec((Path(__file__).with_name('run_stage_v3.py')).read_text().split('mode=sys.argv[1]')[0])
# Every bounded process has the declared 60s wall/120s CPU/4GiB budget.
def limits(test=True,full_ctest=False):
 os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4]);resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_AS,(4<<30,)*2);resource.setrlimit(resource.RLIMIT_CPU,(120,)*2)
def parse(name,prefix):
 return [json.loads(x[len(prefix):]) for x in (BASE/(name+'.log')).read_text().splitlines() if x.startswith(prefix)]
try:
 assert json.loads((BASE/'K-ready-SUCCESS.json').read_text())['success']
 before=identity();save('accept-K-run-IDENTITY_BEFORE.json',before)
 # The old class header is adjacent to this frozen source, ahead of -I paths.
 flags=(BUILD/'src/paper_r7/CMakeFiles/r7_perf2_protocol.dir/flags.make').read_text()
 options=[]
 for line in flags.splitlines():
  if line.startswith(('CXX_DEFINES =','CXX_INCLUDES =','CXX_FLAGS =')):options+=shlex.split(line.split('=',1)[1])
 obj=BASE/'BASELINE-protocol.o';exe=BASE/'BASELINE-protocol'
 run('baseline-fixture-compile',['/usr/bin/c++',*options,'-c',BASE/'BASELINE-perf2_protocol.cpp','-o',obj],60)
 link=shlex.split((BUILD/'src/paper_r7/CMakeFiles/r7_perf2_protocol.dir/link.txt').read_text())
 cwd=BUILD/'src/paper_r7';args=[]
 for token in link:
  if token=='CMakeFiles/r7_perf2_protocol.dir/perf2_protocol.cpp.o':token=str(obj)
  elif token=='r7_perf2_protocol':token=str(exe)
  elif token=='libpaper_r7.a':token=str(BASE/'BASELINE-libpaper_r7.a')
  elif token.endswith(('.a','.so.1.74')) and not token.startswith('/'):token=str((cwd/token).resolve())
  args.append(token)
 run('baseline-fixture-link',args,60)
 new=BUILD/'src/paper_r7/r7_perf2_protocol'
 save('FIXTURE_BINARY_IDENTITY.json',{str(p):sha(p) for p in [exe,new,obj,BASE/'classified_commitments.hpp',BASE/'BASELINE-perf2_protocol.cpp',BASE/'BASELINE-libpaper_r7.a']})
 n='524288';oldproof=BASE/'bounded-before.proof';newproof=BASE/'bounded-after.proof'
 save('MEASUREMENT_LOAD_BEFORE.json',{'load':os.getloadavg(),'available_memory':Path('/proc/meminfo').read_text(),'affinity':sorted(os.sched_getaffinity(0))})
 run('bounded-P-before',[exe,'prove',oldproof,n],60)
 run('bounded-P-after',[new,'prove',newproof,n],60)
 assert oldproof.read_bytes()==newproof.read_bytes(),'STOP_CORRECTNESS deterministic proof bytes'
 save('PROOF_IDENTITY.json',{'bytes':newproof.stat().st_size,'before_sha256':sha(oldproof),'after_sha256':sha(newproof),'equal':True})
 for label,v,p in [('old-old',exe,oldproof),('old-new',exe,newproof),('new-old',new,oldproof),('new-new',new,newproof)]:run('cross-'+label,[v,'verify',p,n],60)
 b=newproof.read_bytes()
 for label,data in [('truncated',b[:-1]),('appended',b+b'X'),('mutated',b[:len(b)//2]+bytes([b[len(b)//2]^1])+b[len(b)//2+1:])]:
  p=BASE/('negative-'+label+'.proof');p.write_bytes(data);run('file-negative-'+label,[new,'verify',p,n],60,True,1)
 # One fixture only. Gate immediately, before any further fixture or IPA.
 run('bounded-V-ab',[new,'ab',newproof,n],60)
 ab=parse('bounded-V-ab','AB ')[0]
 pbefore=parse('bounded-P-before','WHOLE ')[0];pafter=parse('bounded-P-after','WHOLE ')[0]
 passed=ab['paired_ratio_median']>=1.05 and ab['paired_ratio_min']>=0.98 and (ab['paired_ratio_max']-ab['paired_ratio_min'])/ab['paired_ratio_median']<=.15 and pafter['total_seconds']<=pbefore['total_seconds']*1.10
 save('K_PERFORMANCE_GATE_RESULT.json',{'passed':passed,'ab':ab,'P_before':pbefore,'P_after':pafter,'P_single_sample_limitation':True,'decision':'ACCEPT_K' if passed else 'STOP_PERFORMANCE','next_fixture_allowed':passed})
 if not passed:raise RuntimeError('STOP_PERFORMANCE; no more fixture or IPA; disable candidate')
 ENV['ZKGPT_OPT2_METRICS']='1'
 for label,reference in [('reference','1'),('candidate','0')]:
  ENV['ZKGPT_PERF2_K_REFERENCE']=reference;run('profile-K-'+label,[new,'verify',newproof,n],60)
 after=identity();save('accept-K-run-IDENTITY_AFTER.json',after);assert before==after,'STOP_SYNC inputs changed'
 save('accept-K-run-SUCCESS.json',{'success':True})
except BaseException as e:
 save('accept-K-run-FAILURE.json',{'error':str(e),'traceback':traceback.format_exc()});raise
