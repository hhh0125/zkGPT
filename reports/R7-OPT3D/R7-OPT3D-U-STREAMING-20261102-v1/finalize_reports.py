from control import *
import collections,statistics,datetime,difflib,zipfile
def j(p):return json.loads(pathlib.Path(p).read_text(encoding='utf-8'))
receipts={phase:j(ROOT/(phase.upper()+'_WINDOWS_VERIFIED.json')) for phase in ['units','benchmark','oneblock','full12'] if (ROOT/(phase.upper()+'_WINDOWS_VERIFIED.json')).is_file()}
assert 'oneblock' in receipts and 'benchmark' in receipts
for r in receipts.values():
 assert all(r[k] for k in ['sha_match','every_member_match','gzip_read_complete','restore_read_match']) and sha(pathlib.Path(r['local_archive']))==r['sha256']
latest=receipts.get('full12',receipts['oneblock']);restore=pathlib.Path(latest['local_restore']);evidence=restore/'evidence';stage=evidence/'stage'
expected=j(ROOT/'CURRENT_SOURCE_SHA.json');baseline=j(ROOT/'BASELINE_SOURCE_SHA.json');assert len(expected)==404 and len(baseline)==401
assert all(sha(REPO/f)==h and sha(restore/'source'/f)==h for f,h in expected.items())
changes=sorted(f for f,h in expected.items() if h!=baseline.get(f));assert len(changes)==8
assert all(sha(REPO/f)==h for f,h in baseline.items() if f not in changes)
assert all(expected[f]==baseline[f] for f in ['src/paper_r7/public_k.cpp','src/paper_r7/public_k.hpp','src/paper_r7/classified_mapping_plan.hpp'])
dump('R7_OPT3D_SOURCE_SHA.json',expected);dump('SOURCE_CHANGE_LIST.json',changes)
def parse(p):
 groups=collections.defaultdict(list)
 for line in p.read_text(encoding='utf-8').splitlines():
  tag,_,b=line.partition(' ')
  if b.startswith('{'):
   try:groups[tag].append(json.loads(b))
   except json.JSONDecodeError:pass
 return dict(groups)
bench_restore=pathlib.Path(receipts['benchmark']['local_restore'])/'evidence';gate=j(bench_restore/'stage/BENCHMARK_GATE.json');assert gate['success']
bench={}
for kind in ['small','native']:
 d=bench_restore/pathlib.PurePosixPath(gate[kind]).name;rows=j(d/'RESULTS.json');done=j(d/'COMPLETE.json');assert done['success'];assert len(rows)==(72 if kind=='small' else 18)
 for row in rows:
  log=bench_restore/pathlib.PurePosixPath(row['log']).parts[-3]/pathlib.PurePosixPath(row['log']).parts[-2]/'run.log';assert log.is_file();row['raw_log_sha256']=sha(log)
  assert row['exit_code']==0
 for n in {r['n'] for r in rows}:assert len({r['coefficient_challenge_endpoint_digest'] for r in rows if r['n']==n})==1
 bench[kind]=dict(rows=rows,summary=done,identity=j(d/'IDENTITY.json'))
