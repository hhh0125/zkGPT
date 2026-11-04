from pathlib import Path
import os,sys,json,hashlib,tarfile,io,subprocess,re
from run_validation import ROOT,BASE,BIN,sha
os.chdir(ROOT)
scope=sys.argv[1] if len(sys.argv)>1 else 'units'
assert scope in ['units','oneblock','full12']
if scope=='units':assert json.loads((BASE/'UNIT_GATE.json').read_text())['success']
else:assert json.loads((BASE/(scope+'-v1')/'RESULT.json').read_text())['success']
out=BASE/('archive-'+scope+'-v1');out.mkdir(exist_ok=False)
files={}
def tree(path,prefix,skip=()):
 for p in Path(path).rglob('*'):
  if p.is_file() and not any(x in p.parts for x in ['.git','__pycache__']) and not p.name.endswith(skip):files[prefix+'/'+p.relative_to(path).as_posix()]=p
if scope=='units':
 for p in Path('src').rglob('*'):
  if p.is_file():files['source/'+p.as_posix()]=p
 for n in ['CMakeLists.txt','build.sh']:files['source/'+n]=Path(n)
 tree('3rd/mcl','source/3rd/mcl')
 tree('deps/cnpy','dependencies/cnpy')
 tree('data','parameters/data')
 for p in BIN.iterdir():
  if p.is_file() and os.access(p,os.X_OK):files['binaries/'+p.name]=p
 b=Path('build/balanced-hyrax-20261103-v2')
 for p in [b/'CMakeCache.txt',b/'Testing/Temporary/LastTest.log',*b.glob('CMakeFiles/*/CMakeCXXCompiler.cmake')]:
  if p.is_file():files['configuration/'+p.relative_to(b).as_posix()]=p
 runtime=subprocess.check_output(['ldd',str(BIN/'r7_fs_full12_verifier')],text=True)
 for n in re.findall(r'(?:=>\s+|^\s*)(/[^\s]+)',runtime,re.M):
  p=Path(n)
  if p.is_file():files['runtime/'+p.name]=p
 commands={'g++':['g++','--version'],'cmake':['cmake','--version'],'uname':['uname','-a'],'ldd':['ldd',str(BIN/'r7_fs_full12_verifier')]}
 config={k:subprocess.check_output(v,text=True) for k,v in commands.items()}
 config.update(source_count=sum(n.startswith('source/src/') for n in files),security_status='FS_NIZK_CANDIDATE',production_gate='BLOCKED_CRYPTOGRAPHIC_DESIGN',may_enter_stage2=False)
 (out/'TOOLCHAIN.json').write_text(json.dumps(config,indent=2));files['configuration/TOOLCHAIN.json']=out/'TOOLCHAIN.json'
 for p in BASE.iterdir():
  if p.is_file() and p.suffix in ['.json','.log','.md','.diff','.py'] and p.name not in ['ACTIVE.json','pipeline-v2.log','pipeline.log']:files['evidence/'+p.name]=p
 for directory in ['explicit-tests-v1','explicit-retry-v2','red-tests','green-source','runtime-metrics','review-red','review-fix']:
  if (BASE/directory).is_dir():tree(BASE/directory,'evidence/'+directory)
else:
 tree(BASE/(scope+'-v1'),'evidence/'+scope+'-v1')
 if scope=='oneblock' and (BASE/'independent-negatives-v1').is_dir():
  # Keep minimal byte reconstruction operations plus raw logs, not duplicate full proofs.
  for p in (BASE/'independent-negatives-v1').rglob('*'):
   if p.is_file() and p.suffix!='.fsproof':files['evidence/independent-negatives-v1/'+p.name]=p
 for n in ['windows-after.json','changes.json','TASK_ONLY.diff','PLAN.md']:
  files['evidence/'+n]=BASE/n
manifest={}
archive=out/(scope+'.tar.gz')
with tarfile.open(archive,'w:gz',compresslevel=1) as tar:
 for name,p in sorted(files.items()):
  b=p.read_bytes();manifest[name]=dict(bytes=len(b),sha256=hashlib.sha256(b).hexdigest(),origin=p.as_posix())
  assert not re.search(rb'-----BEGIN (?:OPENSSH |RSA |EC |DSA )?PRIVATE KEY-----',b),('private key marker',name)
  info=tarfile.TarInfo(name);info.size=len(b);info.mode=0o755 if os.access(p,os.X_OK) else 0o644;tar.addfile(info,io.BytesIO(b))
(out/'MANIFEST.json').write_text(json.dumps(manifest,indent=2))
receipt=dict(archive=str(archive),sha256=sha(archive),bytes=archive.stat().st_size,members=len(manifest),scope=scope)
(out/'RECEIPT.json').write_text(json.dumps(receipt,indent=2));print(json.dumps(receipt),flush=True)
