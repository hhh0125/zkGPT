from pathlib import Path
import sys,subprocess,tempfile,json,os,signal,resource
prove,verify,plan=sys.argv[1:];root=Path(tempfile.mkdtemp(prefix='r6a-entry-',dir='build')).resolve();env=os.environ.copy();env.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2')
def check(args,code,reason,inherited_gib=None):
 p=subprocess.Popen(args,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=env,start_new_session=True,preexec_fn=(lambda:resource.setrlimit(resource.RLIMIT_AS,(inherited_gib<<30,inherited_gib<<30))) if inherited_gib else None)
 try:out=p.communicate(timeout=90)[0].decode()
 except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait();raise
 assert p.returncode==code and reason in out,(args,p.returncode,out[-1000:])
 print('PASS',args[0].split('/')[-1],args[1:],reason)
target=root/'must-not-exist'
check([prove,str(target),'--blocks','12'],1,'separately authorized')
check([verify,'reports/R5B/normal-v2/model.statement','reports/R5B/normal-v2/model.proof','--blocks','12'],2,'separately authorized')
for args,reason in [(['--blocks','2'],'exactly 1 or 12'),(['--blocks','1','--blocks','12'],'duplicate'),(['--blocks','1','--cpu-limit','1844674407370955161600000000001'],'bound'),(['--blocks','1','--memory-gib','-1'],'unsigned'),(['--blocks','1','--memory-gib','0'],'positive'),(['--blocks','1','--numerical-profile','modified-profile'],'frozen'),(['--blocks','1','--unknown-option'],'unknown'),(['--blocks'],'usage')]:check([plan,str(root/'invalid.json')]+args,1,reason)
output=root/'oneblock.json';check([prove,str(output),'--blocks','1','--plan-only'],0,'no private witness/proof');d=json.loads(output.read_text());assert d['metadata_field_witness_elements']==0 and d['master_actual']==16642424 and len(d['terminal_plans'])==51
check([plan,str(root/'inherited-cap.json'),'--blocks','1','--memory-gib','8'],1,'inherited',inherited_gib=2)
assert not (root/'inherited-cap.json').exists()
check([plan,str(root/'too-small.json'),'--blocks','12','--memory-gib','1'],1,'bad_alloc')
assert not target.exists() and not list(root.rglob('*.proof')) and not list(root.rglob('*.statement'));print('PASS 13 R6 entry cases; no proof/statement or full12 private witness')
