from pathlib import Path
import os,sys,subprocess,time,json,resource,statistics,shutil,hashlib,struct
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd().resolve()==R
S=R/'reports/R7-ROWMAJOR-OPT2-20261028-v1';B=R/'build/release/src/paper_r7'
os.environ.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2',PYTHONDONTWRITEBYTECODE='1',ZKGPT_OPT2_METRICS='1')
os.sched_setaffinity(0,[0,1,2,3]);mode=sys.argv[1];cases=[]
def host():
 cg=Path('/proc/self/cgroup').read_text().strip().split(':')[-1];stats={}
 p=Path('/sys/fs/cgroup'+cg)
 while str(p).startswith('/sys/fs/cgroup'):
  for n in ['cpu.stat','cpu.max','cpu.pressure']:
   if (p/n).exists():stats[str(p/n)]=(p/n).read_text()
  if p==Path('/sys/fs/cgroup'):break
  p=p.parent
 return dict(load=Path('/proc/loadavg').read_text(),cgroup=stats,stat=Path('/proc/stat').read_text().splitlines()[0])
def run(name,args,expected=0,timeout=1200,statement=None):
 log=S/(name+'.log');assert not log.exists(),log
 handles=[];fds=[];env=os.environ.copy()
 if statement:
  f=Path(statement).open('rb');handles.append(f);fds.append(f.fileno());env['R7_ONEBLOCK_STATEMENT_FD']=str(f.fileno())
  for v,suffix in [('R7_ONEBLOCK_COVERAGE_FD','COVERAGE'),('R7_CLASSIFIED_LAYOUT_FD','LAYOUT')]:
   f=(S/(name+'-'+suffix+'.json')).open('xb');handles.append(f);fds.append(f.fileno());env[v]=str(f.fileno())
 before=host();started=time.monotonic()
 try:
  with log.open('xb') as out:
   p=subprocess.run(['/usr/bin/time','-v',*map(str,args)],stdout=out,stderr=subprocess.STDOUT,stdin=subprocess.DEVNULL,env=env,pass_fds=fds,timeout=timeout)
 finally:
  for f in handles:f.close()
 result=dict(name=name,command=list(map(str,args)),exit_code=p.returncode,expected=expected,wall_seconds=time.monotonic()-started,affinity=[0,1,2,3],host_before=before,host_after=host())
 records=[]
 for line in log.read_text(errors='replace').splitlines():
  if line.startswith('{') or line.startswith('OPT2_'):
   try:records.append(json.loads(line[line.index('{'):]))
   except (ValueError,json.JSONDecodeError):pass
 result['records']=records
 (S/(name+'.json')).write_text(json.dumps(result,indent=2));cases.append(result);(S/(mode+'-CASES.json')).write_text(json.dumps(cases,indent=2));print(name,p.returncode,result['wall_seconds'],flush=True)
 assert p.returncode==expected,log.read_text(errors='replace')[-4000:]
 return result
if mode=='baseline':
 run('baseline-build',['cmake','--build','build/release','--target','r7_opt2_kernel_tests','r7_opt2_msm_bench','r7_classified_crypto_tests','r7_fs_probe_prover','r7_fs_probe_verifier','-j4'])
 for n in ['r7_opt2_kernel_tests','r7_fs_probe_prover','r7_fs_probe_verifier']:
  shutil.copy2(B/n,S/'baseline-binaries'/('instrumented-'+n))
 run('baseline-dense',[B/'r7_opt2_kernel_tests'])
 run('baseline-map-negatives',[B/'r7_classified_crypto_tests'])
 run('baseline-kernel',[B/'r7_opt2_kernel_tests','bench'])
 for n in [64,1024]:
  proof=S/('baseline-'+str(n)+'.fsproof')
  for role in ['prover','verifier']:run('baseline-fs-'+str(n)+'-'+role,[B/('r7_fs_probe_'+role),n,'grouped',proof,'zkGPT/R7/FS/test-request-20261028'])
elif mode=='msm':
 for repeat in range(7):
  for implementation in ([0,1,2] if repeat%2==0 else [2,1,0]):run('msm-'+str(repeat)+'-'+str(implementation),[B/'r7_opt2_msm_bench',implementation])
 values={str(i):[next(x for x in r['records'] if 'iterations' in x) for r in cases if r['name'].endswith('-'+str(i))] for i in range(3)}
 (S/'MSM_BENCH.json').write_text(json.dumps(dict(samples=values,medians={i:{k:statistics.median(x[k] for x in samples) for k in ['cpu_seconds','wall_seconds','setup_cpu_seconds']} for i,samples in values.items()}),indent=2))