fr_sizes={r['U_bytes_read']//r['U_scan_coordinates'] for r in bench['small']['rows']}
assert fr_sizes=={32} and all(r['U_bytes_read']==32*r['U_scan_coordinates'] for r in bench['small']['rows'])
fr_storage_bytes=fr_sizes.pop() # Measured sizeof(Fr) from the all-stored public fixture.
bench.update(status='PASS',gate=gate,baseline='frozen successful OPT3-C quadraticRoundFolded',profile=j(bench_restore/'stage/PROFILE_RESULT.json'),notes=dict(U_counts='logical padded coordinate visits unchanged; U_bytes_read is actual Fr payload load bytes including repeated visits, not hardware DRAM bus transactions; real model implicitpadding yields no source load',K_counts='K fold coordinates and K state reads are separate OPT3C metrics',timers='U contraction includes source loads, Fr multiply/add and scheduling; isolated diagnostic phase kernels are nonadditive; reference subphase fields unavailable/null',native_scope='actual Full12 root dimensions/category/order and public X, repeated public U backing array and synthetic K, not actual witness/proof;3samevariant repeats share same totalCPU0-3',small_scope='serial cases with requested1/2/4threads to measure standalone scaling',memory='explicit bounded buffers/publictables plus full1MiB worker stack+guard reservations and conservative metadata; not processRSS or exact malloc/TLS accounting',allocations='ordinary C++new on main thread during round calculation, includes provider/Kfallback; excludes Cmalloc/alignednew/worker malloc; allocation bytes cumulative, not livepeak'),archives=receipts)
dump('R7_OPT3D_U_STREAMING_BENCHMARK.json',bench)
def real(scope):
 d=evidence/(scope+'-v1')
 if not d.is_dir():return dict(status='NOT_RUN')
 result=j(d/'RESULT.json');perf=j(d/'PERFORMANCE.json');audit=j(d/'MESSAGE_AUDIT.json');neg=j(d/'NEGATIVE_STATUS.json');assert result['success'] and neg['success']
 out=dict(status='PASS',result=result,performance=perf,message_audit=audit,negative=neg,metrics={})
 for role in ['P','V']:
  g=parse(d/(role+'.log'));r=dict(raw_log_sha256=sha(d/(role+'.log')))
  for tag in ['UNIFIED_SUMCHECK','UNIFIED_OPENING','UNIFIED_INPUT','OPT3C_K','OPT2_AUTH','OPT3A_AUTH','OPT2_MSM']:r[tag]=g.get(tag,[None])[-1]
  phase_cpu={z['name']:z['cpu_seconds'] for z in g.get('PHASE_CPU',[])};r['phases']=[dict(z,cpu_seconds=phase_cpu.get(z['name'])) for z in perf['prover_summary' if role=='P' else 'verifier_summary'].get('phases',[])]
  r['rounds']=g.get('OPT3D_U_ROUND',[]);roots=g.get('OPT3D_U_ROOT',[]);r['per_root_round']=roots;r['private_U_scan_coordinates']=sum(x['U_scan_coordinates'] for x in roots if x['root']!=0);r['public_X_scan_coordinates']=sum(x['U_scan_coordinates'] for x in roots if x['root']==0)
  assert all(x['U_bytes_read']%fr_storage_bytes==0 for x in roots)
  r['Fr_storage_bytes']=fr_storage_bytes
  r['private_U_source_payload_bytes']=sum(x['U_bytes_read'] for x in roots if x['root']!=0)
  r['public_X_source_payload_bytes']=sum(x['U_bytes_read'] for x in roots if x['root']==0)
  r['private_U_source_Fr_loads']=r['private_U_source_payload_bytes']//fr_storage_bytes
  r['public_X_source_Fr_loads']=r['public_X_source_payload_bytes']//fr_storage_bytes
  r['source_load_count_scope']='semantic source Fr loads, including repeated rounds; excludes implicit zero padding; payload bytes are not hardware DRAM/cache/bus transactions'
  r['U_scan_cpu_seconds']=sum(x['U_scan_cpu_seconds'] for x in roots);r['U_scan_wall_seconds']=sum(x['U_scan_wall_seconds'] for x in roots);r['U_K_product_wall_seconds']=sum(x['U_K_product_wall_seconds'] for x in roots)
  for key in ['U_bytes_read','U_K_fused_load_bytes','U_Fr_multiplications','U_Fr_additions','coefficient_Fr_multiplications','coefficient_Fr_additions','main_thread_cpp_allocations_including_K_provider','main_thread_cpp_allocated_bytes_including_K_provider']:r[key]=sum(x[key] for x in r['rounds'])
  for key in ['U_TILE_RAM_PEAK_BYTES','U_THREAD_LOCAL_BYTES','U_TOTAL_EXTRA_RAM_BYTES']:r[key]=max((x[key] for x in r['rounds']),default=0)
  r['U_address_mapping_calls']=sum(x['U_address_mapping_calls'] for x in roots);r['U_address_mapping_cpu_seconds']=sum(x['U_address_mapping_cpu_seconds'] for x in roots)
  if role=='P':assert len(r['rounds'])==(32 if scope=='full12' else 28) and len(roots)==len(r['rounds'])*(result['roots']+1)
  out['metrics'][role]=r
 return out
one=real('oneblock');full=real('full12');assert one['status']=='PASS'
old=WORK/'zkGPT-backups/r7-opt3c-k-folding-20261102-v1';oldfull=j(old/'R7_OPT3C_FULL12_PERFORMANCE.json')
dump('REAL_RUN_NORMALIZED.json',dict(oneblock=one,full12=full))
correct=dict(status='PASS',baseline_source_files=401,current_source_files=404,changed_paths=changes,K_and_mapping_source_unchanged=True,ctest=j(stage/'UNIT_GATE.json'),oneblock=one,full12=full,review=j(ROOT/'REVIEW_FIX_VALIDATION.json'),fixed_entropy_full_canonical_proof_and_transcript_equal=True,exact_Fr_root_contractions_coefficients_and_endpoints=True,round_variants=[28,32],threads=[1,2,4],short_nonzero_span_implicit_padding_tested=True,security_status='FS_NIZK_CANDIDATE',production_gate='BLOCKED_CRYPTOGRAPHIC_DESIGN',may_enter_stage2=False,archives=receipts)
fs_gate=j(stage/'FS_MODULE_GATE.json');assert fs_gate['success'];module_dir=evidence/pathlib.PurePosixPath(fs_gate['directory']).name;module=j(module_dir/'COMPLETE.json');assert module['success'] and len(module['rows'])==8
correct['supplemental_full_FS_module_28_32_rounds_45_openings']=module
dump('R7_OPT3D_U_STREAMING_CORRECTNESS.json',correct)
if full['status']=='PASS':
 dump('R7_OPT3D_FULL12_RESULT.json',full['result']);dump('R7_OPT3D_FULL12_PERFORMANCE.json',dict(**full['performance'],metrics=full['metrics']));dump('R7_OPT3D_FULL12_MESSAGE_AUDIT.json',full['message_audit'])
else:dump('R7_OPT3D_FULL12_RESULT.json',dict(status='NOT_RUN',decision=j(ROOT/'FULL12_RUN_DECISION.json')))
patch=[];stats=[]
for f in changes:
 a=(ROOT/'before'/f).read_bytes().decode('utf-8').splitlines(keepends=True) if f in baseline else [];b=(REPO/f).read_bytes().decode('utf-8').splitlines(keepends=True);delta=list(difflib.unified_diff(a,b,fromfile='a/'+f if a else '/dev/null',tofile='b/'+f));patch+=['diff --git a/'+f+' b/'+f+'\n']+([] if f in baseline else ['new file mode 100644\n'])+delta;stats.append(dict(path=f,added=sum(x.startswith('+') and not x.startswith('+++') for x in delta),removed=sum(x.startswith('-') and not x.startswith('---') for x in delta)))
(ROOT/'R7_OPT3D_U_STREAMING_DIFF.patch').write_text(''.join(patch),encoding='utf-8',newline='\n')
dest=ROOT/('patch-restore-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ'));dest.mkdir()
for f in changes:
 if f in baseline:q=dest/f;q.parent.mkdir(parents=True,exist_ok=True);q.write_bytes((ROOT/'before'/f).read_bytes())
