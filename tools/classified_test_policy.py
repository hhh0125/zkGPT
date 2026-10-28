"""Test status reflects actual completed cases, never only command-line flags."""
FULL_ATTACKS={'legacy-v1','wrong-X','wrong-Y','wrong-internal','wrong-weight','wrong-model-witness','model-response','truncated','cross-session','wrong-mapping','wrong-commitment','wrong-lookup','missing-authentication'}
def validate_options(options):
 if '--positive-only' in options and '--negative-only' in options:raise ValueError('conflicting positive/negative filters')
 if any(x in options for x in ['--positive-only','--negative-only']) and '--full' not in options:raise ValueError('full-case filter requires --full')
 for option in options:
  if option.startswith('--attacks=') and (not set(option.split('=',1)[1].split(','))<=FULL_ATTACKS or '--negative-only' not in options):raise ValueError('unknown attacks or missing negative-only scope')
def selected_attacks(options):
 validate_options(options)
 return next((set(x.split('=',1)[1].split(',')) for x in options if x.startswith('--attacks=')),FULL_ATTACKS)
def completed_status(options,cases):
 validate_options(options)
 if not cases or not all(c['passed'] for c in cases):raise ValueError('missing or failed live cases')
 ids=[c['case'] for c in cases]
 if len(set(ids))!=len(ids):raise ValueError('duplicate live case')
 if '--full' not in options:return 'CLASSIFIED_LOCAL_CRYPTO_TESTED'
 expected=({'correct-full'} if '--negative-only' not in options else set())|(selected_attacks(options) if '--positive-only' not in options else set())
 if set(ids)!=expected:raise ValueError('uncompleted or unexpected full cases')
 return 'CLASSIFIED_ONEBLOCK_POSITIVE_TESTED' if '--positive-only' in options else 'CLASSIFIED_ONEBLOCK_NEGATIVES_TESTED' if '--negative-only' in options else 'CLASSIFIED_ONEBLOCK_INTERACTIVE_TESTED'

def retain_stream(raw,target):
 import hashlib
 target.mkdir();h=hashlib.sha256();total=count=0
 with raw.open('rb') as f:
  while b:=f.read(4<<20):
   assert (target/(str(count).zfill(5)+'.part')).write_bytes(b)==len(b)
   h.update(b);total+=len(b);count+=1
 assert total==raw.stat().st_size
 raw.unlink()
 return dict(bytes=total,parts=count,sha256=h.hexdigest())

def finish_prover(child,verifier_exit,negative_case):
 import subprocess
 if negative_case and verifier_exit==2 and child.poll() is None:
  child.terminate()
  try:return child.wait(timeout=5),True
  except subprocess.TimeoutExpired:child.kill();return child.wait(),True
 return child.wait(timeout=30),False
