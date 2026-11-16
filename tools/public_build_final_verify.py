"""One final no-trace verifier replay, with process-local runner state."""
from pathlib import Path
import sys,importlib.util,json,hashlib
R=Path.cwd();S=R/'reports/R7-PUBLIC-BUILD-20261113-v1';scope=sys.argv[1];assert scope in ['oneblock','full12']
spec=importlib.util.spec_from_file_location('final_runtime_'+scope,R/'tools/product_leaves_r9_method.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
out=S/('final-'+scope+'-verifier');out.mkdir(exist_ok=False);m.S=out;m.BIN=R/'build/release/public-build-20261113-v1/src/paper_r7';m.LOG=S/'final-runtime-logs';m.AFFINITY=list(map(int,sys.argv[2].split(',')));m.ENV.update(ZKGPT_PERF5_PROFILE='1',ZKGPT_PERF6_A2='1',ZKGPT_PERF6_C2='1');m.ENV['ZKGPT_EVENT_TRACE_PATH']=str(out/'V-EVENTS.bin')
original=S/(scope+'-shared-build');m.run(out,'V',scope,original/'STATEMENT.bin',original/(scope+'.fsproof'),'R9')
assert m.sha(out/'V-EVENTS.bin')==m.sha(original/'P-EVENTS.bin')
coverage=json.loads((out/'V-COVERAGE.json').read_text());assert coverage['required_count']==coverage['verified_count']==len(coverage['required'])
(out/'VERIFIED.json').write_text(json.dumps({'success':True,'scope':scope,'canonical_events_equal_to_prover':True,'coverage':coverage['verified_count'],'cpu_affinity':m.AFFINITY},indent=2))