command=['git','-c','core.autocrlf=false','apply','--whitespace=nowarn','--directory='+dest.name,str(ROOT/'R7_OPT3D_U_STREAMING_DIFF.patch')]
subprocess.run(command[:5]+['--check']+command[5:],cwd=ROOT,check=True);subprocess.run(command,cwd=ROOT,check=True);assert all(sha(dest/f)==expected[f] for f in changes)
dump('PATCH_RESTORE_WINDOWS_VERIFIED.json',dict(success=True,restore_directory=str(dest),source={f:sha(dest/f) for f in changes},exact_bytes_and_line_endings=True));dump('STAGE_DIFF_STAT.json',stats)
git=['git','-c','safe.directory='+REPO.as_posix(),'-C',str(REPO)];gitstate={k:subprocess.check_output(git+v).decode().strip() for k,v in [('head',['rev-parse','HEAD']),('branch',['branch','--show-current']),('status',['status','--short'])]};assert gitstate['head']==j(ROOT/'BASELINE_GIT.json')['head'] and gitstate['branch']==j(ROOT/'BASELINE_GIT.json')['branch'];dump('FINAL_GIT_STATE.json',gitstate)
dump('FINAL_REPORT_INPUTS.json',dict(oneblock=one,full12=full,baseline_full=oldfull,small=bench['small']['summary'],native=bench['native']['summary'],git=gitstate,archives=receipts,changes=stats))
print('Final JSON and exact patch restore verified; full12',full['status'])
