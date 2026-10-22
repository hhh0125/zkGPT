import pathlib,subprocess,tempfile,resource,sys
with tempfile.TemporaryDirectory(prefix='r2-budget-') as d:
 target=pathlib.Path(d)/'oversized'
 def cap():resource.setrlimit(resource.RLIMIT_AS,(256*1024*1024,256*1024*1024))
 p=subprocess.run([sys.argv[1],str(1<<27),str(target)],capture_output=True,text=True,preexec_fn=cap,timeout=10)
 if p.returncode!=1 or 'lookup exceeds selected memory budget' not in p.stderr or target.exists():
  raise RuntimeError(f'budget guard ran too late: exit={p.returncode}, output={p.stdout+p.stderr}, output_dir={target.exists()}')
 print('PASS memory budget rejects before witness allocation, commitment, or artifact creation')
