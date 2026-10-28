from pathlib import Path
import subprocess, sys, json, hashlib
sys.stdout.reconfigure(encoding='utf-8')
HERE=Path(__file__).resolve().parent
ROOT=HERE.parent
REMOTE='/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work'
STAGE='reports/R7-STAGE1-20261022-v1'
SSH=['ssh','-F',str(ROOT/'4090_sub2api_ssh_config'),'4090_sub2api']
def run(script, name):
    result=subprocess.run(SSH+['cd '+REMOTE+' && python3 -B -'],input=script.encode(),capture_output=True)
    (HERE/(name+'.log')).write_bytes(result.stdout+result.stderr)
    print(result.stdout.decode(errors='replace')); print(result.stderr.decode(errors='replace'))
    if result.returncode: raise SystemExit(result.returncode)
if sys.argv[1]=='script':
    run(Path(sys.argv[2]).read_text(encoding='utf-8'),Path(sys.argv[2]).stem)
elif sys.argv[1]=='get':
    source=sys.argv[2]; dest=HERE/sys.argv[3]; dest.parent.mkdir(parents=True,exist_ok=True)
    result=subprocess.run(SSH+['cd '+REMOTE+' && cat '+source],capture_output=True)
    if result.returncode: print(result.stderr.decode()); raise SystemExit(result.returncode)
    dest.write_bytes(result.stdout); print(dest, len(result.stdout),hashlib.sha256(result.stdout).hexdigest())
elif sys.argv[1]=='publish':
    import base64
    payload={}
    for folder in ['delivery','support']:
        for p in sorted((HERE/folder).rglob('*')):
            if p.is_file():payload[str(p.relative_to(HERE)).replace('\\','/')]=base64.b64encode(p.read_bytes()).decode()
    script='from pathlib import Path\nimport base64,json,hashlib\nr=Path('+repr(REMOTE)+')\nassert Path.cwd()==r\ns=r/'+repr(STAGE)+'\npayload=json.loads('+repr(json.dumps(payload))+')\n'
    script+='for name,b64 in payload.items():\n p=s/name\n assert not p.exists(),str(p)\n p.parent.mkdir(parents=True,exist_ok=True)\n p.write_bytes(base64.b64decode(b64))\n'
    script+='print("PUBLISHED",len(payload),"stage files; no production edits")\n'
    run(script,'publish')
