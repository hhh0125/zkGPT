"""Retained Stage3B1/3A/2/1/CSIPA/R7/R6 live and unit regressions.
New report children only; no historical evidence or source is modified.
"""
from pathlib import Path
import os,sys,json,subprocess,time,shutil
ROOT=Path.cwd();stage=Path(sys.argv[1]);assert stage.parent==ROOT/'reports' and stage.name.startswith(('R7-ONEBLOCK-RUN-','R7-CLASSIFIED-RUN-'))
events=[];BIN='build/release/src/paper_r7/';env=os.environ.copy()
for k in list(env):
 if k.startswith(('R7_','ZKGPT_')):env.pop(k)
env.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2',PYTHONDONTWRITEBYTECODE='1')
def run(name,args,timeout=300):
 print('REGRESSION',name,flush=True);t=time.monotonic()
 # Inherit the outer runner process group; its timeout/normal cleanup owns all descendants.
 with (stage/(name+'.log')).open('wb') as log:code=subprocess.run(['/usr/bin/time','-v',*args],stdout=log,stderr=subprocess.STDOUT,timeout=timeout,env=env).returncode
 events.append(dict(name=name,command=args,exit_code=code,seconds=time.monotonic()-t,passed=code==0));(stage/'REGRESSIONS.json').write_text(json.dumps(events,indent=2))
 assert code==0,name
targets=['r7_a3_tests','r7_a3_prover','r7_a3_verifier','r7_crypto_tests','r7_local_prover','r7_local_verifier','r7_framework_tests','r7_oneblock_prove','r7_oneblock_verify','r6_builder_tests','r6_model_prove','r6_model_verify','r6_model_plan','r5b_gkr_tests','r6_resources_tests','r7_s2_fixture','r7_s2_prover','r7_s2_verifier','r7_s2_tests','r7_m3_tests','r7_m3_fixture','r7_m3_prover','r7_m3_verifier','r7_b1_dense_tests','r7_b1_tests','r7_b1_prover','r7_b1_verifier']
targets += ['r7_oneblock_plan','r7_oneblock_plan_tests']
run('full-verifier-guards',['python3','-B','tools/oneblock_guard_tests.py',str(stage)],120)
run('retained-build',['cmake','--build','build/release','--target',*targets,'-j4'],600)
run('retained-units',['ctest','--test-dir','build/release','-V','--output-on-failure','-R','^(r7_hyrax_a3_unit|r7_hyrax_a3_stage2_unit|r7_stage3a_unit|r7_stage3b1_unit|r7_stage3b1_dense_reference|r7_crypto_arithmetic|r7_framework|r6_builder_oneblock|r6_entry_guards|r5b_FC_GKR_coupling|r6_system_resources)$'],600)
run('retained-a3-independent',['python3','-B','tools/hyrax_a3_tests.py',str(stage/'a3-sessions')],300)
run('retained-csipa',['python3','-B','tools/r7_interactive_tests.py',str(stage/'csipa-sessions'),'--real'],300)
suffix=stage.name.split('-RUN-',1)[1]+'-ONEBLOCK-REGRESSION'
for tag in ['R7-HYRAX-A3-STAGE2-RUN-','R7-STAGE3A-RUN-','R7-STAGE3B1-RUN-']:
 child=ROOT/'reports'/(tag+suffix);child.mkdir(exist_ok=False)
 if tag.startswith('R7-HYRAX'):
  env['R7_STAGE2_PRIVATE_PATH']=str(ROOT/'build/r7-stage2-private'/child.name/'fixture.private')
  run('retained-stage2-setup',['python3','-B','tools/hyrax_a3_stage2_setup.py',str(child)],240)
  run('retained-stage2-independent',['python3','-B','tools/hyrax_a3_stage2_tests.py',str(child/'stage2-sessions')],300)
 else:
  name='stage3a' if tag.startswith('R7-STAGE3A') else 'stage3b1'
  run('retained-'+name+'-setup',[BIN+'r7_m3_fixture',child.name],240)
  run('retained-'+name+'-independent',['python3','-B','tools/'+name+'_tests.py',str(child)]+(['--small-only'] if name=='stage3b1' else []),1800)
 shutil.copytree(child,stage/('retained-'+tag.rstrip('-')))
print('RETAINED_REGRESSIONS_PASSED',flush=True)
