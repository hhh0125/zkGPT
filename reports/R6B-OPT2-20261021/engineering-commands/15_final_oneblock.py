from pathlib import Path
import os,json,hashlib,threading,time,datetime
r=Path.cwd();d=r/'reports/R6B-OPT2-20261021';l=r/'logs/R6B-OPT2-20261021'
os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
for k in list(os.environ):
 if k not in ['PATH','HOME','USER','LANG','LC_ALL','TZ']:del os.environ[k]
os.environ['PATH']='/usr/bin:/bin'
h=Path('reports/R6A/helper.py').read_bytes();assert hashlib.sha256(h).hexdigest()=='14c6207100a29a00aa0bcc6795ec0a2ddb120eca0edc0e6f149713b936875351'
ns={};exec(compile(h,'frozen-r6a-helper','exec'),ns);ns['r']=d;ns['logs']=l
def sample_run(name,args,**kw):
 stop=threading.Event();start=time.monotonic()
 def sample():
  with (l/(name+'-rss-phases.jsonl')).open('x') as out:
   while not stop.is_set():
    todo=[os.getpid()];rss=0;processes=[]
    while todo:
     pid=todo.pop()
     try:
      todo.extend(map(int,Path('/proc',str(pid),'task',str(pid),'children').read_text().split()))
      if pid==os.getpid():continue
      row={x.split(':')[0]:x.split(':',1)[1].strip() for x in Path('/proc',str(pid),'status').read_text().splitlines()}
      n=int(row.get('VmRSS','0 kB').split()[0])*1024;rss+=n;processes.append({'pid':pid,'name':row.get('Name'),'rss':n,'threads':row.get('Threads')})
     except (FileNotFoundError,ProcessLookupError,PermissionError):pass
    log=l/(name+'.log');phase='startup'
    if log.exists():
     with log.open('rb') as f:f.seek(max(0,log.stat().st_size-8192));tail=f.read().decode(errors='replace')
     lines=[s for s in tail.splitlines() if s.startswith('PHASE ')];phase=lines[-1] if lines else 'see-memory-diagnostics-log'
    out.write(json.dumps({'elapsed':time.monotonic()-start,'rss_bytes':rss,'phase':phase,'processes':processes})+'\n');out.flush();stop.wait(.5)
 thread=threading.Thread(target=sample,daemon=True);thread.start()
 try:ns['run'](name,args,**kw)
 finally:stop.set();thread.join()
anchor=str(r/'reports/R5B/operator-trust-v2.anchor');pin='cd339d42738ef8ffd2bce5853d0bbbbae7a32c9e80332dafe93de5e93e0c25aa';manifest=str(r/'reports/R5B/engineering-model-manifest-v1.txt');opts=['--trusted-anchor',anchor,'--anchor-sha256',pin,'--model-manifest',manifest]
sample_run('final-oneblock-prover',[str(r/'build/release/src/paper_r2/r5b_model_prove'),str(d/'final-oneblock')]+opts,timeout=1500,mem_gib=24,disk_gib=64,env={'ZKGPT_MEMORY_DIAGNOSTICS':'1'})
rows={}
for name in ['model.statement','model.proof']:
 b=(r/'reports/R6B-OPT-20261021/optimized-oneblock'/name).read_bytes();o=(d/'final-oneblock'/name).read_bytes();old=(r/'reports/R5B/normal-v2'/name).read_bytes()
 assert b==o,'serialized proof/statement differs: '+name
 rows[name]={'baseline_sha256':hashlib.sha256(b).hexdigest(),'optimized_sha256':hashlib.sha256(o).hexdigest(),'same_bytes':True,'bytes':len(o),'identical_to_original_independently_accepted_R5B':o==old}
 assert o==old,'historical accepted proof bytes differ: '+name
(d/'FINAL_ONEBLOCK_BYTE_COMPATIBILITY.json').write_text(json.dumps(rows,indent=2))
empty=d/'final-independent-empty';empty.mkdir(exist_ok=False);assert not list(empty.iterdir())
sample_run('final-oneblock-independent',[str(r/'build/release/src/paper_r2/r5b_model_verify'),str(d/'final-oneblock/model.statement'),str(d/'final-oneblock/model.proof')]+opts,timeout=1200,mem_gib=8,disk_gib=64,cwd=str(empty))
assert 'ACCEPT R5B engineering candidate' in (l/'final-oneblock-independent.log').read_text()
assert not list(empty.iterdir())
with (d/'PROGRESS.md').open('a') as f:f.write('Final realoneblockP+independentV complete. Statement/proof byteidentical to freshbeforebaseline AND originalindependentlyACCEPTED R5Bnormal-v2; no input/model/protocol change. Full12 still notexecuted.\n')

