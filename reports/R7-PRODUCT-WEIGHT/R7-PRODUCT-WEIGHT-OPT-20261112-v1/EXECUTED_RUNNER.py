"""Scoped R7 Product weight validation; authored locally and uploaded verbatim."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
import importlib.util
import resource

ROOT = Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')
STAGE = ROOT/'reports/R7-PRODUCT-WEIGHT-OPT-20261112-v1'
BUILD = ROOT/'build/product-weight-20261112-v1'
BIN = BUILD/'src/paper_r7'
os.chdir(ROOT)
STAGE.mkdir(exist_ok=True)
ENV = {k:v for k,v in os.environ.items() if not k.startswith(('ZKGPT_','R7_'))}
ENV.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2',PYTHONDONTWRITEBYTECODE='1')
ATTEMPT=sys.argv[2] if len(sys.argv)>2 else ''
assert not ATTEMPT or ATTEMPT.isdigit()

def save(name, value):
    (STAGE/name).write_text(json.dumps(value,indent=2)+'\n')

def run(name,args,env=None,expect=0):
    if ATTEMPT:name+='-attempt'+ATTEMPT
    path=STAGE/(name+'.log'); assert not path.exists(),path
    begin=time.monotonic()
    with path.open('xb') as log:
        p=subprocess.run([str(x) for x in args],stdout=log,stderr=subprocess.STDOUT,env=env or ENV)
    result={'command':[str(x) for x in args],'exit_code':p.returncode,'wall_seconds':time.monotonic()-begin,'log':str(path)}
    save(name+'.json',result)
    print(name,p.returncode,result['wall_seconds'],flush=True)
    assert p.returncode==expect,(name,p.returncode,path.read_text(errors='replace')[-3000:])
    return path

def build(tag):
    run(tag+'-configure',['cmake','-S',ROOT,'-B',BUILD,'-DCMAKE_BUILD_TYPE=Release','-DZKGPT_BUILD_R7_FRAMEWORK=ON',
                          '-DCNPY_INCLUDE_DIR='+str(ROOT/'deps/cnpy/include'),'-DCNPY_LIBRARY='+str(ROOT/'deps/cnpy/lib/libcnpy.a')])
    targets=['r7_product_weight_tests','r7_perf5_product_tests','r7_perf5_profile_tests','r7_perf6_product_inputs_tests','r7_perf6_exact_native','r7_oneblock_lookup_tests','r7_perf5_product_fs_probe','r7_fs_full12_prover','r7_fs_full12_verifier']
    run(tag+'-build',['cmake','--build',BUILD,'--target',*targets,'-j4'])

def baseline():
    build('baseline')
    dest=STAGE/'baseline-bin'; dest.mkdir()
    for name in ['r7_perf6_exact_native','r7_perf5_product_fs_probe','r7_fs_full12_prover','r7_fs_full12_verifier']:
        shutil.copy2(BIN/name,dest/name)
    path=run('expected-red',[BIN/'r7_product_weight_tests'],expect=1)
    assert 'Verifier materialized Product layer weights' in path.read_text()
    run('baseline-product-tests',['ctest','--test-dir',BUILD,'-R','^r7_perf(5_product_equivalence|5_profile_hierarchy|6_product_inputs)$','--output-on-failure'])
    save('BASELINE_READY.json',{'success':True,'expected_red':True,'release_build_preserved':True})

def objects(path,tag):
    return [json.loads(line[len(tag)+1:]) for line in path.read_text().splitlines() if line.startswith(tag+' ')]

def candidate():
    assert (STAGE/'BASELINE_READY.json').exists()
    build('candidate')
    run('candidate-weight-tests',[BIN/'r7_product_weight_tests'])
    run('candidate-product-tests',['ctest','--test-dir',BUILD,'-R','^r7_perf(5_product_equivalence|5_profile_hierarchy|6_product_inputs)$','--output-on-failure'])
    run('candidate-lookup-tests',[BIN/'r7_oneblock_lookup_tests'])
    seed=STAGE/'synthetic-seed.bin';seed.write_bytes(bytes(range(32)))
    comparisons=[]
    for c2 in ['0','1']:
        env=ENV|{'ZKGPT_PERF5_PROFILE':'1','ZKGPT_PERF6_C2':c2}
        paths={}
        for label,binary in [('baseline',STAGE/'baseline-bin/r7_perf6_exact_native'),('candidate',BIN/'r7_perf6_exact_native')]:
            proof=STAGE/(label+'-c2-'+c2+'.fsproof');trace=STAGE/(label+'-c2-'+c2+'-P.trace')
            run(label+'-c2-'+c2+'-prove',[binary,'prove',proof,trace,'512',seed],env)
            vtrace=STAGE/(label+'-c2-'+c2+'-V.trace')
            vlog=run(label+'-c2-'+c2+'-verify',[binary,'verify',proof,vtrace,'512'],env)
            if label=='candidate':
                products=objects(vlog,'PERF5_PRODUCT');assert len(products)==3
                assert all(all(x['weights'][key]==0 and x['leaf_evaluate_weights'][key]==0 for key in ['calls','elements','field_multiplications']) for x in products)
                assert all(x['regions']['equality_authentication']['calls']>0 for x in products)
            paths[label]=(proof,trace,vtrace)
        for a,b in zip(paths['baseline'],paths['candidate']):assert a.read_bytes()==b.read_bytes(),(a,b)
        comparisons.append({'c2':c2,'proof_bytes_equal':True,'all_prover_events_equal':True,'all_verifier_events_equal':True,'independent_verifier':True})
    save('CANDIDATE_READY.json',{'success':True,'comparisons':comparisons,'product_verifier_weights_zero':True,'leaf_verifier_weights_zero':True})

def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda:f.read(4<<20),b''):h.update(block)
    return h.hexdigest()

def measured(name,args,env,affinity):
    path=STAGE/(name+'.log');assert not path.exists()
    def limits():
        os.sched_setaffinity(0,affinity);resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    before=time.monotonic()
    with path.open('xb') as log:
        p=subprocess.Popen([str(x) for x in args],stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,env=env,preexec_fn=limits)
        save('ACTIVE-MEASUREMENT.json',{'name':name,'pid':p.pid,'started_unix':time.time()})
        _,status,usage=os.wait4(p.pid,0);p.returncode=os.waitstatus_to_exitcode(status)
    result={'command':[str(x) for x in args],'exit_code':p.returncode,'wall_seconds':time.monotonic()-before,'cpu_seconds':usage.ru_utime+usage.ru_stime,'peak_rss_kib':usage.ru_maxrss,'log':str(path),'load_after':os.getloadavg()}
    save(name+'.json',result);print(name,p.returncode,result['wall_seconds'],flush=True)
    assert p.returncode==0,(name,p.returncode)
    return result

def experiments():
    assert json.loads((STAGE/'CANDIDATE_READY.json').read_text())['success']
    assert not (STAGE/'EXPERIMENTS_STARTED.json').exists(),'refuse rerun'
    save('EXPERIMENTS_STARTED.json',{'started_unix':time.time(),'micro_order':['A','B','B','A','A','B'],'full12_order':['B','A'],'production_gate':'BLOCKED_CRYPTOGRAPHIC_DESIGN','may_enter_stage2':False})
    affinity=sorted(os.sched_getaffinity(0))[:4]
    assert len(affinity)==4
    env=ENV|{'ZKGPT_PERF5_PROFILE':'1','ZKGPT_PERF4_PROFILE':'1'}
    results=[]
    # Public-fingerprint surrogate isolates recursive Product. This does not
    # measure the three private Evaluate scans; Full12 below does.
    for n in [1<<22,1<<23]:
        for index,label in enumerate(['A','B','B','A','A','B']):
            binary=(STAGE/'baseline-bin' if label=='A' else BIN)/'r7_perf5_product_fs_probe'
            name=f'product-{n}-{index}-{label}';proof=STAGE/(name+'.fsproof')
            for role,mode in [('P','prove'),('V','verify')]:
                result=measured(name+'-'+role,[binary,mode,proof,n],env,affinity)
                product=objects(Path(result['log']),'PERF5_PRODUCT');assert len(product)==1
                if label=='B' and role=='V':assert all(product[0]['weights'][key]==0 for key in ['calls','elements','field_multiplications'])
                result.update(n=n,label=label,role=role,product=product[0],public_fingerprint_surrogate=True)
                results.append(result)
            save('PRODUCT-AB-RAW.json',results)
    # Reuse R9's exact statement, parameter identity, environment, 4-core
    # affinity, resource sampling and independent-verifier acceptance checks.
    source=ROOT/'reports/R9-FULL12-PHASE-TIMING-20261111-v1/run_experiments.py'
    spec=importlib.util.spec_from_file_location('retained_r9_method',source)
    r9=importlib.util.module_from_spec(spec);spec.loader.exec_module(r9)
    r9.S=STAGE;r9.LOG=ROOT/'logs'/STAGE.name;r9.LOG.mkdir(exist_ok=True)
    r9.ENV['ZKGPT_PERF5_PROFILE']='1'
    r9.AFFINITY=affinity
    frozen=r9.sources();save('FROZEN_SOURCE.json',frozen)
    parameters=json.loads((ROOT/'reports/R9-FULL12-PHASE-TIMING-20261111-v1/PARAMETERS.json').read_text())
    assert {p:sha(ROOT/p) for p in parameters}==parameters
    save('PARAMETERS.json',parameters)
    save('METHOD.json',{'R9_method_script_sha256':sha(source),'micro_cpu_affinity':affinity,'full12_environment':r9.ENV,'baseline_binary_source':'BASE_SHA snapshot, before weight implementation; original source bytes preserved in overwrite backup','candidate_binary_source':'FROZEN_SOURCE.json','baseline_leaf_weights_wall':'not instrumented in original binary; analytical counts are reconstructed separately','full12_repetitions_per_variant':1,'full12_causal_precision':'one B/A pair; no precise causal speedup claim from this pair alone'})
    for label,name in [('B','full12-B-candidate'),('A','full12-A-baseline')]:
        r9.BIN=BIN if label=='B' else STAGE/'baseline-bin'
        r9.execute('full12',name,'R9',frozen)
        if label=='B':
            verifier=objects(r9.LOG/(name+'-V.log'),'PERF5_PRODUCT');assert len(verifier)==312
            assert all(all(p['weights'][key]==0 and p['leaf_evaluate_weights'][key]==0 for key in ['calls','elements','field_multiplications']) for p in verifier)
            prover=objects(r9.LOG/(name+'-P.log'),'PERF5_PRODUCT');assert len(prover)==312
            assert sum(p['leaf_evaluate_weights']['calls'] for p in prover)==312
            save('FULL12-CANDIDATE-WEIGHTS.json',{'P':prover,'V':verifier})
        assert {p:sha(ROOT/p) for p in parameters}==parameters
    save('EXPERIMENTS_RESULT.json',{'success':True,'parameters_unchanged':True,'source_unchanged':r9.sources()==frozen,'full12_independent_verifiers':2,'product_micro_processes':len(results),'production_gate':'BLOCKED_CRYPTOGRAPHIC_DESIGN','may_enter_stage2':False})

if __name__=='__main__':
    try:
        {'baseline':baseline,'candidate':candidate,'experiments':experiments}[sys.argv[1]]()
    except BaseException as error:
        save('FAILURE-'+sys.argv[1]+'.json',{'error':repr(error)}); raise
