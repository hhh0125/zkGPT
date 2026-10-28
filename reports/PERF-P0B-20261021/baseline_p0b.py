from pathlib import Path
import os,sys,json,subprocess,shlex,hashlib,shutil
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(r);os.sched_setaffinity(0,[0,1,2,3]);d=r/'reports/PERF-P0B-20261021';logs=r/'logs/PERF-P0B-20261021'
def run_build(name,args):
 p=subprocess.run(args,capture_output=True,text=True);(logs/(name+'.log')).write_text(shlex.join(args)+'\n'+p.stdout+p.stderr);assert p.returncode==0,(name,(p.stdout+p.stderr)[-3000:])
common=['c++','-O3','-DNDEBUG','-std=c++17','-pthread','-DMCLBN_NO_AUTOLINK','-DMCL_NO_AUTOLINK','-DMCL_USE_LLVM=1','-Isrc','-Isrc/paper_r1','-Isrc/paper_r2','-I3rd/mcl/include','-Ideps/cnpy/include','-I'+str(d)]
libs=['build/release/src/paper_r2/libpaper_r2_verifier.a','build/release/src/libgpt_lib.a','deps/cnpy/lib/libcnpy.a','-lz',str(d/'frozen/build/release/src/paper_r1/libpaper_r1_verifier.a'),'build/release/lib/libmclbn256.so.1.74','build/release/lib/libmcl.so.1.74','-Wl,-rpath,'+str(r/'build/release/lib')]
old=(d/'mapped_input.p0a.cpp').read_text();marker='const unsigned d=std::max(child.size(),master.size());';assert marker in old
(d/'mapped_input.baseline-counted.cpp').write_text(old.replace(marker,'P0B_KERNEL_HIT(fallback);\n    '+marker,1))
run_build('01-export-layout-build',common+[str(d/'export_layout.cpp')]+libs+['-o',str(d/'export-layout')])
run_build('02-p0a-kernel-build',common+['-DR1_VERIFY_ONLY=1',str(d/'kernel_layout_bench.cpp')]+libs+['-o',str(d/'p0a-kernel-bench')])
sys.path.insert(0,str(r/'reports/R6B-OPT2-20261021'));import r6b_resources
sys.path.insert(0,str(d));from oneblock_resource_policy import check
ns={};exec(compile((d/'oneblock_supervisor.py').read_text(),'bounded small-test supervisor','exec'),ns);ns['r']=d;ns['logs']=logs
def bounded(name,args,timeout):
 row=r6b_resources.snapshot(r);row['WARN_other_user_large_jobs']=check(row,8);(d/(name+'-resource.json')).write_text(json.dumps(row,indent=2));ns['run'](name,args,timeout=timeout,mem_gib=8,disk_gib=64,reserve_gib=32)
bounded('03-export-public-layout',[str(d/'export-layout'),str(d/'r5b-public-layout.bin')],90)
identity={'layout_sha256':hashlib.sha256((d/'r5b-public-layout.bin').read_bytes()).hexdigest(),'layout_bytes':(d/'r5b-public-layout.bin').stat().st_size,'point_source':'deterministic synthetic full-field FS stream; not frozen proof transcript','frozen_statement':'reports/PERF-P0A-ONEBLOCK-R2-20261021/real-oneblock-once/production-after/model.statement','no_complete_oneblock_execution':True}
(d/'LAYOUT_BENCH_IDENTITY.json').write_text(json.dumps(identity,indent=2))
bounded('04-p0a-kernel-baseline',[str(d/'p0a-kernel-bench'),str(d/'r5b-public-layout.bin'),str(r/identity['frozen_statement'])],300)
assert Path('src/paper_r1/mapped_input.cpp').read_bytes()==(d/'mapped_input.p0a.cpp').read_bytes()
with (d/'progress.md').open('a') as f:f.write('P0Aisolatedkernelbaseline finished before any production source edit; originalDP+entrycounter only,51publicterminal layout and syntheticpoints,NOT end-to-end. All originalsource unchanged.\n')
print('P0A_KERNEL_BASELINE_COMPLETE_NO_SOURCE_EDIT',flush=True)
