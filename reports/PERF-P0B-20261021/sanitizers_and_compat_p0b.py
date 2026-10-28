from pathlib import Path
import os,sys,json,subprocess,shlex,shutil,hashlib
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(r);os.sched_setaffinity(0,[0,1,2,3]);d=r/'reports/PERF-P0B-20261021';logs=r/'logs/PERF-P0B-20261021'
sys.path.insert(0,str(r/'reports/R6B-OPT2-20261021'));import r6b_resources
sys.path.insert(0,str(d));from oneblock_resource_policy import check
ns={};exec(compile((d/'oneblock_supervisor.py').read_text(),'bounded small-test supervisor','exec'),ns);ns['r']=d;ns['logs']=logs
def bounded(name,args,expected=0,asan=False,env=None):
 row=r6b_resources.snapshot(r);row['WARN_other_user_large_jobs']=check(row,8);(d/(name+'-resource.json')).write_text(json.dumps(row,indent=2))
 ns['run'](name,args,expected=expected,timeout=300,mem_gib=None if asan else 8,rss_gib=8,env=env,disk_gib=64,reserve_gib=32)
def build(name,args):
 p=subprocess.run(args,capture_output=True,text=True);(logs/(name+'.log')).write_text(shlex.join(args)+'\n'+p.stdout+p.stderr);assert p.returncode==0,(name,(p.stdout+p.stderr)[-2500:]);print(name,'PASS',flush=True)
common=['c++','-O1','-g','-fno-omit-frame-pointer','-std=c++17','-pthread','-DMCLBN_NO_AUTOLINK','-DMCL_NO_AUTOLINK','-DMCL_USE_LLVM=1','-Isrc/paper_r1','-I3rd/mcl/include']
runtime=['build/release/lib/libmclbn256.so.1.74','build/release/lib/libmcl.so.1.74','-Wl,-rpath,'+str(r/'build/release/lib')]
commands=[]
for mode,san in [('asan','address'),('ubsan','undefined')]:
 out=d/mode;out.mkdir();lib=out/'libpaper_r1.a';shutil.copy2('build/release/src/paper_r1/libpaper_r1.a',lib)
 obj=out/'mapped_input.cpp.o';args=common+['-fsanitize='+san,'-c','src/paper_r1/mapped_input.cpp','-o',str(obj)];commands.append(args);build('15-'+mode+'-mapped-build',args)
 build('16-'+mode+'-archive',['ar','r',str(lib),str(obj)])
 for tag,source in [('kernel','tests_mapped_kernel.cpp'),('mapped-protocol','tests_mapped_input.cpp')]:
  binary=out/(tag+'-tests');args=common+['-fsanitize='+san,'src/paper_r1/'+source,str(lib)]+runtime+['-o',str(binary)];commands.append(args);build('17-'+mode+'-'+tag+'-build',args)
  env={'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1:abort_on_error=1:quarantine_size_mb=64'} if mode=='asan' else {'UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'}
  bounded('18-'+mode+'-'+tag,[str(binary)],asan=mode=='asan',env=env)
(d/'SANITIZER_BUILD_COMMANDS.json').write_text(json.dumps(commands,indent=2))
(d/'SANITIZER_SCOPE.md').write_text('Targeted mapped_input.cpp TU, kernel differential and real small mapped-protocol fixture. ASan virtual shadow requires no RLIMIT_AS; RSS remains capped8GiB and system/cgroup32GiB reserve/disk64GiB/4CPU enforced. This is an ASan small test, not the realoneblock Verifier; its8GiB runtime limit has not been relaxed. UBSan retains8GiB address+RSS. No production compile flags changed; native MCL assembly/unmodified archive members are not fully sanitizer-instrumented.\n')
# Small FCONN integration fixture, not the GPT/R5B oneblock workload.
c=d/'small-integration-compatibility';c.mkdir();meta=[]
for mode in ['before','after']:
 folder=c/mode;folder.mkdir()
 base=d/'frozen/build/release/src/paper_r2' if mode=='before' else r/'build/release/src/paper_r2'
 bounded('19-small-'+mode+'-generate',[str(base/'r2_integration_tests'),str(folder)])
for name in ['valid.statement','valid.proof']:
 assert (c/'before'/name).read_bytes()==(c/'after'/name).read_bytes(),'BYTE_INCOMPATIBILITY_STOP'
for verifier in ['before','after']:
 binary=d/'frozen/build/release/src/paper_r2/r2_verify' if verifier=='before' else r/'build/release/src/paper_r2/r2_verify'
 for source in ['before','after']:
  for statement in sorted((c/source).glob('*.statement')):
   name='20-small-'+verifier+'-on-'+source+'-'+statement.stem;expected=0 if statement.stem=='valid' else 2
   bounded(name,[str(binary),str(statement),str(statement.with_suffix('.proof'))],expected=expected)
   text=(logs/(name+'.log')).read_text();assert ('ACCEPT' in text) if expected==0 else ('REJECT' in text)
   meta.append({'name':name,'expected_exit':expected,'actual_exit':expected,'PASS':True})
ident={str(p.relative_to(c)):{'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'bytes':p.stat().st_size} for p in c.rglob('*') if p.is_file()}
(d/'SMALL_COMPATIBILITY_RESULTS.json').write_text(json.dumps({'scope':'small four-FCONN/GKR+Range+Exp+bridge+checked-input fixture,NOT R5Boneblock','valid_bytes_identical':True,'results':meta,'artifacts':ident},indent=2))
with (d/'progress.md').open('a') as f:f.write('ASan/UBSan targetedkernel+smallmappedprotocol4tests PASS; no sanitizerfindings. Small4FCONN fixture old/new statement+proofbyte-identical,all crossverifications expected,negativefixturesREJECT. NotrealR5Boneblock.\n')
print('SANITIZERS_AND_SMALL_CROSS_COMPATIBILITY_PASS',flush=True)
