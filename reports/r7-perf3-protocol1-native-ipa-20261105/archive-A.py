from pathlib import Path
import hashlib,json,tarfile,subprocess,os
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd()==R
S=R/'reports/r7-perf3-protocol1-native-ipa-20261105';B=R/'build/release/r7-perf3-protocol1-native-ipa-20261105'
assert json.loads((S/'candidate-retry1-DONE.json').read_text())['done']
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(4<<20),b''):h.update(b)
 return h.hexdigest()
identity=json.loads((S/'candidate-retry1-IDENTITY_AFTER.json').read_text())
for group in identity.values():
 for n,h in group.items():assert sha(R/n)==h,('STOP_SYNC',n)
toolchain={n:subprocess.check_output(args,text=True,stderr=subprocess.STDOUT) for n,args in [('compiler',['c++','--version']),('cmake',['cmake','--version']),('os',['uname','-a']),('ldd',['ldd',str(B/'src/paper_r7/r7_zero_suffix_storage_tests')])]}
toolchain.update(cpu_affinity=sorted(os.sched_getaffinity(0)),load=os.getloadavg(),cpu_model=[x for x in Path('/proc/cpuinfo').read_text().splitlines() if x.startswith('model name')][:1])
(S/'TOOLCHAIN-A.json').write_text(json.dumps(toolchain,indent=2))
(S/'BOUNDED_STATEMENT-A.json').write_text(json.dumps({'proof_type':'OneblockSingleHyrax','context':'perf3-bounded-mixed','parameter_id':'a'*64,'public_instance':[7,2,1,8],'layout':'perf3_protocol.cpp plan and execute; n65536, 3 roots, 16 distinct queries; source bytes archived','random':'test-only FixedRandom/fixed FS header nonce, production RNG unchanged'},indent=2))
paths=[p for p in S.rglob('*') if p.is_file() and p.name not in ['EVIDENCE.tar.gz','ARCHIVE.json']]
paths += [R/n for n in identity['third_party_dependencies']]
paths += [R/'AGENTS.md',R/'WORK_IDENTITY',R/'reports/migration-20261020/MIGRATION_RESULT.json']
paths += [B/'CMakeCache.txt',B/'compile_commands.json']+list((B/'Testing/Temporary').glob('*'))
paths += [p for p in (B/'lib').glob('*.so*') if p.is_file()]
paths += [B/'src/paper_r7'/n for n in ['r7_perf3_mapping_tests','r7_perf3_protocol','r7_zero_suffix_tests','r7_zero_suffix_storage_tests','r7_fs_oneblock_prover','r7_fs_oneblock_verifier','r7_fs_full12_prover','r7_fs_full12_verifier','libpaper_r7.a']]
paths += [p for p in B.rglob('*.o.d') if p.is_file()]
paths += [p for p in B.rglob('link.txt') if p.is_file()]
paths += [p for p in B.rglob('flags.make') if p.is_file()]
paths += [B/'src/paper_r2/libpaper_r2.a', B/'src/paper_r1/libpaper_r1.a', B/'src/libgpt_lib.a', R/'deps/cnpy/lib/libcnpy.a']
manifest={};target=S/'EVIDENCE-A.tar.gz'
with tarfile.open(target,'x:gz',compresslevel=1,dereference=True) as z:
 for p in sorted(set(paths)):
  if not p.is_file():continue
  n=p.relative_to(R).as_posix();assert not any(x in ['.git','.ssh'] for x in Path(n).parts)
  h=sha(p);z.add(p,arcname=n,recursive=False);assert sha(p)==h
  manifest[n]={'sha256':h,'bytes':p.stat().st_size}
result={'path':target.relative_to(R).as_posix(),'sha256':sha(target),'bytes':target.stat().st_size,'members':manifest}
(S/'ARCHIVE-A.json').write_text(json.dumps(result,indent=2));print(json.dumps({'bytes':result['bytes'],'sha256':result['sha256'],'members':len(manifest)}))
