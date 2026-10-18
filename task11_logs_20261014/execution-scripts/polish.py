from pathlib import Path
import base64,json,subprocess,datetime
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT');log=r/'task11_logs_20261014';build=log/'build'
payload=json.loads('@@PAYLOAD@@')
name='src/zk_committed_linear_opening.cpp';p=r/name;old=p.read_text()
new=old.replace('for(const auto& b:s.coefficients)e.element(b);return e.bytes;','for(const auto& b:s.coefficients)e.element(b);\n    return e.bytes;').replace('if(error)error->clear();return true;','if(error)error->clear();\n        return true;')
expected=base64.b64decode(payload[name]).decode();assert new==expected,'source changed unexpectedly'
p.write_text(new)
for cmd,label in [(['cmake','--build',str(build),'--target','zk_committed_opening_tests','zk_committed_opening_verify','--parallel','1'],'build-polished'),([str(build/'src/zk_committed_opening_tests')],'unit-tests-final')]:
    with (log/(label+'.log')).open('x') as f:result=subprocess.run(['/usr/bin/time','-v']+cmd,cwd=r,stdout=f,stderr=subprocess.STDOUT)
    with (log/'commands.jsonl').open('a') as f:f.write(json.dumps({'argv':cmd,'exit_code':result.returncode,'log':label+'.log','time_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()})+'\n')
    print(label,result.returncode,(log/(label+'.log')).read_text()[-1600:]);assert result.returncode==0
with (log/'progress.md').open('a') as f:f.write('\nBuild cleanup: two new-source misleading-indentation warnings removed, no logic change. Incremental build and final unit run exit0; preexisting MCL dependency warning left unchanged.\n')
