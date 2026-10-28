from pathlib import Path
import os,sys,json,hashlib,shutil,subprocess,difflib,ast,importlib.util,datetime
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(r);os.sched_setaffinity(0,[0,1,2,3])
prior=r/'reports/PERF-P0A-20261021';old=r/'reports/PERF-P0A-ONEBLOCK-20261021';oldlogs=r/'logs/PERF-P0A-ONEBLOCK-20261021'
d=r/'reports/PERF-P0A-ONEBLOCK-R2-20261021';logs=r/'logs/PERF-P0A-ONEBLOCK-R2-20261021';d.mkdir(exist_ok=False);logs.mkdir(exist_ok=False)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def dump(n,v):(d/n).write_text(json.dumps(v,indent=2,ensure_ascii=False)+'\n')
retention=json.loads((prior/'FAILED_ATTEMPT_RETENTION_VERIFIED.json').read_text());assert retention['verified'] and retention['entries_restore_read_reverified']==50
assert retention['archive_sha256']==sha(old/'ATTEMPT_EVIDENCE.tar.gz')=='1cfa2ff98cca40656523877d43f20ac42d2d19a6082b86b63651523eec74e7da'
manifest=json.loads((old/'ATTEMPT_MANIFEST.json').read_text());assert all(sha(r/name)==row['sha256'] for name,row in manifest.items())
dump('FAILED_ATTEMPT_RETENTION_VERIFIED.json',retention)
for name in ['oneblock_resource_policy.py','oneblock_supervisor.py','test_oneblock_policy.py']:
 shutil.copy2(old/name,d/name)
driver=prior/'run_oneblock_when_gate_passes.py';before=driver.read_text();(d/'driver.before.py').write_text(before)
s=(old/'FUTURE_DRIVER_FIXED_NOT_EXECUTED.py').read_text().replace("ACCEPT=ROOT/'reports/PERF-P0A-ONEBLOCK-20261021'","ACCEPT=ROOT/'reports/PERF-P0A-ONEBLOCK-R2-20261021'").replace('real-oneblock-next-attempt','real-oneblock-once').replace('logs/PERF-P0A-ONEBLOCK-20261021/','logs/PERF-P0A-ONEBLOCK-R2-20261021/')
assert 'folder.mkdir()' not in s;ast.parse(s)
assert "out=ACCEPT/'real-oneblock-once'" in s
# Exercise the actual workflow tail with filesystem-only replacement callbacks.
# No prover/verifier executable is called and no fixture is called a real proof.
def path_contract(text,name,pass_expected):
 root=d/('path-contract-'+name);root.mkdir()
 tail=text[text.index('# Production acceptance first;'):text.index("(out/'COMPLETED.json')")]
 calls=[]
 def fake_run(label,binary,args,prover=False):
  if prover:
   target=Path(args[0]);assert not target.exists(),'Prover target pre-created';target.mkdir();(target/'DRY_RUN_ONLY').write_text('not a proof')
  else:
   for arg in args:assert Path(arg).exists(),str(arg)
  calls.append(label)
 def fake_identical(folder):assert (folder/'DRY_RUN_ONLY').is_file()
 # Verifier argument paths must exist; only dry-run placeholder files are used.
 def with_paths(label,binary,args,prover=False):
  fake_run(label,binary,args,prover)
  if prover:
   for item in ['model.statement','model.proof']:(Path(args[0])/item).write_text('DRY RUN PLACEHOLDER ONLY')
 ns={'out':root,'ROOT':r,'D':prior,'old':r/'reports/R6B-OPT2-20261021/final-oneblock','run':with_paths,'identical':fake_identical}
 try:exec(compile(tail,'actual-driver-workflow-tail','exec'),ns)
 except AssertionError as e:
  assert not pass_expected;(logs/('01-path-'+name+'.log')).write_text('EXPECTED RED: '+str(e)+'\n');return
 assert pass_expected and len(calls)==8,calls
 (logs/('02-path-'+name+'.log')).write_text('PASS filesystem-only workflow contract,8 planned calls; NO actual binaries launched.\n'+json.dumps(calls)+'\n')
