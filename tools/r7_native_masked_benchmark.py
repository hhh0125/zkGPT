from pathlib import Path
import os,subprocess,json,time,statistics,traceback
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(ROOT)
S=ROOT/'reports/r7-native-masked-20261118';L=ROOT/'logs/r7-native-masked-20261118';B=ROOT/'build/release/src/r7_native_masked_tests'
ENV={k:v for k,v in os.environ.items() if not k.startswith(('ZKGPT_','R7_'))}
AFFINITY=[min(os.sched_getaffinity(0))]
def limits():os.sched_setaffinity(0,AFFINITY)
runs=[]
def objects(text,prefix):return [json.loads(x[len(prefix):]) for x in text.splitlines() if (x.startswith(prefix) if prefix else x.startswith('{'))]
def run(kind,n,mode,rep,role,path):
 name=f'bench2-{kind}-{n}-{mode}-{rep}-{role}';args=[str(B),role,kind,mode,str(n),str(path),'none']
 start=time.monotonic()
 with (L/(name+'.log')).open('x') as f:p=subprocess.run(args,stdout=f,stderr=subprocess.STDOUT,env=ENV,preexec_fn=limits)
 wall=time.monotonic()-start;text=(L/(name+'.log')).read_text();assert p.returncode==0,(name,p.returncode,text[-1000:])
 result={'name':name,'kind':kind,'n':n,'mode':mode,'rep':rep,'role':role,'command':args,'whole_process_wall':wall,'exit_code':p.returncode,'result':objects(text,'RESULT ')[0],'native_phases':objects(text,'NATIVE_SUMCHECK '),'fs':next(x for x in objects(text,'') if 'challenge_digest' in x),'log':str((L/(name+'.log')).relative_to(ROOT))}
 runs.append(result);(S/'BENCHMARK_IN_PROGRESS.json').write_text(json.dumps(runs,indent=2))
 print(name,round(wall,3),flush=True)
def main():
 assert json.loads((S/'ONEBLOCK_RESULT.json').read_text())['success']
 for kind,n in [('matrix',4096),('matrix',65536),('native',64),('fconn',64)]:
  for rep in [-1,0,1,2]:
   for mode in (['sigma','masked'] if rep%2==0 else ['masked','sigma']):
    path=S/f'bench2-{kind}-{n}-{mode}-{rep}.proof'
    for role in ['prove','verify']:run(kind,n,mode,rep,role,path)
 medians=[]
 for kind,n in [('matrix',4096),('matrix',65536),('native',64),('fconn',64)]:
  for mode in ['sigma','masked']:
   for role in ['prove','verify']:
    rows=[x for x in runs if x['kind']==kind and x['n']==n and x['mode']==mode and x['role']==role and x['rep']>=0]
    medians.append({'kind':kind,'n':n,'mode':mode,'role':role,'seconds':statistics.median(x['result']['seconds'] for x in rows),'whole_process_wall':statistics.median(x['whole_process_wall'] for x in rows),'proof_bytes':statistics.median(x['result']['proof_bytes'] for x in rows),'peak_rss_kib':statistics.median(x['result']['peak_rss_kib'] for x in rows)})
 (S/'BENCHMARK.json').write_text(json.dumps({'success':True,'affinity':AFFINITY,'warmups':1,'repetitions':3,'shared_host':True,'includes_all_fixture_commitments_openings_serialization':True,'runs':runs,'medians':medians},indent=2))
 print('BENCHMARK_COMPLETE',len(runs),flush=True)
try:main()
except BaseException as e:(S/'BENCHMARK_FAILURE.json').write_text(json.dumps({'error':repr(e),'traceback':traceback.format_exc()},indent=2));raise
