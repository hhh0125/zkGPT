from pathlib import Path
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd().resolve()==R
S=R/'reports/R7-ROWMAJOR-OPT2-20261028-v1';p=S/'run.py';s=p.read_text();needle='else:raise ValueError(mode)'
s=s.replace(needle,r'''elif mode=='directed':
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
'''+needle)
p.write_text(s)
print('DIRECTED AND SEVEN-PAIR RUN MODES READY')