path_contract(before,'RED',False);path_contract(s,'GREEN',True)
env=os.environ.copy();env['PYTHONPATH']=str(d)+':'+str(r/'reports/R6B-OPT2-20261021');env['PYTHONDONTWRITEBYTECODE']='1'
p=subprocess.run([sys.executable,str(d/'test_oneblock_policy.py')],capture_output=True,text=True,env=env);(logs/'03-scoped-policy-tests.log').write_text(p.stdout+p.stderr);assert p.returncode==0
driver.write_text(s);(d/'driver.after.py').write_text(s)
(d/'OUTPUT_DIRECTORY_FIX.patch').write_text(''.join(difflib.unified_diff(before.splitlines(True),s.splitlines(True),fromfile='driver.before.py',tofile='driver.after.py')))
dump('SCRIPT_SHA.json',{'driver_before':hashlib.sha256(before.encode()).hexdigest(),'driver_after':sha(driver),'policy':sha(d/'oneblock_resource_policy.py'),'private_supervisor':sha(d/'oneblock_supervisor.py')})
for n in ['AFTER_SOURCE_SHA.json','FROZEN_PARAMETER_PROFILE_SHA.json','PROTECTED_APPROVAL_ARTIFACT_SHA.json']:
 values=json.loads((prior/n).read_text());assert all(sha(k)==v for k,v in values.items())
base=json.loads((old/'BEFORE_TEST_SCRIPT_SHA.json').read_text());assert all(sha(k)==v for k,v in base.items() if k!=str(driver))
dump('UNCHANGED_FULL12_GATE_SOURCE_PARAMETER_HISTORY.json',{'source_parameters_history_unchanged':True,'original_full12_resources_and_R6A_supervisor_unchanged':True})
launcher=(old/'launch_real_acceptance.py').read_text().replace('PERF-P0A-ONEBLOCK-20261021','PERF-P0A-ONEBLOCK-R2-20261021');(d/'launch_real_acceptance.py').write_text(launcher);ast.parse(launcher)
# Only the failed experiment's exact remote output/log directories and its
# preparation helper/cache are cleaned; the verified Windows archives remain.
helper=prior/'prepare_real_acceptance.py';cache=Path(importlib.util.cache_from_source(str(driver)))
targets=[old,oldlogs]+([helper] if helper.exists() else [])+([cache] if cache.exists() else [])
allowed={r/'reports/PERF-P0A-ONEBLOCK-20261021',r/'logs/PERF-P0A-ONEBLOCK-20261021',prior/'prepare_real_acceptance.py',cache}
for target in targets:
 assert target in allowed and not target.is_symlink();resolved=target.resolve();assert resolved.is_relative_to(r.resolve()) and resolved!=r.resolve()
 if target not in [old,oldlogs]:
  dest=d/'retired-experiment-helper-bytes'/target.relative_to(r);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(target,dest)
dump('CLEANUP_EXACT_PLAN.json',{'authorization':'User: fix error, delete files of the failed immediately-execute experiment, rerun once.','paths':[str(t) for t in targets],'Windows_archive_retained':retention,'original_zkGPT_and_history_not_targets':True})
for target in targets:
 if target.is_dir():shutil.rmtree(target)
 else:target.unlink()
assert all(not t.exists() for t in targets)
dump('CLEANUP_RESULT.json',{'deleted_exact_paths':[str(t) for t in targets],'verified_absent':True,'recovery_archive_retained':True,'production_source_and_binary_deleted':False,'utc':datetime.datetime.now(datetime.timezone.utc).isoformat()})
(d/'progress.md').write_text('R2 scope: user authorizes correcting output-directory contract, bounded failed-experiment cleanup and one renewed acceptance. Original foreign-job WARN authorization persists; affinity4/P24/V8/64disk/32mem/1500P1200V retained. Source/binaries/parameters/history/full12 gate unchanged. Filesystem RED/GREEN workflow and6 policy tests passed; no real binary yet. Windows archive+50 restore-read SHA reverified before exact remote cleanup. One renewed driver attempt, failure stop/no retry.\n')
gate=subprocess.run([sys.executable,'-B',str(driver),'--other-user-jobs-warn-once'],capture_output=True,text=True);(logs/'04-gate-only.log').write_text(gate.stdout+gate.stderr);print(gate.stdout+gate.stderr,flush=True);assert gate.returncode==0
print('R2 READY; FAILED REMOTE EXPERIMENT CLEANED; NO REAL RUN YET',flush=True)
