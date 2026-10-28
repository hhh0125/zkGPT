from pathlib import Path
import json,hashlib,subprocess,os,datetime
r=Path.cwd();d=r/'reports/R6-FULL12-P0AB-20261021';b=r/'build/release-r6-full12-p0ab-20261021';old=r/'reports/R6B-OPT2-20261021'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(n,v):(d/n).write_text(json.dumps(v,indent=2)+'\n')
current=json.loads((d/'SOURCE_IDENTITY.json').read_text())['files'];baseline=json.loads((old/'FINAL_SOURCE_SHA.json').read_text())
changed={n:{'OPT2':baseline.get(n),'current':h} for n,h in current.items() if baseline.get(n)!=h}
critical=['src/prover.cpp','src/prover.hpp','src/paper_r2/r6_model_prove.cpp','src/paper_r2/r6_model_verify.cpp','src/paper_r2/r6_cli.hpp','src/gkr_proof_builder.cpp','src/paper_r2/protocol.cpp','src/neuralNetwork.cpp']
assert all(current[n]==baseline[n] for n in critical)
write('P0AB_OPT2_INCLUSION.json',{'P0AB_source_match':'5c7053817202c418cd63947e8b75d83e7d04f4279abfe6b9f64cda708a6ae416','changes_since_OPT2':changed,'OPT2_critical_files_unchanged':{n:current[n] for n in critical},'implementation':'borrowed pr.sharedInputWitness const reference, cc.w owned by prover vector, val0 capacity planning intact; runtime addresses will be inspected from smoke diagnostics','protocol_or_numerical_changes_this_task':False,'P1_MSM_new_optimization':False})
commands={'compiler':['/usr/bin/c++','--version'],'cmake':['/usr/bin/cmake','--version'],'kernel':['/usr/bin/uname','-a'],'libc':['/usr/bin/ldd','--version'],'cpu':['/usr/bin/lscpu'],'toolchain_packages':['/usr/bin/dpkg-query','-W','g++','gcc','cmake','libgmp-dev','zlib1g-dev']}
rows={}
for name,cmd in commands.items():
 p=subprocess.run(cmd,capture_output=True,text=True);rows[name]={'argv':cmd,'exit':p.returncode,'stdout':p.stdout,'stderr':p.stderr}
rows['MCL_version']='1.74 (from configured shared-library sonames, source bytes hashed)';rows['build_cache_sha256']=sha(b/'CMakeCache.txt');rows['MCL_source_CMake_sha256']=sha(r/'3rd/mcl/CMakeLists.txt')
rows['compiler_flags']={str(p.relative_to(b)):p.read_text() for p in b.rglob('flags.make') if any(n in str(p) for n in ['paper_r2.dir','paper_r2_verifier.dir','paper_r1.dir','paper_r1_verifier.dir','gpt_lib.dir','mcl.dir','r6_model_'])}
write('TOOLCHAIN_AND_FLAGS.json',rows)
write('STAGE_STATUS.json',{'checked_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'task':'R6 full12 Prover plus independent Verifier','full12_prover_started':False,'full12_prover_success':False,'full12_independent_ACCEPT':False,'full12_wall_user_system_RSS_proof_statement':None,'status':'PREPARING_CURRENT_IDENTITY_AND_COMPATIBILITY','note':'R6 oneblock smoke is compatibility only. No real full12 retry is authorized.'})
print('PROVENANCE_RECORDED; OPT2 critical sources unchanged',flush=True)
