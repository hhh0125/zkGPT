import pathlib,subprocess,json,hashlib,sys,base64,tarfile,io
ROOT=pathlib.Path(__file__).resolve().parent
WORK=ROOT.parent.parent
REPO=WORK/'zkGPT-R7-dev'
REMOTE='/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work'
STAGE='reports/R7-OPT3D-U-STREAMING-20261102-v1'
SSH=['ssh','-F',str(WORK/'4090_sub2api_ssh_config'),'4090_sub2api']
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as stream:
  for block in iter(lambda:stream.read(4<<20),b''):h.update(block)
 return h.hexdigest()
def remote(code):
 p=subprocess.run(SSH+['python3 -'],input=code.encode(),capture_output=True)
 if p.returncode: raise RuntimeError(p.stderr.decode(errors='replace')+p.stdout.decode(errors='replace')[-5000:])
 return p.stdout
def files():
 return sorted([p.relative_to(REPO).as_posix() for p in (REPO/'src').rglob('*') if p.is_file()]+['CMakeLists.txt'])
def dump(name,obj): (ROOT/name).write_text(json.dumps(obj,indent=2),encoding='utf-8')
if __name__=='__main__':
 if sys.argv[1]=='freeze':
  current={f:sha(REPO/f) for f in files()}
  old=json.loads((WORK/'zkGPT-backups/r7-opt3c-k-folding-20261102-v1/CURRENT_SOURCE_SHA.json').read_text())
  assert current==old,('successful Protocol1 source differs',set(current)^set(old),[f for f in current if f in old and current[f]!=old[f]])
  result=json.loads(remote('import pathlib,json,hashlib\np=pathlib.Path('+repr(REMOTE)+')\ne='+repr(current)+'\nassert json.loads((p/"reports/migration-20261020/MIGRATION_RESULT.json").read_text())["success"]\na={f:hashlib.sha256((p/f).read_bytes()).hexdigest() for f in e}\nassert a==e,"remote source conflict"\ns=p/'+repr(STAGE)+'\ns.mkdir(exist_ok=True)\n(s/"BASELINE_SOURCE_SHA.json").write_text(json.dumps(a,indent=2))\nprint(json.dumps(dict(source_files=len(a),Windows_Linux_match=True)))'))
  dump('PREFLIGHT.json',result);dump('BASELINE_SOURCE_SHA.json',current);dump('LAST_SYNC_SHA.json',current)
  git=['git','-c','safe.directory='+REPO.as_posix(),'-C',str(REPO)]
  dump('BASELINE_GIT.json',{k:subprocess.check_output(git+v).decode().strip() for k,v in [('head',['rev-parse','HEAD']),('branch',['branch','--show-current']),('status',['status','--short'])]})
  (ROOT/'BASELINE_FULL_DIFF.patch').write_bytes(subprocess.check_output(git+['diff','--binary','--full-index']))
  for f in current:
   dst=ROOT/'before'/f;dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes((REPO/f).read_bytes());assert sha(dst)==current[f]
  evidence=WORK/'zkGPT-backups/r7-opt3c-k-folding-20261102-v1'
  dump('BASELINE_EVIDENCE.json',{f:sha(evidence/f) for f in ['R7_OPT3C_FULL12_RESULT.json','R7_OPT3C_FULL12_PERFORMANCE.json','R7_OPT3C_FULL12_MESSAGE_AUDIT.json','CURRENT_SOURCE_SHA.json','FINAL_DELIVERY_WINDOWS_VERIFIED.json']})
  print(json.dumps(result))
 elif sys.argv[1]=='run': sys.stdout.buffer.write(remote((ROOT/sys.argv[2]).read_text(encoding='utf-8')))
 elif sys.argv[1]=='stream':
  child=subprocess.run(SSH+['python3 -u -'],input=(ROOT/sys.argv[2]).read_bytes())
  sys.exit(child.returncode)
 elif sys.argv[1]=='sync':
  expected=json.loads((ROOT/'LAST_SYNC_SHA.json').read_text());names=sys.argv[2:]
  assert names and all(f.startswith('src/paper_r7/') and '..' not in pathlib.PurePosixPath(f).parts for f in names)
  remote('import pathlib,hashlib\np=pathlib.Path('+repr(REMOTE)+')\ne='+repr({f:expected.get(f) for f in names})+'\nfor f,h in e.items():\n q=p/f\n assert (hashlib.sha256(q.read_bytes()).hexdigest() if q.is_file() else None)==h,"SYNC CONFLICT "+f\nprint("PASS")')
  for f in names: subprocess.run(['scp','-F',str(WORK/'4090_sub2api_ssh_config'),str(REPO/f),'4090_sub2api:'+REMOTE+'/'+f],check=True)
  changed={f:sha(REPO/f) for f in names}
  remote('import pathlib,hashlib\np=pathlib.Path('+repr(REMOTE)+')\ne='+repr(changed)+'\nassert all(hashlib.sha256((p/f).read_bytes()).hexdigest()==h for f,h in e.items())\nprint("PASS")')
  expected.update(changed);dump('LAST_SYNC_SHA.json',expected);print(json.dumps(changed))