elif mode=='directed':
 run('optimized-dense',[B/'r7_opt2_kernel_tests'])
 run('msm-full-width-equivalence',[B/'r7_opt2_msm_bench',0])
 run('sumcheck-coefficient-equivalence',[B/'r7_opt2_sumcheck_tests'])
 run('optimized-source-map-negatives',[B/'r7_classified_crypto_tests'])
 context='zkGPT/R7/FS/test-request-20261028'
 for n in [16,64,256,1024]:
  for group in ['joint','grouped']:
   name=f'optimized-fs-{n}-{group}';proof=S/(name+'.fsproof')
   p=run(name+'-P',[B/'r7_fs_probe_prover',n,group,proof,context]);v=run(name+'-V',[B/'r7_fs_probe_verifier',n,group,proof,context])
   pf=next(x for x in p['records'] if 'challenge_digest' in x);vf=next(x for x in v['records'] if 'challenge_digest' in x)
   assert all(pf[k]==vf[k] for k in ['session','proof_sha256','proof_bytes','transcript_digest','challenge_digest','challenges','events'])
 for n in [64,1024]:run('new-V-baseline-proof-'+str(n),[B/'r7_fs_probe_verifier',n,'grouped',S/('baseline-'+str(n)+'.fsproof'),context])
 run('baseline-V-new-proof',[S/'baseline-binaries/instrumented-r7_fs_probe_verifier',64,'grouped',S/'optimized-fs-64-grouped.fsproof',context])
 for attack in ['challenge','map','value','late-change','missing-barrier']:
  proof=S/('malicious-'+attack+'.fsproof');run('malicious-'+attack+'-P',[B/'r7_fs_probe_prover',64,'grouped',proof,context,attack],0 if attack in ['challenge','map','late-change'] else 2)
  if not proof.exists():proof=next(S.glob(proof.name+'.partial.*'))
  run('malicious-'+attack+'-V',[B/'r7_fs_probe_verifier',64,'grouped',proof,context],2)
 proof=S/'optimized-fs-64-grouped.fsproof';data=bytearray(proof.read_bytes());data[-7]^=1;bad=S/'tampered-small.fsproof';bad.write_bytes(data);run('tampered-small-V',[B/'r7_fs_probe_verifier',64,'grouped',bad,context],2)
elif mode=='paired':
 for repeat in range(7):
  for implementation in (['reference','optimized'] if repeat%2==0 else ['optimized','reference']):
   run('kernel-pair-'+str(repeat)+'-'+implementation,[B/'r7_opt2_kernel_tests',implementation])
   run('sumcheck-pair-'+str(repeat)+'-'+implementation,[B/'r7_opt2_sumcheck_tests',implementation])
  for implementation in ([0,3] if repeat%2==0 else [3,0]):run('stack-pair-'+str(repeat)+'-'+str(implementation),[B/'r7_opt2_msm_bench',implementation])
 summary={}
 for kind in ['kernel','sumcheck','stack']:
  implementations=['0','3'] if kind=='stack' else ['reference','optimized'];summary[kind]={}
  for implementation in implementations:
   samples=[next(x for x in r['records'] if 'mode' in x) for r in cases if r['name'].startswith(kind+'-pair-') and r['name'].endswith('-'+implementation)]
   summary[kind][implementation]=dict(samples=samples,medians={k:statistics.median(x[k] for x in samples) for k in ['cpu_seconds','wall_seconds']})
  if kind=='kernel':assert len({x['checksum'] for i in implementations for x in summary[kind][i]['samples']})==1
 (S/'PAIRED_PERFORMANCE.json').write_text(json.dumps(summary,indent=2))
elif mode=='final-checks':
 targets=['r7_opt2_kernel_tests','r7_opt2_sumcheck_tests','r7_classified_crypto_tests','r7_classified_layout_tests','r7_a3_tests','r7_fs_io_tests','r7_oneblock_vectors_tests','r7_oneblock_model_tests','r7_oneblock_lookup_tests','r7_oneblock_gkr_tests','r7_fs_probe_prover','r7_fs_probe_verifier','r7_fs_oneblock_prover','r7_fs_oneblock_verifier','r7_complete_prover','r7_complete_verifier']
 run('final-build',['cmake','--build','build/release','--target',*targets,'-j4'],timeout=1800)
 for target in targets[:10]:run('final-unit-'+target,[B/target])
 for repeat in range(7):
  for implementation in (['reference','optimized'] if repeat%2==0 else ['optimized','reference']):run('polynomial-pair-'+str(repeat)+'-'+implementation,[B/'r7_opt2_sumcheck_tests',implementation])
 for role in ['prover','verifier']:run('final-small-'+role,[B/('r7_fs_probe_'+role),64,'grouped',S/'final-small.fsproof','zkGPT/R7/FS/test-request-20261028'])
