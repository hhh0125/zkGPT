from pathlib import Path
import os,subprocess,json,time,struct
root=Path('task9_logs_20261014');out=root/'exp-attack-v4'
env={**os.environ,'ZKGPT_THREADS':'8','ZKGPT_PUBLIC_EMBEDDING':str(Path('task8_logs_20261013/embedding.txt').resolve()),'ZKGPT_MEMORY_DIAGNOSTICS':'1'}
while True:
    log=(root/'acceptance-driver.log').read_text()
    if 'Traceback' in log:raise RuntimeError('honest acceptance failed')
    if 'HONEST AND ORIGINAL REGRESSIONS DONE' in log:break
    time.sleep(10)
results=[]
def run(name,args,expected):
    print('START',name,flush=True);started=time.time()
    with (root/(name+'.log')).open('x') as f:
        p=subprocess.run(['/usr/bin/time','-v',*args],stdout=f,stderr=subprocess.STDOUT,env=env)
    item={'test':name,'args':args,'exit':p.returncode,'expected':expected,'driver_seconds':time.time()-started}
    results.append(item);(root/'attack-results.json').write_text(json.dumps(results,indent=2))
    print('END',name,p.returncode,round(item['driver_seconds'],2),flush=True)
    if expected=='nonzero':
        assert p.returncode!=0,item
    else: assert p.returncode==expected,item
run('malicious-local-check-oneblock',[str(out/'malicious_exp_prover'),'--blocks','1','--range-threads','8','--trusted-model',str(root/'oneblock.anchor.bin'),'--artifact-prefix',str(root/'malicious-local-check-oneblock')],'nonzero')
assert 'LogUp reciprocal sums disagree' in (root/'malicious-local-check-oneblock.log').read_text()
def identities(path):
    data=path.read_bytes();n=struct.unpack_from('<Q',data,0)[0];pos=8+n
    return {key:data[pos+32*i:pos+32*(i+1)].hex() for i,key in enumerate(['model_id','profile_id','input_id'])}
for depth,label in [(1,'oneblock'),(12,'full12')]:
    prefix=root/('malicious-'+label)
    run('malicious-prove-'+label,[str(out/'malicious_exp_prover_bypass'),'--blocks',str(depth),'--range-threads','8','--trusted-model',str(root/(label+'.anchor.bin')),'--artifact-prefix',str(prefix)],0)
    honest=identities(root/(label+'.strict-v4.bin'));malicious=identities(Path(str(prefix)+'.strict-v4.bin'))
    assert honest==malicious,(honest,malicious)
    hs=(root/(label+'.statement.bin')).read_bytes();ms=Path(str(prefix)+'.statement.bin').read_bytes()
    assert hs!=ms
    (root/('malicious-'+label+'-identity-comparison.json')).write_text(json.dumps({'honest':honest,'malicious':malicious,'same_declared_identities':True,'different_committed_witness_statement':True},indent=2))
    run('malicious-verify-'+label,['cmake-build-release/src/model_binding','verify',str(root/(label+'.anchor.bin')),str(prefix)+'.strict-v4.bin',env['ZKGPT_PUBLIC_EMBEDDING']],1)
    assert 'degree-1 sumcheck round consistency failed' in (root/('malicious-verify-'+label+'.log')).read_text()
print('FULL REAL SCALE ATTACK REJECTED BY INDEPENDENT VERIFIER',flush=True)
