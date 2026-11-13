"""Exercise real short-lived OS children; never launch a cryptographic target."""
from pathlib import Path
import ast,json,os,resource,signal,subprocess,sys,time
S=Path.cwd()/'reports/r7-perf4-full12-20261105';source=(S/'full12_runner_v3.py').read_text()
tree=ast.parse(source);definitions=[n for n in tree.body if isinstance(n,(ast.FunctionDef,ast.AsyncFunctionDef))]
base=S/'control-selftests';base.mkdir(exist_ok=False)
real_popen=subprocess.Popen
results=[]
for case,code in [('rss','data=bytearray(48<<20);import time;time.sleep(5)'),('fast-peak','data=bytearray(48<<20)'),('wall','import time;time.sleep(5)'),('monitor-error','import time;time.sleep(5)')]:
 stage=base/case;stage.mkdir();statement=stage/'statement.bin';statement.write_bytes(b'public-control-test')
 ns={'Path':Path,'json':json,'os':os,'resource':resource,'signal':signal,'subprocess':subprocess,'time':time,'hashlib':__import__('hashlib'),'ROOT':Path.cwd(),'BASE':stage,'STAGE':stage,'BIN':Path('/CONTROL_TEST_NOT_A_CRYPTO_TARGET'),'variant':'CONTROL_TEST','CONTEXT':'CONTROL_TEST','ENV':dict(os.environ),'CAP':{'cores':4,'address_space_bytes':128<<20,'P_wall_seconds':1 if case=='wall' else 10,'V_wall_seconds':10,'total_wall_seconds_max':30,'rss_bytes':32<<20,'new_swap_growth_bytes_max':1<<20},'usage':{'active':{}},'prior':0,'total_started':time.monotonic(),'cleanup_confirmed':True}
 exec(compile(ast.Module(body=definitions,type_ignores=[]),str(S/'full12_runner_v3.py'),'exec'),ns)
 subprocess.Popen=lambda args,**kw:real_popen([sys.executable,'-c',code],**kw)
 if case=='monitor-error':
  def broken_stat(pid):raise RuntimeError('intentional monitor failure')
  ns['stat']=broken_stat
 rejected=False
 try:ns['run']('P',stage/'NOT_A_PROOF',statement,ns['total_started'],0)
 except RuntimeError as e:
  rejected=True;assert 'STOP_RESOURCE' in str(e) or 'intentional monitor failure' in str(e)
 finally:subprocess.Popen=real_popen
 assert rejected and ns['cleanup_confirmed'],case
 active=json.loads((stage/'ACTIVE_FULL12.json').read_text());assert not Path('/proc',str(active['pid'])).exists(),('orphan',case)
 results.append({'case':case,'rejected':True,'child_reaped':True,'phase':json.loads((stage/'P.json').read_text())})
(base/'RESULT.json').write_text(json.dumps({'passed':True,'cases':results,'cryptographic_runs':0},indent=2));print('CONTROL_SELFTESTS_PASS RSS,short-peak,wall,monitor-exception; all children reaped')
