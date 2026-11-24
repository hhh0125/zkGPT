"""Run frozen Category mapping fixtures in independent processes."""
from pathlib import Path
import json,subprocess,sys,tempfile

def main():
 binary=Path(sys.argv[1]).resolve();out=Path(sys.argv[2]) if len(sys.argv)>2 else Path(tempfile.mkdtemp(prefix='r7-category-frozen-'));out.mkdir(parents=True,exist_ok=True);records=[]
 def run(name,args):
  p=subprocess.run([str(binary),*map(str,args)],capture_output=True,text=True,timeout=60);(out/(name+'.log')).write_text(p.stdout+p.stderr);records.append({'name':name,'exit_code':p.returncode});assert p.returncode==0,(name,p.stderr[-3000:]);return p
 run('guards',['guards'])
 for mode in ['ram','file']:
  proof=out/(mode+'.proof');p=run(mode+'-P',['prove',mode,proof]);v=run(mode+'-V',['verify',mode,proof]);a,b=json.loads(p.stdout),json.loads(v.stdout)
  for key in ['challenge_digest','transcript_digest','session','proof_sha256','proof_bytes','complete']:assert a[key]==b[key],key
  run(mode+'-cross-storage-V',['verify','file' if mode=='ram' else 'ram',proof])
 (out/'RESULT.json').write_text(json.dumps({'success':True,'records':records},indent=2)+'\n');print('PASS frozen Category mapping and independent RAM/file/cross-storage P/V',out)
if __name__=='__main__':main()
