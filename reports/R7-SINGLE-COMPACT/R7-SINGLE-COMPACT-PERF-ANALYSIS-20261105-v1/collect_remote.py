from pathlib import Path
import subprocess,sys
out=Path(__file__).resolve().parent
workspace=out.parents[2]
cmd=['ssh','-F',str(workspace/'4090_sub2api_ssh_config'),'-o','BatchMode=yes','4090_sub2api','cd /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work && python3 -B -']
p=subprocess.run(cmd,input=(out/'remote_preflight.py').read_bytes(),capture_output=True)
(out/'REMOTE_PREFLIGHT.stderr.txt').write_bytes(p.stderr)
if p.returncode:
 sys.stderr.buffer.write(p.stderr);raise SystemExit(p.returncode)
(out/'REMOTE_PREFLIGHT.json').write_bytes(p.stdout)
print('Remote preflight received',len(p.stdout),'bytes')
