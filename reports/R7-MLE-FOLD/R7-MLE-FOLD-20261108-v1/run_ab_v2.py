from pathlib import Path
import importlib.util,sys,os,json,hashlib,shutil,time
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');S=ROOT/'reports/R7-MLE-FOLD-20261108-v1'
spec=importlib.util.spec_from_file_location('validated',S/'baseline_validation_driver.py');v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
scope,variant,index=sys.argv[1:4];assert scope in ['oneblock','full12'] and variant in ['A','B','C','D'] and index in ['1','2','3','4']
base=S/'ab-v2'/(scope+'-'+variant+index);base.mkdir(parents=True,exist_ok=False)
v.BASE=base;v.BIN=S/'bin-B2';v.BUILD=ROOT/'build/release/r7-mle-fold-20261108-v1'
v.ENV.update(ZKGPT_CATEGORY_U_FOLD='0' if variant in ['A','D'] else '1',ZKGPT_SOURCE_MLE_STREAM='1' if variant=='C' else '0',ZKGPT_CATEGORY_U_RAM_BYTES=str(512<<20),ZKGPT_K_COMPACT='0' if variant in ['A','D'] else '1',ZKGPT_U_PROFILE_READ='1' if variant=='D' else '0',ZKGPT_CATEGORY_K_ENDPOINT='0' if variant in ['A','D'] else '1')
def identity():
 i=json.loads((S/'BASELINE_IDENTITY.json').read_text())
 for section in ['parameters','dependencies']:
  assert all(v.sha(ROOT/p)==h for p,h in i[section].items()),section
 frozen=json.loads((S/'FROZEN_BUILD_V2.json').read_text())
 assert all(v.sha(ROOT/p)==h for p,h in frozen['source'].items()),'frozen source changed'
 assert all(v.sha(S/'bin-B2'/p)==h for p,h in frozen['binaries'].items()),'frozen binary changed'
 return i
v.identity=identity
try:
 v.execute(scope)
 result=base/(scope+'-v1/RESULT.json');d=json.loads(result.read_text());d['optimization_variant']=variant;d['repetition']=int(index);d['common_configuration']='PERF6-4threads-fixed-affinity-K512MiB-file4GiB-v1';result.write_text(json.dumps(d,indent=2))
 before=ROOT/'reports/R7-CATEGORY-X-20261106-v1'/(scope+'-v1')
 oldcov=json.loads((before/'V-COVERAGE.json').read_text());newcov=json.loads((result.parent/'V-COVERAGE.json').read_text())
 assert {x['id']:x['family'] for x in oldcov['required']}=={x['id']:x['family'] for x in newcov['required']},'relation set changed'
 oldlayout=json.loads((before/'V-LAYOUT.json').read_text())
 for key in ['suite','layout','model_scope','source_layout_identity','rows','columns','domain','logical_values','physical_values','physical_domain','source_segments','native_public_cells','category_alignment_gap','root_padding']:
  assert d['layout'][key]==oldlayout[key],('layout/map changed',key)
 for tag in ['U_SCAN_DETAIL','OPT3D_U_ROUND','PERF4_U_STATE','SOURCE_MLE','WITNESS_STORAGE','CATEGORY_TILE_DETAIL','CATEGORY_K_ENDPOINT','OPT3A_AUTH','OPT2_AUTH','PERF2_K_TOTAL','PERF3_MAP']:
  d[tag]={role:v.tagged(result.parent/(role+'.log'),tag) for role in ['P','V']}
 d['strict_postchecks_passed']=True;result.write_text(json.dumps(d,indent=2))
 print('AB_RUN_COMPLETE',scope,variant,index,flush=True)
except BaseException as e:
 import traceback
 (base/'FAILURE.json').write_text(json.dumps({'error':str(e),'traceback':traceback.format_exc()},indent=2));raise
