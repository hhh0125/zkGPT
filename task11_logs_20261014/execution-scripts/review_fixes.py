from pathlib import Path
import json,base64,subprocess,datetime
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT');log=r/'task11_logs_20261014';build=log/'build'
payload=json.loads('@@PAYLOAD@@')
name='src/main_zk_committed_opening_tests.cpp';p=r/name;new=base64.b64decode(payload[name]).decode();old=p.read_text()
start=new.index('    test("statement_truncated_every_offset"');end=new.index('    test("statement_noncanonical_Fr"',start)
assert new[:start]+new[end:]==old,'unexpected source drift'
p.write_text(new)
p=log/'design-review.md';old=p.read_text();needle='programs earlier counters with invalid/zero strings'
assert needle in old
p.write_text(old.replace(needle,'programs each earlier counter with an independently sampled 32-byte output uniformly conditioned on rejection (noncanonical Fr encoding or zero)')+
 '\nFinal review: no Critical/Important static finding. Minor rejected-output distribution clarified here; added full Statement truncation and unique header/length regression coverage. No protocol/API behavior changed. Finite-cap simulation includes negligible all-rejected abort; conditioned successful simulation differs only by that abort bound.\n')
with (log/'final_review.md').open('xb') as f:f.write(base64.b64decode(payload['task11_logs_20261014/final_review.md']))
for cmd,label in [(['cmake','--build',str(build),'--target','zk_committed_opening_tests','--parallel','1'],'build-review-tests'),([str(build/'src/zk_committed_opening_tests')],'unit-tests-reviewed'),(['ctest','--test-dir',str(build),'-R','^zk_(committed_opening_tests|opening_prototype_tests)$','--output-on-failure'],'ctest-reviewed')]:
    with (log/(label+'.log')).open('x') as f:p=subprocess.run(['/usr/bin/time','-v']+cmd,cwd=r,stdout=f,stderr=subprocess.STDOUT)
    with (log/'commands.jsonl').open('a') as f:f.write(json.dumps({'argv':cmd,'exit_code':p.returncode,'log':label+'.log','time_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()})+'\n')
    out=(log/(label+'.log')).read_text();print(label,p.returncode,out[-1500:]);assert p.returncode==0
with (log/'progress.md').open('a') as f:f.write('\nStage6 review: fresh static reviewer found no Critical/Important issue. Minor simulator rejection-output distribution clarified and Statement-wire test coverage added; final59/59 units and both primitive CTests pass. No protocol code changed after benchmark/independent verification.\n')
