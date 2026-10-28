from pathlib import Path
import sys,base64,json
sys.path.insert(0,str(Path(__file__).resolve().parent))
import importlib.util
# Import only the transport helper source before its CLI dispatch by using an explicit SSH call.
import subprocess
here=Path(__file__).resolve().parent;root=here.parent
payload={n:(here/n).read_text(encoding='utf-8') for n in ['research_math.py','static_resources.py']}
script='from pathlib import Path\nimport json,subprocess,sys\nr=Path("/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work")\nassert Path.cwd()==r\ns=r/"reports/R7-STAGE1-TASK0A-20261022-v1/research"\npayload=json.loads('+repr(json.dumps(payload))+')\n'
script+='for name,body in payload.items():\n p=s/name\n assert not p.exists()\n p.write_text(body)\n result=subprocess.run([sys.executable,"-B",str(p)],cwd=r,capture_output=True,text=True,timeout=65)\n (s/(name+".log")).write_text(result.stdout+result.stderr)\n print(name,result.returncode,result.stdout,result.stderr)\n assert result.returncode==0\n'
r=subprocess.run(['ssh','-F',str(root/'4090_sub2api_ssh_config'),'4090_sub2api','cd /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work && python3 -B -'],input=script.encode(),capture_output=True)
(here/'logs/RESEARCH_EXECUTION.log').write_bytes(r.stdout+r.stderr);print(r.stdout.decode());print(r.stderr.decode());raise SystemExit(r.returncode)
