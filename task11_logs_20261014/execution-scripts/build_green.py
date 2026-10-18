from pathlib import Path
import json,base64,hashlib,subprocess,datetime
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT');log=r/'task11_logs_20261014';build=log/'build'
payload=json.loads('@@PAYLOAD@@')
for name,b64 in payload.items():
    p=r/name
    with p.open('xb') as f:f.write(base64.b64decode(b64))
p=r/'src/CMakeLists.txt';baseline=json.loads((log/'baseline.json').read_text())
assert hashlib.sha256(p.read_bytes()).hexdigest()==baseline['source']['src/CMakeLists.txt'],'CMake changed since baseline'
old=p.read_text();needle='zk_(hiding_commitment|linear_opening)';assert old.count(needle)==1
new=old.replace(needle,'zk_(hiding_commitment|linear_opening|committed_linear_opening)')+(log/'prototype_cmake.txt').read_text()
p.write_text(new)
(log/'cmake-before.txt').write_text(old);(log/'cmake-after.txt').write_text(new)
def run(cmd,name,expected=0):
    with (log/(name+'.log')).open('x') as f:
        result=subprocess.run(['/usr/bin/time','-v']+cmd,cwd=r,stdout=f,stderr=subprocess.STDOUT)
    with (log/'commands.jsonl').open('a') as f:f.write(json.dumps({'time_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'argv':cmd,'exit_code':result.returncode,'expected_exit_code':expected,'log':name+'.log'})+'\n')
    out=(log/(name+'.log')).read_text();print(name,'exit',result.returncode,out[-5000:])
    assert result.returncode==expected,name
run(['cmake','-S',str(r),'-B',str(build),'-DCMAKE_BUILD_TYPE=Release','-DZKGPT_BUILD_COMMITTED_Y_PROTOTYPE=ON','-DZKGPT_BUILD_ZK_OPENING_PROTOTYPE=ON','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'],'configure')
run(['cmake','--build',str(build),'--target','zk_committed_opening_tests','zk_committed_opening_verify','zk_opening_tests','--parallel','1'],'build-green')
run([str(build/'src/zk_committed_opening_tests')],'unit-tests')
run([str(build/'src/zk_opening_tests')],'task10-regression')
with (log/'progress.md').open('a') as f:f.write('\nStage3: real protocol/codec/verifier uploaded; minimal CMake exclusion+opt-in targets added. Isolated Release configuration/build, committed tests and Task10 regression exit0.\nNext: independent-process negative fixtures, matched benchmark, review, integrity checks and report.\n')
