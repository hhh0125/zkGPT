from pathlib import Path
import os,sys,json,hashlib
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(r);os.sched_setaffinity(0,[0,1,2,3]);d=r/'reports/PERF-P0B-20261021';logs=r/'logs/PERF-P0B-20261021';c=d/'small-integration-compatibility'
assert '--fixture' in Path('src/paper_r2/verify_main.cpp').read_text()
sys.path.insert(0,str(r/'reports/R6B-OPT2-20261021'));import r6b_resources
sys.path.insert(0,str(d));from oneblock_resource_policy import check
ns={};exec(compile((d/'oneblock_supervisor.py').read_text(),'small-test supervisor','exec'),ns);ns['r']=d;ns['logs']=logs
results=[]
for name in ['valid.statement','valid.proof']:assert (c/'before'/name).read_bytes()==(c/'after'/name).read_bytes()
for verifier in ['before','after']:
 binary=d/'frozen/build/release/src/paper_r2/r2_verify' if verifier=='before' else r/'build/release/src/paper_r2/r2_verify'
 for source in ['before','after']:
  statements=sorted((c/source).glob('*.statement'),key=lambda p:(p.stem!='valid',p.stem))
  for statement in statements:
   name='21-fixture-'+verifier+'-on-'+source+'-'+statement.stem;expected=0 if statement.stem=='valid' else 2
   row=r6b_resources.snapshot(r);row['WARN_other_user_large_jobs']=check(row,8);(d/(name+'-resource.json')).write_text(json.dumps(row,indent=2))
   ns['run'](name,[str(binary),'--fixture',str(statement),str(statement.with_suffix('.proof'))],expected=expected,timeout=300,mem_gib=8,disk_gib=64,reserve_gib=32)
   text=(logs/(name+'.log')).read_text();assert 'usage:' not in text
   assert ('ACCEPT' in text) if expected==0 else ('REJECT' in text)
   results.append({'name':name,'expected_exit':expected,'actual_exit':expected,'PASS':True})
ident={str(p.relative_to(c)):{'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'bytes':p.stat().st_size} for p in c.rglob('*') if p.is_file()}
(d/'SMALL_COMPATIBILITY_RESULTS.json').write_text(json.dumps({'scope':'small fourFCONN/GKR+Range+Exp+bridge+checked-input fixture,NOT realR5Boneblock','valid_bytes_identical':True,'results':results,'artifacts':ident,'discarded_usage_error_runs':'20-small-* retained but NOT acceptance or negativeproof evidence'},indent=2))
with (d/'progress.md').open('a') as f:f.write('Smallcross-verifier harness correction: prior20-small-* missed required --fixture and never reachedcrypto; preserved invalidlogs,excluded fromevidence. Corrected21-fixture-* runvalidfirst; all expectedACCEPT/REJECT checks pass,proofbytesidentical. No arithmetic/FS/protocol incompatibility detected, no actualoneblock executed.\n')
print('CORRECTED_SMALL_CROSS_COMPATIBILITY_PASS',len(results),flush=True)
