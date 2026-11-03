from control import *
import time
code='''import pathlib,json,os,datetime
p=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');s=p/'reports/R7-OPT3D-U-STREAMING-20261102-v1';out=dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat())
def read(q):
 try:return json.loads(q.read_text())
 except (FileNotFoundError,json.JSONDecodeError):return None
b=read(s/'BENCHMARK_STATUS.json')
if b:
 out['benchmark']={k:b.get(k) for k in ['phase','n','variant','completed']};out['active']=[]
 for job in b.get('running',[]):
  pid=job['pid'];r=dict(job)
  try:
   stat=(pathlib.Path('/proc')/str(pid)/'stat').read_text().split();r['CPU_seconds']=(int(stat[13])+int(stat[14]))/os.sysconf('SC_CLK_TCK');r['RSS_kib']=int(stat[23])*os.sysconf('SC_PAGE_SIZE')//1024
  except (FileNotFoundError,PermissionError,ProcessLookupError):r['ended']=True
  out['active'].append(r)
for scope in ['oneblock','full12']:
 st=read(s/(scope+'-v1')/'STATUS.json')
 if st:out[scope]={k:st.get(k) for k in ['state','wall_seconds','verified_count','success','memory_kib','proof_storage']}
gate=read(s/'BENCHMARK_GATE.json')
if gate:out['benchmark_gate']=dict(success=gate['success'],native=gate['native_medians'],small_2pow24=gate['small_medians']['16777216'])
if (s/'FINAL_COMPLETE.json').exists():out['remote_final_complete']=True
print(json.dumps(out))
'''
while True:
 state=json.loads((ROOT/'PIPELINE_STATUS.json').read_text());finish=json.loads((ROOT/'FINISH_STATUS.json').read_text()) if (ROOT/'FINISH_STATUS.json').exists() else {}
 print(json.dumps(dict(pipeline=state.get('state'),step=state.get('step'),finish=finish.get('state'))),flush=True)
 try:print(remote(code).decode().strip(),flush=True)
 except Exception as e:print('MONITOR_READ_ERROR',str(e)[-500:],flush=True)
 if state.get('state')=='FAILED' or finish.get('state') in ['FAILED','COMPLETE']:break
 time.sleep(55)
