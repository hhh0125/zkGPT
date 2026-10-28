from pathlib import Path
import subprocess,sys,hashlib,base64,json
sys.stdout.reconfigure(encoding='utf-8')
HERE=Path(__file__).resolve().parent
ROOT=HERE.parent
REMOTE='/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work'
STAGE='reports/R7-STAGE1-TASK0A-20261022-v1'
SSH=['ssh','-F',str(ROOT/'4090_sub2api_ssh_config'),'4090_sub2api']
def run(script,name):
 r=subprocess.run(SSH+['cd '+REMOTE+' && python3 -B -'],input=script.encode(),capture_output=True)
 (HERE/'logs').mkdir(exist_ok=True);(HERE/'logs'/(name+'.log')).write_bytes(r.stdout+r.stderr)
 print(r.stdout.decode(errors='replace'));print(r.stderr.decode(errors='replace'))
 if r.returncode:raise SystemExit(r.returncode)
if sys.argv[1]=='script':run(Path(sys.argv[2]).read_text(encoding='utf-8'),Path(sys.argv[2]).stem)
elif sys.argv[1]=='get':
 source=sys.argv[2];dest=HERE/sys.argv[3];dest.parent.mkdir(parents=True,exist_ok=True)
 r=subprocess.run(SSH+['cd '+REMOTE+' && cat '+source],capture_output=True)
 if r.returncode:print(r.stderr.decode());raise SystemExit(r.returncode)
 dest.write_bytes(r.stdout);print(dest,len(r.stdout),hashlib.sha256(r.stdout).hexdigest())
elif sys.argv[1]=='publish':
 payload={}
 for folder in ['delivery','support']:
  for p in sorted((HERE/folder).rglob('*')):
   if p.is_file():payload[str(p.relative_to(HERE)).replace('\\','/')]=base64.b64encode(p.read_bytes()).decode()
 script='from pathlib import Path\nimport base64,json\nr=Path('+repr(REMOTE)+')\nassert Path.cwd()==r\ns=r/'+repr(STAGE)+'\npayload=json.loads('+repr(json.dumps(payload))+')\n'
 script+='for n,b in payload.items():\n p=s/n\n assert not p.exists(),str(p)\n p.parent.mkdir(parents=True,exist_ok=True)\n p.write_bytes(base64.b64decode(b))\nprint("published",len(payload))\n'
 run(script,'publish')
