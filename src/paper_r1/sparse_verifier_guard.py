import subprocess,resource,sys
def cap():resource.setrlimit(resource.RLIMIT_AS,(384*1024*1024,384*1024*1024))
p=subprocess.run([sys.argv[1]],capture_output=True,text=True,preexec_fn=cap,timeout=30)
print(p.stdout+p.stderr)
if p.returncode:raise RuntimeError('sparse verifier allocated a dense query/master table')
