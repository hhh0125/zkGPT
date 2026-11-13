import pathlib,json,os,signal,datetime
p=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');s=p/'reports/R7-OPT3D-U-STREAMING-20261102-v1';state=json.loads((s/'BENCHMARK_STATUS.json').read_text());parents=set();stopped=[]
for row in state['running']:
 pid=row['pid'];proc=pathlib.Path('/proc')/str(pid)
 try:
  cmd=(proc/'cmdline').read_bytes().replace(b'\0',b' ').decode();assert 'r7_opt3d_streaming_bench' in cmd and proc.stat().st_uid==os.getuid();assert (proc/'cwd').resolve()==p
  parent=int(next(l.split()[1] for l in (proc/'status').read_text().splitlines() if l.startswith('PPid:')));assert (pathlib.Path('/proc')/str(parent)/'cwd').resolve()==p
  parents.add(parent);os.killpg(pid,signal.SIGTERM);stopped.append(dict(pid=pid,command=cmd))
 except FileNotFoundError:pass
assert stopped,'no positively identified own benchmark to stop'
for parent in parents:os.kill(parent,signal.SIGTERM)
(s/'INITIAL_BENCH_STOP.json').write_text(json.dumps(dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),reason='complete required allocation/fused-byte telemetry before final freeze; no arithmetic or protocol changes; preserve partial benchmark, exclude from final formal evidence',stopped=stopped,parents=list(parents),proofs_started=False),indent=2));print('Stopped only own benchmark and its Python controller; evidence retained')
