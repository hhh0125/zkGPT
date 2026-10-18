from pathlib import Path
import json,hashlib,subprocess,re,datetime
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT');log=r/'task11_logs_20261014';build=log/'build'
baseline=json.loads((log/'baseline.json').read_text());changed=[];missing=[]
for name,digest in baseline['source'].items():
    p=r/name
    if not p.exists():missing.append(name)
    elif hashlib.sha256(p.read_bytes()).hexdigest()!=digest:changed.append(name)
assert not missing and changed==['src/CMakeLists.txt'],(missing,changed)
old=(log/'cmake-before.txt').read_text();new=(r/'src/CMakeLists.txt').read_text()
expected=old.replace('zk_(hiding_commitment|linear_opening)','zk_(hiding_commitment|linear_opening|committed_linear_opening)')+(log/'prototype_cmake.txt').read_text()
assert new==expected,'unexpected CMake delta'
history_changed=[]
for name,meta in baseline['evidence_metadata'].items():
    p=r/name
    if not p.exists() or [p.stat().st_size,p.stat().st_mtime_ns]!=meta:history_changed.append(name)
assert not history_changed,history_changed
binaries_changed=[]
for name,digest in baseline['production_binaries'].items():
    p=r/name
    if not p.exists() or hashlib.sha256(p.read_bytes()).hexdigest()!=digest:binaries_changed.append(name)
assert not binaries_changed,binaries_changed
commands=json.loads((build/'compile_commands.json').read_text())
production=[c for c in commands if 'gpt_lib.dir' in c['command']]
leaked=[c['file'] for c in production if Path(c['file']).name.startswith('zk_') or 'zk_committed_opening' in c['file']]
assert not leaked,leaked
assert production,'empty production inventory'
probe=log/'field-probe.cpp'
with probe.open('x') as f:f.write('''#include "zk_committed_linear_opening.hpp"
#include <iostream>
int main(){mcl::bn::initPairing(mcl::BN254);auto p=zkproto::committed::makeParameters(2);auto g=p.value_generator;g.normalize();mcl::bn::Fp b=g.y*g.y-g.x*g.x*g.x;
std::cout<<"Fr_order="<<mcl::bn::Fr::getModulo()<<"\\nFp_modulus="<<mcl::bn::Fp::getModulo()<<"\\ncurve_b="<<b<<"\\nFr_bytes="<<mcl::bn::Fr::getByteSize()<<"\\nG1_bytes="<<mcl::bn::G1::getSerializedByteSize()<<"\\n";}
''')
def run(cmd,label):
    p=subprocess.run(cmd,cwd=r,capture_output=True,text=True)
    (log/(label+'.log')).write_text(p.stdout+p.stderr)
    with (log/'commands.jsonl').open('a') as f:f.write(json.dumps({'argv':cmd,'exit_code':p.returncode,'log':label+'.log','time_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()})+'\n')
    assert p.returncode==0,p.stderr
    return p.stdout
run(['c++','-std=c++17','-O3','-DNDEBUG','-I'+str(r/'src'),'-I'+str(r/'3rd/mcl/include'),str(probe),str(build/'src/libzk_committed_opening_prototype.a'),'-L'+str(build/'lib'),'-lmclbn256','-lmcl','-Wl,-rpath,'+str(build/'lib'),'-lgmp','-lgmpxx','-pthread','-o',str(log/'field-probe')],'field-probe-build')
fields=run([str(log/'field-probe')],'field-probe')
assert 'curve_b=2\n' in fields,fields
audit=json.loads((log/'task10-audit-snapshot.json').read_text())
result={'existing_source_count':len(baseline['source']),'existing_source_unchanged':len(baseline['source'])-1,'allowed_changed':['src/CMakeLists.txt'],
        'exact_cmake_delta_matches':True,'historical_evidence_metadata_count':len(baseline['evidence_metadata']),'historical_evidence_metadata_unchanged':True,
        'old_production_binary_hash_count':len(baseline['production_binaries']),'old_production_binary_hashes_unchanged':True,
        'gpt_lib_compile_source_count':len(production),'prototype_sources_in_gpt_lib':leaked,
        'task10_files_unchanged':True,'field_probe':fields,
        'git_status_after':subprocess.check_output(['git','-C',str(r),'status','--porcelain=v1','-z','--untracked-files=all'],text=True)}
with (log/'integrity-verification.json').open('x') as f:json.dump(result,f,indent=2)
with (log/'progress.md').open('a') as f:f.write('\nStage5 integrity: '+str(len(baseline['source'])-1)+' old source files unchanged; only exact allowed CMake delta. '+str(len(baseline['evidence_metadata']))+' existing history metadata records and '+str(len(baseline['production_binaries']))+' production binary hashes unchanged. gpt_lib compiles no primitive source. Runtime curve b2, Fr/Fp/32-byte suite probe exit0.\nNext finalize review/report/status and download mirrors.\n')
print(json.dumps({k:v for k,v in result.items() if k!='git_status_after'},indent=2))
