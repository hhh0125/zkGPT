from control import *
data=json.loads(remote('''import pathlib,json,hashlib,shutil
p=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');s=p/'reports/R7-OPT3D-U-STREAMING-20261102-v1'
allfile=sorted(s.glob('all-*.json'))[-1];rows=json.loads(allfile.read_text());assert len(rows)==2 and all(x['exit_code']==0 for x in rows)
text=(p/rows[1]['log']).read_text();assert '100% tests passed, 0 tests failed out of 94' in text
test=p/'build/opt3d-u-streaming-release/Testing/Temporary/LastTest.log';copy=s/('FINAL_CTEST_LAST_TEST-'+allfile.stem+'.log');shutil.copyfile(test,copy);body=copy.read_text()
markers=['OPT3D_HETEROGENEOUS_LARGE_ROOT_WEIGHT_BUDGET_TESTED','OPT3D_NO_PER_TILE_JOB_ALLOCATION_TESTED','OPT3D_REALLOCATED_SEALED_STORAGE_REJECTED','OPT3D_EXACT_STREAMING_COEFFICIENTS_32_ROUNDS_THREADS_1_2_4_AND_BOUNDS','OPT3C_EXACT_FS_PROOF_TRANSCRIPT_AND_ROOT_OPENINGS_TESTED']
assert all(m in body for m in markers)
result=dict(success=True,ctest=rows,ctest_count=94,markers=markers,raw_test_log=str(copy.relative_to(p)),raw_test_log_sha256=hashlib.sha256(copy.read_bytes()).hexdigest(),I1='RED repeatedlowbudget; GREEN sharedlow+allocationpreflight+heterogeneousrootstest',I2='RED explicit changedseal-before-read failure; GREEN livevector data/size validation eachborrow',M1='unmeasured reference phase/RAM null; Umapqueries structurallyzero alreadyrootlocal',M2='short nonzero span ends inside group with implicitpadding tested serial/parallel',runtime_fault_injection_or_TSAN=False)
(s/'REVIEW_FIX_VALIDATION.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
'''))
dump('REVIEW_FIX_VALIDATION.json',data)
before=(ROOT/'before/src/paper_r7/unified_input.cpp').read_text(encoding='utf-8');after=(REPO/'src/paper_r7/unified_input.cpp').read_text(encoding='utf-8')
assert before[before.index('static Round quadraticRoundImpl'):before.index('Reduction reduce(')]==after[after.index('static Round quadraticRoundImpl'):after.index('Reduction reduce(')]
print('Original OPT3-C reference calculation unchanged;94tests and review regression markers verified')
