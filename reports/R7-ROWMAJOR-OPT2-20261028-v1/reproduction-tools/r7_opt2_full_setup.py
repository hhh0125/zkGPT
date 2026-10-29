from pathlib import Path
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd().resolve()==R
P=R/'src/paper_r7';S=R/'reports/R7-ROWMAJOR-OPT2-20261028-v1'
p=P/'opt2_msm_bench.cpp';s=p.read_text();old='for(unsigned i=0;i<8192;++i)checksum+=evaluate(mode,a[i%256],b[i%256]);';new='for(unsigned i=0;i<8192;++i){MSMTrace trace(2);checksum+=evaluate(mode,a[i%256],b[i%256]);}';assert old in s;s=s.replace(old,new);p.write_text(s)
p=S/'run.py';s=p.read_text().replace('import os,sys,subprocess,time,json,resource,statistics,shutil,hashlib','import os,sys,subprocess,time,json,resource,statistics,shutil,hashlib,struct')
needle='else:raise ValueError(mode)';s=s.replace(needle,r'''elif mode=='allocations':
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
'''+needle);p.write_text(s)
print('ONE FULL INDEPENDENT P/V AND FIVE VERIFIER-ONLY NEGATIVES READY')
