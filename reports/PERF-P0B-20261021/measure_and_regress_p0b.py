from pathlib import Path
import os,sys,subprocess,shlex,json,hashlib,re,shutil
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(r);os.sched_setaffinity(0,[0,1,2,3]);d=r/'reports/PERF-P0B-20261021';logs=r/'logs/PERF-P0B-20261021'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
extra={}
for name in ['r2_integration_tests','r2_verify']:
 p=Path('build/release/src/paper_r2')/name
 if p.is_file():dest=d/'frozen'/p;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dest);extra[str(p)]={'sha256':sha(p),'bytes':p.stat().st_size,'copy':str(dest)}
(d/'BEFORE_SMALL_INTEGRATION_SHA.json').write_text(json.dumps(extra,indent=2))
def build(name,args):
 p=subprocess.run(args,capture_output=True,text=True);(logs/(name+'.log')).write_text(shlex.join(args)+'\n'+p.stdout+p.stderr);print(name,'exit',p.returncode,flush=True);assert p.returncode==0,(p.stdout+p.stderr)[-2500:]
common=['c++','-O3','-DNDEBUG','-std=c++17','-pthread','-DMCLBN_NO_AUTOLINK','-DMCL_NO_AUTOLINK','-DMCL_USE_LLVM=1','-Isrc','-Isrc/paper_r1','-Isrc/paper_r2','-I3rd/mcl/include','-Ideps/cnpy/include','-I'+str(d)]
libs=['build/release/src/paper_r2/libpaper_r2_verifier.a','build/release/src/libgpt_lib.a','deps/cnpy/lib/libcnpy.a','-lz',str(d/'frozen/build/release/src/paper_r1/libpaper_r1_verifier.a'),'build/release/lib/libmclbn256.so.1.74','build/release/lib/libmcl.so.1.74','-Wl,-rpath,'+str(r/'build/release/lib')]
build('10-p0b-isolated-bench-build',common+['-DR1_VERIFY_ONLY=1','-DP0B_BENCH_AFTER=1',str(d/'kernel_layout_bench.cpp')]+libs+['-o',str(d/'p0b-kernel-bench')])
sys.path.insert(0,str(r/'reports/R6B-OPT2-20261021'));import r6b_resources
sys.path.insert(0,str(d));from oneblock_resource_policy import check
ns={};exec(compile((d/'oneblock_supervisor.py').read_text(),'bounded small-test supervisor','exec'),ns);ns['r']=d;ns['logs']=logs
def bounded(name,args,timeout=300):
 row=r6b_resources.snapshot(r);row['WARN_other_user_large_jobs']=check(row,8);(d/(name+'-resource.json')).write_text(json.dumps(row,indent=2));ns['run'](name,args,timeout=timeout,mem_gib=8,disk_gib=64,reserve_gib=32)
identity=json.loads((d/'LAYOUT_BENCH_IDENTITY.json').read_text());assert sha(d/'r5b-public-layout.bin')==identity['layout_sha256'];expected=re.search(r'result_hex=(\w+)',(logs/'04-p0a-kernel-baseline.log').read_text()).group(1)
for index in range(1,4):
 name='11-p0b-isolated-kernel-'+str(index);bounded(name,[str(d/'p0b-kernel-bench'),str(d/'r5b-public-layout.bin'),str(r/identity['frozen_statement'])])
 actual=re.search(r'result_hex=(\w+)',(logs/(name+'.log')).read_text()).group(1);assert actual==expected,'MATH_INCOMPATIBILITY_STOP'
build('12-release-build-all',['cmake','--build','build/release','-j4'])
# Only foundation/small fixture tests. No R5B/R6 model entry or real oneblock.
selection=r'^(r1_.*|r2_.*|p0a_g1_validation|p0b_mapped_kernel|opt2_span_bounds|r6b_memory_ownership)$'
inventory=subprocess.run(['ctest','--test-dir','build/release','-N','-R',selection],capture_output=True,text=True);(logs/'13-regression-inventory.log').write_text(inventory.stdout+inventory.stderr);assert inventory.returncode==0
bounded('14-small-release-regression',['ctest','--test-dir','build/release','--output-on-failure','-j1','-R',selection],300)
with (d/'progress.md').open('a') as f:f.write('Task3:3 isolated P0Bkernelruns match frozenP0A32-byte result; layout hash unchanged. Fullreleasebuild succeeds; selected foundation/smallfixtures regressions pass. No realR5Boneblock entry executed.\n')
print('ISOLATED_PERFORMANCE_AND_SMALL_REGRESSION_PASS',flush=True)
