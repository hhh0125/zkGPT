from pathlib import Path
import base64,json,subprocess,datetime
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT');log=r/'task11_logs_20261014'
payload=json.loads('@@PAYLOAD@@')
for name,b64 in payload.items():
    p=r/name;p.parent.mkdir(parents=True,exist_ok=True)
    data=base64.b64decode(b64)
    if p.exists():assert p.read_bytes()==data, 'existing file differs: '+name
    else:
        with p.open('xb') as f:f.write(data)
libs=list((r/'task10_logs_20261014/build').rglob('libmcl.so'))
bnlibs=list((r/'task10_logs_20261014/build').rglob('libmclbn256.so'))
assert len(libs)==1 and len(bnlibs)==1,(libs,bnlibs)
cmd=['c++','-std=c++17','-O3','-DNDEBUG','-I'+str(r/'src'),'-I'+str(r/'3rd/mcl/include'),
     str(r/'src/main_zk_committed_opening_tests.cpp'),str(log/'red_stubs.cpp'),
     str(r/'src/zk_hiding_commitment.cpp'),str(r/'src/zk_linear_opening.cpp'),
     str(bnlibs[0]),str(libs[0]),'-Wl,-rpath,'+str(libs[0].parent),'-lgmp','-lgmpxx','-pthread','-o',str(log/'red_tests')]
def run(cmd,name,expected):
    p=subprocess.run(cmd,cwd=r,capture_output=True,text=True)
    (log/(name+'.log')).write_text(p.stdout+p.stderr)
    with (log/'commands.jsonl').open('a') as f:f.write(json.dumps({'time_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'argv':cmd,'exit_code':p.returncode,'expected_exit_code':expected,'log':name+'.log'})+'\n')
    print(name,'exit',p.returncode,(p.stdout+p.stderr)[-4000:])
    assert p.returncode==expected
run(cmd,'red-build',0)
run([str(log/'red_tests'),'--red'],'red-tests',1)
with (log/'progress.md').open('a') as f:f.write('\nStage2: design/plan/headers/tests uploaded; RED-only stub build exit0, missing scalar/proof tests exit1 (expected).\nNext implement reviewed protocol and codecs; no stubs in final targets.\n')