elif mode=='allocations':
 run('allocation-bench-build',['cmake','--build','build/release','--target','r7_opt2_msm_bench','-j4'])
 for implementation in [0,3]:run('scalarCommit-allocations-'+str(implementation),[B/'r7_opt2_msm_bench',implementation])
elif mode=='full':
 for gate in ['directed','paired','final-checks','allocations']:assert json.loads((S/(gate+'-DONE.json')).read_text())['success'],gate
 # Copy the exact already accepted RowMajor public X/Y statement, never rerun
 # numerical witness generation to create a different public claim.
 statement=S/'PUBLIC_STATEMENT.bin';shutil.copy2(R/'reports/R7-ROWMAJOR-RUN-20261028-v1/PUBLIC_STATEMENT.bin',statement)
 proof=S/'full-opt2.fsproof';assert not proof.exists()
 context='zkGPT/R7/FS/test-request-20261028'
 p=run('full-opt2-P',[B/'r7_fs_oneblock_prover','prove',proof,context],timeout=7200,statement=statement)
 v=run('full-opt2-V',[B/'r7_fs_oneblock_verifier','verify',proof,context],timeout=7200,statement=statement)
 pf=next(x for x in p['records'] if 'challenge_digest' in x);vf=next(x for x in v['records'] if 'challenge_digest' in x)
 for k in ['session','proof_sha256','proof_bytes','transcript_digest','challenge_digest','challenges','prover_messages','events','field_rejections']:assert pf[k]==vf[k],k
 metric=next(x for x in v['records'] if x.get('required_count')==5351);assert metric['root_count']==6 and metric['claims']==936 and metric['registered_rows']==8193 and metric['GKR_rounds']==627
 coverage=json.loads((S/'full-opt2-V-COVERAGE.json').read_text());baseline=json.loads((R/'reports/R7-ROWMAJOR-RUN-20261028-v1/full-row-V-COVERAGE.json').read_text())
 assert coverage['required_count']==coverage['verified_count']==5351 and all(x['status']=='VERIFIED' for x in coverage['required'])
 fields=['id','family','domain','sumcheck_rounds'];assert {tuple(x[k] for k in fields) for x in coverage['required']}=={tuple(x[k] for k in fields) for x in baseline['required']}
 text=(S/'full-opt2-V.log').read_text();assert text.count('"phase":"opening"')==6
 assert pf['proof_bytes']==proof.stat().st_size==675472397
 (S/'FULL_SUCCESS.json').write_text(json.dumps(dict(security_status='FS_NIZK_CANDIDATE',production_gate='BLOCKED_CRYPTOGRAPHIC_DESIGN',may_enter_stage2=False,relations=5351,roots=6,claims=936,registered_rows=8193,GKR_rounds=627,openings=6,proof_bytes=proof.stat().st_size,proof_sha256=pf['proof_sha256'],session=pf['session']),indent=2))
 # Independent verifier-only public input negatives; no additional prover.
 run('full-wrong-context',[B/'r7_fs_oneblock_verifier','verify',proof,context+'-wrong'],expected=2,statement=statement)
 data=statement.read_bytes();at=4+struct.unpack_from('<I',data,0)[0]+8;fields=[]
 for _ in range(7):
  n=struct.unpack_from('<I',data,at)[0];fields.append((at+4,n));at+=4+n
 recipes=[]
 for name,offset in [('X',at+16+4),('model',fields[4][0]),('layout',fields[3][0])]:
  bad=bytearray(data);bad[offset]^=1;path=S/('WRONG_'+name+'_STATEMENT.bin');path.write_bytes(bad);recipes.append(dict(name=name,offset=offset,original=data[offset],replacement=bad[offset]))
  run('full-wrong-'+name,[B/'r7_fs_oneblock_verifier','verify',proof,context],expected=2,statement=path)
 # Corrupt the first public-binding payload in a separate retained work copy.
 tampered=S/'full-tampered.fsproof';shutil.copy2(proof,tampered)
 with tampered.open('r+b') as f:
  header=f.read(12);f.seek(struct.unpack('<I',header[8:])[0],1);frame=f.read(17);_,_,length=struct.unpack('<cQQ',frame);f.seek(length,1);size=struct.unpack('<Q',f.read(8))[0];offset=f.tell();assert size>0;old=f.read(1);f.seek(offset);f.write(bytes([old[0]^1]))
 recipes.append(dict(name='proof',offset=offset,original=old[0],replacement=old[0]^1))
 run('full-tampered-proof',[B/'r7_fs_oneblock_verifier','verify',tampered,context],expected=2,statement=statement)
 (S/'NEGATIVE_RECIPES.json').write_text(json.dumps(recipes,indent=2))
else:raise ValueError(mode)
(S/(mode+'-DONE.json')).write_text(json.dumps(dict(success=True,cases=len(cases))))
