from pathlib import Path
import os,sys,json,hashlib,subprocess,threading,time
r=Path.cwd();d=r/'reports/R6-FULL12-P0AB-20261021';l=r/'logs/R6-FULL12-P0AB-20261021';b=r/'build/release-r6-full12-p0ab-20261021';old=r/'reports/R6B-OPT2-20261021'
assert (d/'BUILD_READY.json').exists()
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
os.environ.update({'PATH':'/usr/bin:/bin','LANG':'C.UTF-8','LC_ALL':'C','TZ':'UTC','ZKGPT_THREADS':'4','OMP_NUM_THREADS':'4','OPENBLAS_NUM_THREADS':'4','MALLOC_ARENA_MAX':'2'})
helper=r/'reports/R6A/helper.py';assert sha(helper)=='14c6207100a29a00aa0bcc6795ec0a2ddb120eca0edc0e6f149713b936875351'
ns={};exec(compile(helper.read_bytes(),str(helper),'exec'),ns);ns['r']=d;ns['logs']=l
def run(name,args,**kwargs):ns['run'](name,args,**kwargs)
# All registered tests except schema test with historical hardcoded outputs.
run('ctest-current',['/usr/bin/ctest','--test-dir',str(b),'--output-on-failure','-j1','-E','^r6_actual_schema$'],timeout=1500,mem_gib=24,disk_gib=64)
# Build the identical schema test with evidence output paths relocated only.
src=r/'src/paper_r2/tests_r6_integration.cpp';text=src.read_text().replace('reports/R6B-OPT2-20261021','reports/R6-FULL12-P0AB-20261021')
(d/'tests_r6_integration_relocated.cpp').write_text(text)
link=json.loads((old/'parser-build-command.json').read_text())['argv']
cmd=[x.replace('build/release','build/release-r6-full12-p0ab-20261021').replace(str(old),str(d)) for x in link]
cmd=[x for x in cmd if x!='-DR1_VERIFY_ONLY']
cmd=[str(d/'tests_r6_integration_relocated.cpp') if x.endswith('r6b_statement_parser.cpp') else str(d/'r6_schema_tests_relocated') if x.endswith('/r6b_statement_parser') else x for x in cmd]
cmd=[x.replace('libpaper_r2_verifier.a','libpaper_r2.a').replace('libpaper_r1_verifier.a','libpaper_r1.a') for x in cmd]
run('schema-build',cmd,timeout=180,mem_gib=8,disk_gib=64)
run('schema-current',[str(d/'r6_schema_tests_relocated'),str(r/'reports/R6A/full12-operator-trust-v1.anchor'),'754efd1a61622e2d92a2057ccbb7ef7b026cb9bdfc58e67805ed68abf9bff306',str(r/'reports/R6A/full12-model-manifest-v1.txt')],timeout=300,mem_gib=16,disk_gib=64)
run('full12-actual-builder-plan',[str(b/'src/paper_r2/r6_model_plan'),str(d/'FULL12_PLAN.json'),'--blocks','12'],timeout=300,mem_gib=8,disk_gib=64)
plan=json.loads((d/'FULL12_PLAN.json').read_text());oldplan=json.loads((old/'FULL12_FINAL_OPT2_PLAN.json').read_text());assert plan==oldplan,'full12 structural/numerical plan changed'
(d/'FULL12_IDENTITY_COMPARISON.json').write_text(json.dumps({'all_plan_fields_equal':True,'blocks':plan['blocks'],'layers':plan['layers'],'terminals':plan['terminals'],'circuit':plan['circuit'],'profile_sha256':plan['profile_sha256'],'current_plan_sha256':sha(d/'FULL12_PLAN.json'),'frozen_plan_sha256':sha(old/'FULL12_FINAL_OPT2_PLAN.json'),'actual_full12_proof':False},indent=2))
opts=['--blocks','1','--trusted-anchor',str(r/'reports/R6A/oneblock-operator-trust-v1.anchor'),'--anchor-sha256',sha(r/'reports/R6A/oneblock-operator-trust-v1.anchor'),'--model-manifest',str(r/'reports/R6A/oneblock-model-manifest-v1.txt')]
# This is explicit R6 CLI compatibility evidence; never a full12 result.
run('r6-oneblock-smoke-prover',[str(b/'src/paper_r2/r6_model_prove'),str(d/'r6-oneblock-smoke')]+opts+['--memory-gib','24','--max-seconds','1800'],timeout=1860,mem_gib=24,disk_gib=64,env={'ZKGPT_MEMORY_DIAGNOSTICS':'1'})
empty=d/'smoke-independent-empty';empty.mkdir();assert not list(empty.iterdir())
run('r6-oneblock-smoke-independent',[str(b/'src/paper_r2/r6_model_verify'),str(d/'r6-oneblock-smoke/model.statement'),str(d/'r6-oneblock-smoke/model.proof')]+opts+['--memory-gib','16','--max-seconds','900'],timeout=960,mem_gib=16,disk_gib=64,cwd=str(empty))
assert 'ACCEPT R6 blocks=1 GKR+production Lasso+bridge+unified input,externally OPERATOR-TRUSTED W; candidate only' in (l/'r6-oneblock-smoke-independent.log').read_text().splitlines()
assert not list(empty.iterdir())
(d/'R6_COMPATIBILITY_PASS.json').write_text(json.dumps({'R6_smoke_blocks':1,'independent_accept':True,'full12_prover_success':False,'full12_verifier_accept':False,'proof':{'sha256':sha(d/'r6-oneblock-smoke/model.proof'),'bytes':(d/'r6-oneblock-smoke/model.proof').stat().st_size},'statement':{'sha256':sha(d/'r6-oneblock-smoke/model.statement'),'bytes':(d/'r6-oneblock-smoke/model.statement').stat().st_size}},indent=2))
print('R6_COMPATIBILITY_PASS; SMOKE ONLY; FULL12 NOT STARTED',flush=True)
