from pathlib import Path
BASE=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/r7-single-hyrax-perf-mapping-k-ipa-20261105-v1')
exec((BASE/'run_stage_v2.py').read_text().split('mode=sys.argv[1]')[0])
import statistics,shutil
assert json.loads((BASE/'green-final-SUCCESS.json').read_text())['success']
original_limits=limits
def limits(test):
 original_limits(test)
 if test:resource.setrlimit(resource.RLIMIT_CPU,(120,120))
before=identity();save('accept-IDENTITY_BEFORE.json',before)
BIN=BUILD/'src/paper_r7'
try:
 # The same test TU is linked to the preserved baseline library; original old
 # production objects were compiled before any algorithm edits. No source copy.
 command=(BIN/'CMakeFiles/r7_zero_suffix_storage_tests.dir/link.txt').read_text()
 import shlex
 args=shlex.split(command);args=[str(BIN/a) if a.startswith(('CMakeFiles/','../')) or a=='libpaper_r7.a' else a for a in args]
 at=args.index('-o');args[at+1]=str(BASE/'baseline-fixture')
 args=[str(BASE/'BASELINE-libpaper_r7.a') if a==str(BIN/'libpaper_r7.a') else a for a in args]
 run('baseline-link',args,60,False)
 proof0=BASE/'baseline.fsproof';proof1=BASE/'optimized.fsproof'
 run('baseline-prove',['/usr/bin/time','-v',BASE/'baseline-fixture','prove',proof0],60)
 run('optimized-prove',['/usr/bin/time','-v',BIN/'r7_zero_suffix_storage_tests','prove',proof1],60)
 assert proof0.read_bytes()==proof1.read_bytes(),'STOP_CORRECTNESS deterministic proof differs'
 run('optimized-verify-old',[BIN/'r7_zero_suffix_storage_tests','verify',proof0],60)
 run('optimized-verify-new',[BIN/'r7_zero_suffix_storage_tests','verify',proof1],60)
 run('baseline-verify-new',[BASE/'baseline-fixture','verify',proof1],60)
 save('INDEPENDENT_PROOF_COMPARISON.json',{'equal':True,'sha256':sha(proof1),'bytes':proof1.stat().st_size,'baseline_accepts_new':True,'new_accepts_baseline':True,'strict_EOF':True,'real_bounded_Single_protocol_fixture':True,'official_model_proof':False})
 for kind,data in [('truncated',proof1.read_bytes()[:-1]),('trailing',proof1.read_bytes()+b'\0'),('changed',proof1.read_bytes()[:len(proof1.read_bytes())//2]+bytes([proof1.read_bytes()[len(proof1.read_bytes())//2]^1])+proof1.read_bytes()[len(proof1.read_bytes())//2+1:])]:
  path=BASE/(kind+'.fsproof');path.write_bytes(data)
  run('negative-'+kind,[BIN/'r7_zero_suffix_storage_tests','verify',path],60,True,1)
 save('BOUNDED_NEGATIVE.json',{'truncated':'REJECT','trailing':'REJECT','changed':'REJECT','mechanisms':'See raw logs; program catches exception and returns 1, never crash/timeout'})
 # Process-isolated alternating A/B: 7 pairs plus one explicitly cold process
 # pair, three stored fractions. No hidden scalar-dependent sparse fixtures.
 records=[]
 for threads in [1,4]:
  n=1<<19
  for stored in [n//4,357093,490418,517547,n]:
   pairs=[]
   for sample in range(8):
    results={}
    for opt in ([0,1] if sample%2==0 else [1,0]):
     label=f'bench-t{threads}-s{stored}-i{sample}-o{opt}'
     run(label,[BIN/'r7_zero_suffix_tests','bench',n,stored,threads,opt,4],60)
     tagged=[json.loads(line[6:]) for line in (BASE/(label+'.log')).read_text().splitlines() if line.startswith('BENCH ')]
     assert len(tagged)==1;results[opt]=tagged[0]
    assert results[0]['digest']==results[1]['digest'],'STOP_CORRECTNESS benchmark coefficients differ'
    assert results[0]['covered']==results[1]['covered']
    pairs.append({'sample':sample,'cold_process_pair':sample==0,'reference':results[0],'optimized':results[1]})
   warm=pairs[1:];a=[p['reference']['seconds'] for p in warm];b=[p['optimized']['seconds'] for p in warm];paired=[p['reference']['seconds']/p['optimized']['seconds'] for p in warm]
   record={'threads':threads,'n':n,'stored':stored,'samples':len(warm),'reference_median':statistics.median(a),'optimized_median':statistics.median(b),'reference_range':[min(a),max(a)],'optimized_range':[min(b),max(b)],'paired_speedup_median':statistics.median(paired),'paired_speedup_range':[min(paired),max(paired)],'pairs':pairs}
   records.append(record);save('BENCHMARKS.json',records)
 # Only padded target fixtures determine acceptance; full-span control retains
 # the same visited data/arithmetic and is reported as overhead/no-elision case.
 assert all(r['paired_speedup_median']>1.03 for r in records if r['stored']<r['n']),'STOP_PERFORMANCE no confirmed padded-fixture gain'
 after=identity();save('accept-IDENTITY_AFTER.json',after);assert before==after
 save('accept-SUCCESS.json',{'success':True})
except BaseException as e:save('accept-FAILURE.json',{'error':str(e),'traceback':traceback.format_exc()});raise
