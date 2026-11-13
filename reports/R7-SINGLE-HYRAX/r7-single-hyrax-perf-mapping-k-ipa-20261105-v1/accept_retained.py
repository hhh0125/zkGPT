from pathlib import Path
BASE=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/r7-single-hyrax-perf-mapping-k-ipa-20261105-v1')
exec((BASE/'run_stage_v2.py').read_text().split('mode=sys.argv[1]')[0])
import statistics,shutil
assert json.loads((BASE/'retained-default-SUCCESS.json').read_text())['success']
original_limits=limits
def limits(test):
 original_limits(test)
 if test:resource.setrlimit(resource.RLIMIT_CPU,(120,120))
before=identity();save('retained-accept-IDENTITY_BEFORE.json',before)
BIN=BUILD/'src/paper_r7'
try:
 # The same test TU is linked to the preserved baseline library; original old
 # production objects were compiled before any algorithm edits. No source copy.
 command=(BIN/'CMakeFiles/r7_zero_suffix_storage_tests.dir/link.txt').read_text()
 import shlex
 args=shlex.split(command);args=[str(BIN/a) if a.startswith(('CMakeFiles/','../')) or a=='libpaper_r7.a' else a for a in args]
 at=args.index('-o');args[at+1]=str(BASE/'baseline-fixture-retained')
 args=[str(BASE/'BASELINE-libpaper_r7.a') if a==str(BIN/'libpaper_r7.a') else a for a in args]
 run('retained-baseline-link',args,60,False)
 proof0=BASE/'retained-baseline.fsproof';proof1=BASE/'retained-optimized.fsproof'
 run('retained-baseline-prove',['/usr/bin/time','-v',BASE/'baseline-fixture-retained','prove',proof0],60)
 run('retained-optimized-prove',['/usr/bin/time','-v',BIN/'r7_zero_suffix_storage_tests','prove',proof1],60)
 assert proof0.read_bytes()==proof1.read_bytes(),'STOP_CORRECTNESS deterministic proof differs'
 run('retained-optimized-verify-old',[BIN/'r7_zero_suffix_storage_tests','verify',proof0],60)
 run('retained-optimized-verify-new',[BIN/'r7_zero_suffix_storage_tests','verify',proof1],60)
 run('retained-baseline-verify-new',[BASE/'baseline-fixture-retained','verify',proof1],60)
 save('RETAINED_PROOF_COMPARISON.json',{'equal':True,'sha256':sha(proof1),'bytes':proof1.stat().st_size,'baseline_accepts_new':True,'new_accepts_baseline':True,'strict_EOF':True,'real_bounded_Single_protocol_fixture':True,'official_model_proof':False})
 for kind,data in [('truncated',proof1.read_bytes()[:-1]),('trailing',proof1.read_bytes()+b'\0'),('changed',proof1.read_bytes()[:len(proof1.read_bytes())//2]+bytes([proof1.read_bytes()[len(proof1.read_bytes())//2]^1])+proof1.read_bytes()[len(proof1.read_bytes())//2+1:])]:
  path=BASE/('retained-'+kind+'.fsproof');path.write_bytes(data)
  run('retained-negative-'+kind,[BIN/'r7_zero_suffix_storage_tests','verify',path],60,True,1)
 save('RETAINED_BOUNDED_NEGATIVE.json',{'truncated':'REJECT','trailing':'REJECT','changed':'REJECT','mechanisms':'See raw logs; program catches exception and returns 1, never crash/timeout'})
 after=identity();save('retained-accept-IDENTITY_AFTER.json',after);assert before==after
 save('retained-accept-SUCCESS.json',{'success':True,'performance_stop_retained':True,'scan_candidate_default_enabled':False,'storage_optimization_retained':True})
except BaseException as e:save('retained-accept-FAILURE.json',{'error':str(e),'traceback':traceback.format_exc()});raise
