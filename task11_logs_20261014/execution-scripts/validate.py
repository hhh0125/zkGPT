from pathlib import Path
import subprocess,json,datetime,re,hashlib,platform
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT');log=r/'task11_logs_20261014';build=log/'build'
tests=build/'src/zk_committed_opening_tests';verifier=build/'src/zk_committed_opening_verify';fixtures=log/'fixtures'
def run(cmd,name,expected=0):
    with (log/(name+'.log')).open('x') as f:p=subprocess.run(['/usr/bin/time','-v']+cmd,cwd=r,stdout=f,stderr=subprocess.STDOUT)
    with (log/'commands.jsonl').open('a') as f:f.write(json.dumps({'time_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'argv':cmd,'exit_code':p.returncode,'expected_exit_code':expected,'log':name+'.log'})+'\n')
    text=(log/(name+'.log')).read_text();rss=int(re.search(r'Maximum resident set size \(kbytes\): (\d+)',text)[1])
    assert p.returncode==expected,(name,p.returncode,text)
    return text,rss,p.returncode
run([str(tests),'--emit',str(fixtures)],'fixture-emission')
rows=[];manifest={}
decode={'wrong-magic','wrong-version','wrong-transcript-domain','truncated','trailing','invalid-G1','noncanonical-Fr','oversized-n','oversized-message','wrong-vector-domain','wrong-scalar-domain'}
for s in sorted(fixtures.glob('*.statement.bin')):
    name=s.name.removesuffix('.statement.bin');p=fixtures/(name+'.proof.bin');expected=0 if name.startswith('positive-') else (2 if name in decode else 1)
    out,rss,exitcode=run([str(verifier),str(s),str(p)],'independent-'+name,expected)
    rows.append({'sample':name,'expected_exit':expected,'actual_exit':exitcode,'classification':('ACCEPT' if exitcode==0 else 'CRYPTO_REJECT' if exitcode==1 else 'DECODE_REJECT'),'peak_rss_kib':rss,'diagnostic':out.splitlines()[0]})
    for file in [s,p]:manifest[str(file.relative_to(r))]={'sha256':hashlib.sha256(file.read_bytes()).hexdigest(),'bytes':file.stat().st_size}
out,rss,ec=run([str(verifier),str(fixtures/'positive-n4.statement.bin'),str(fixtures/'positive-n4.proof.bin'),'task11/session-b/layout-v1'],'independent-expected-context',2)
rows.append({'sample':'wrong-expected-context','expected_exit':2,'actual_exit':ec,'classification':'DECODE_REJECT','peak_rss_kib':rss,'diagnostic':out.splitlines()[0]})
with (log/'independent-results.json').open('x') as f:json.dump({'samples':rows,'matched':len(rows),'total':len(rows),'verifier':'dedicated public-only executable; no witness file argument','exit_policy':{'0':'accept','1':'cryptographic rejection','2':'decode/format/expected-context rejection'}},f,indent=2)
with (log/'public-artifact-manifest.json').open('x') as f:json.dump(manifest,f,indent=2)
out,rss,ec=run([str(tests),'--benchmark'],'benchmark')
bench=[]
for line in out.splitlines():
    if not line.startswith('BENCH protocol='):continue
    vals=dict(x.split('=',1) for x in line.split()[1:]);bench.append({k:(v if k=='protocol' else float(v) if 'mean' in k else int(v)) for k,v in vals.items()})
info={'samples':bench,'same_conditions':True,'rounds_per_protocol_dimension':100,'peak_rss_kib':rss,'rss_scope':'combined process total high-water mark, not per-proof incremental memory',
      'timing_scope':'steady_clock prove/verify calls include full pp validation/local checks/OS sampling/FS/group equations; exclude fixture setup, codec, startup; alternating protocol execution order; no external system load isolation',
      'build':'Release -O3 -DNDEBUG; C++17; ZKGPT_NATIVE=OFF; single-thread build; no sanitizers',
      'platform':platform.platform(),'cpu':next((l.split(':',1)[1].strip() for l in Path('/proc/cpuinfo').read_text().splitlines() if l.startswith('model name')),'unknown'),
      'compiler':subprocess.check_output(['c++','--version'],text=True).splitlines()[0],
      'historical_task10_benchmark_comparison':'not used; public-y newly rerun in same binary and environment'}
with (log/'benchmark-results.json').open('x') as f:json.dump(info,f,indent=2)
# Scoped CTest suite only: legacy full GPT tests exceed requested task/resource scope.
out,rss,ec=run(['ctest','--test-dir',str(build),'-R','^zk_(committed_opening_tests|opening_prototype_tests)$','--output-on-failure'],'ctest-primitive-suite')
with (log/'progress.md').open('a') as f:f.write('\nStage4: public-only fixture emission, '+str(len(rows))+'/'+str(len(rows))+' independent process expected exits; same-condition public-y/committed-y100-round benchmarks and both primitive CTest suites exit0.\nNo production/full GPT tests executed. Review and baseline integrity/report next.\n')
print(json.dumps({'independent':len(rows),'benchmark':info,'ctest':out.split('Command being timed')[0]},indent=2))
