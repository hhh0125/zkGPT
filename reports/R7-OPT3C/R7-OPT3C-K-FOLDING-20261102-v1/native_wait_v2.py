import pathlib,time,subprocess,os,json
p=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(p);s=p/'reports/R7-OPT3C-K-FOLDING-20261102-v1'
while True:
 dirs=sorted(d for d in s.glob("bench-*") if d.is_dir())
 assert dirs
 d=dirs[-1]
 if (d/"COMPLETE.json").exists():break
 time.sleep(10)
r=subprocess.run(["python3",str(s/"bench_native.py")]);(s/"NATIVE_RUNNER_EXIT.json").write_text(json.dumps(dict(exit_code=r.returncode)))
