from pathlib import Path
import os,subprocess,shlex,json
r=Path.cwd();d=r/'reports/R6B-OPT2-20261021';l=r/'logs/R6B-OPT2-20261021';os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
ns={};exec(compile(Path('reports/R6A/helper.py').read_bytes(),'frozen-helper','exec'),ns);ns['r']=d;ns['logs']=l;run=ns['run']
run('release-ctest',['/usr/bin/ctest','--test-dir','build/release','--output-on-failure','-j','1'],timeout=900,mem_gib=16,disk_gib=64)
run('full12-plan-only',[str(r/'build/release/src/paper_r2/r6_model_plan'),str(d/'FULL12_OPT2_PLAN.json'),'--blocks','12','--memory-gib','8','--max-seconds','240'],timeout=270,mem_gib=8,disk_gib=64)
before=json.loads((r/'reports/R6B-OPT-20261021/FULL12_FINAL_OPT_PLAN.json').read_text());after=json.loads((d/'FULL12_OPT2_PLAN.json').read_text());changes={k:{'before':before[k],'after':after.get(k)} for k in before if before[k]!=after.get(k)};assert not changes,changes
(d/'FULL12_LAYOUT_COMPARISON.json').write_text(json.dumps({'all_previous_keys_and_values_identical':True,'new_storage_and_safety_breakdown':{k:v for k,v in after.items() if k not in before},'zero_private_field_witness':after['metadata_field_witness_elements']==0,'full12_proof':False},indent=2))
commands=[];inc=['-I'+str(r/'src'),'-I'+str(r/'src/paper_r1'),'-I'+str(r/'deps/cnpy/include'),'-I'+str(r/'3rd/mcl/include')]
for sanitizer in ['address','undefined']:
 at=d/('targeted-'+sanitizer);at.mkdir(exist_ok=False);flags=['-std=c++17','-O1','-g','-pthread','-fsanitize='+sanitizer,'-fno-omit-frame-pointer']
 if sanitizer=='undefined':flags+=['-fno-sanitize-recover=undefined']
 with (l/('sanitizer-'+sanitizer+'-build.log')).open('x') as out:
  for unit in ['src/prover.cpp','src/paper_r1/mapped_input.cpp']:
   target=at/(Path(unit).stem+'.o');cmd=['/usr/bin/c++']+flags+inc+['-c',str(r/unit),'-o',str(target)];commands.append(cmd);subprocess.run(cmd,stdout=out,stderr=subprocess.STDOUT,check=True,timeout=180)
  # Reuse exact link dependency order, replacing only the test/main/prover units.
  origin=r/'build/release/src';link=shlex.split((origin/'CMakeFiles/opt2_ownership_tests.dir/link.txt').read_text());start=link.index('-o')+2;deps=[]
  for x in link[start:]:
   deps.append(str((origin/x).resolve()) if not x.startswith('-') and (origin/x).exists() else x)
  cmd=['/usr/bin/c++']+flags+inc+[str(r/'src/main_opt2_ownership_tests.cpp'),str(at/'prover.o')]+deps+['-o',str(at/'ownership')];commands.append(cmd);subprocess.run(cmd,stdout=out,stderr=subprocess.STDOUT,check=True,timeout=180)
  cmd=['/usr/bin/c++']+flags+inc+[str(r/'src/paper_r1/tests_opt2_span_bounds.cpp'),str(at/'mapped_input.o'),str(r/'build/release/src/paper_r1/libpaper_r1.a'),'-L'+str(r/'build/release/lib'),'-Wl,-rpath,'+str(r/'build/release/lib'),'-lmcl','-lmclbn256','-pthread','-o',str(at/'span')];commands.append(cmd);subprocess.run(cmd,stdout=out,stderr=subprocess.STDOUT,check=True,timeout=180)
 for name in ['ownership','span']:
  run(sanitizer+'-'+name,[str(at/name)],timeout=180,mem_gib=0 if sanitizer=='address' else 4,rss_gib=4,disk_gib=64,env={'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
(d/'TARGETED_SANITIZER_COMMANDS.json').write_text(json.dumps({'commands':commands,'coverage':'prover.cpp + ownership test, mapped_input.cpp + span test instrumented; all other frozen dependency objects not instrumented; ASan unlimitedvirtualshadow but4GiBRSS supervised'},indent=2))
with (d/'PROGRESS.md').open('a') as f:f.write('FinalfullRelease68CTest+actualfull12metadata+ASan/UBSanproverownershipanddirectspan testscomplete. Alloldplanfieldsidentical,newstoragebreakdownonly. No full12witnessorproof.\n')
