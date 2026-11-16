"""Materialized, credential-excluding stage archive with per-file SHA manifest."""
from pathlib import Path
import hashlib,json,tarfile,sys,subprocess
R=Path.cwd();S=R/'reports/R7-PUBLIC-BUILD-20261113-v1';mode=sys.argv[1];assert mode in ['baseline','final']
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for chunk in iter(lambda:f.read(4<<20),b''):h.update(chunk)
 return h.hexdigest()
paths=[]
if mode=='baseline':
 assert (S/'BASELINE_DONE.json').exists()
 for p in S.iterdir():
  if (p.name.startswith(('baseline-1-','baseline-12-','baseline-probe-','baseline-enabled-')) or p.name in ['baseline-source.tar.gz','baseline-instrumented.tar.gz','baseline-ctest.log','baseline-ctest.json','REMOTE_BASELINE.json','REMOTE_GIT_STATUS.txt','REMOTE_HEAD.txt','RED_API.log','RED_DUPLICATE_COUNTS.json','RED_STDOUT.log','PARAMETERS.json','TOOLCHAIN.json','REQUEST.zh-CN.md','public_build_trace.hpp','public_build_probe.cpp']):
   paths.extend(p.rglob('*') if p.is_dir() else [p])
 build=R/'build/release/public-build-baseline-20261113-v1'
else:
 assert (S/'RUNTIME_DONE.json').exists() and (S/'FINAL_CHECKS_DONE.json').exists()
 for n,h in json.loads((S/'FINAL_SOURCE_SHA256.json').read_text()).items():assert sha(R/n)==h,('source drift before archive',n)
 for n,h in json.loads((S/'PARAMETERS.json').read_text()).items():assert sha(R/n)==h,('parameter drift before archive',n)
 paths.extend(p for p in S.rglob('*') if 'archives' not in p.relative_to(S).parts and p.suffix!='.tar.gz')
 paths += [S/'baseline-source.tar.gz',S/'baseline-instrumented.tar.gz',S/'traced-source.tar.gz']
 paths += list((R/'src').rglob('*'))+[R/'CMakeLists.txt',R/'build.sh',R/'AGENTS.md',R/'docs/superpowers/plans/2026-11-13-r6-r7-public-build.md']
 paths += [R/'reports/WORKSPACE/migration-20261020/MIGRATION_RESULT.json',R/'reports/WORKSPACE/cleanup-20261020/REMOTE_CLEANUP_RESULT.json']
 paths += list((R/'deps/cnpy').rglob('*'))
 paths += [p for p in (R/'3rd').rglob('*') if not any(x in ['.git','obj','bin','lib','CMakeFiles','__pycache__'] or x.startswith('build') for x in p.relative_to(R/'3rd').parts) and p.suffix not in ['.o','.a','.so','.dll','.exe','.pyc']]
 paths += list((R/'data/gpt2_int').rglob('*'))
 paths += list((R/'reports/R6-BASELINE/R6A').glob('*manifest*.txt'))
 from_references=[R/'reports/R6-ROWMAJOR/R6-ROWMAJOR-v2/oneblock-operator-trust-rowmajor-v2.anchor',R/'reports/R7-FULL12/R7-FULL12-UNRESTRICTED-20261029-v1/full12-chunks.anchor']
 paths += [p for p in from_references if p.exists()]
 paths += list((R/'tools').glob('public_build_*'))
 paths += [R/'tools/product_leaves_r9_method.py']
 build=R/'build/release/public-build-20261113-v1'
paths += [build/'CMakeCache.txt',build/'compile_commands.json']
paths += list((build/'Testing/Temporary').glob('*'))+list((build/'lib').glob('*.so*'))
paths += [build/'src/paper_r7'/n for n in ['r7_fs_oneblock_prover','r7_fs_oneblock_verifier','r7_fs_full12_prover','r7_fs_full12_verifier','r7_public_build_tests'] if (build/'src/paper_r7'/n).exists()]
files={p.relative_to(R).as_posix():p for p in paths if p.is_file() and '__pycache__' not in p.parts}
for n in files:
 assert not any(k in n.lower() for k in ['4090_sub2api_ssh_config','id_rsa','id_ed25519','authorized_keys','known_hosts','credentials','private_key'])
manifest={n:{'sha256':sha(p),'bytes':p.stat().st_size} for n,p in sorted(files.items())}
out=S/'archives';out.mkdir(exist_ok=True);mp=out/(mode+'.manifest.json');assert not mp.exists();mp.write_text(json.dumps(manifest,indent=2))
target=out/(mode+'.tar.gz');assert not target.exists()
with tarfile.open(target,'x:gz',compresslevel=1,dereference=True) as t:
 for n,p in sorted(files.items()):t.add(p,arcname=n,recursive=False)
 t.add(mp,arcname='ARCHIVE_MANIFEST.json',recursive=False)
receipt={'archive':str(target.relative_to(R)),'sha256':sha(target),'bytes':target.stat().st_size,'files':len(manifest),'manifest_sha256':sha(mp),'materialized_regular_files':True,'credentials_excluded':True}
(out/(mode+'.receipt.json')).write_text(json.dumps(receipt,indent=2));print(json.dumps(receipt))
