"""Independent processes: verifier receives only explicitly selected public statement and proof."""
import json,pathlib,subprocess,sys,contextlib,uuid,hashlib
prover,verifier=sys.argv[1:3]
run_dir=pathlib.Path(verifier).resolve().parent/'independent_runs'/uuid.uuid4().hex
run_dir.mkdir(parents=True)
with contextlib.nullcontext(str(run_dir)) as name:
 root=pathlib.Path(name);p=subprocess.run([prover,str(root)],capture_output=True,text=True);assert p.returncode==0,(p.stdout,p.stderr)
 rows=[]
 cases=[('valid',0),('other-model',0),('model-substitution',2),('query',2),('memory-root',2),('read-counter',2),('gkr',2),('final-L',2),('indexed-value',2),('transcript',2),('public-X',2),('public-Y',2),('model-commitment',2),('table',2)]
 for label,expected in cases:
  p=subprocess.run([verifier,str(root/(label+'.statement')),str(root/(label+'.proof'))],cwd=root,capture_output=True,text=True,timeout=30)
  rows.append({'case':label,'expected':expected,'exit':p.returncode,'stdout':p.stdout.strip(),'stderr':p.stderr.strip()})
 for label in ['trailing','truncated','noncanonical-Fr']:
  p=subprocess.run([verifier,str(root/'valid.statement'),str(root/(label+'.proof'))],cwd=root,capture_output=True,text=True,timeout=30)
  rows.append({'case':label,'expected':2,'exit':p.returncode,'stdout':p.stdout.strip(),'stderr':p.stderr.strip()})
 raw=bytearray((root/'valid.proof').read_bytes());at=31+8
 size=int.from_bytes(raw[at:at+8],'little');at+=8+size+5*8+8
 raw[at:at+32]=b'\xff'*32;(root/'invalid-G1.proof').write_bytes(raw)
 p=subprocess.run([verifier,str(root/'valid.statement'),str(root/'invalid-G1.proof')],cwd=root,capture_output=True,text=True,timeout=30)
 rows.append({'case':'invalid-G1','expected':2,'exit':p.returncode,'stdout':p.stdout.strip(),'stderr':p.stderr.strip()})
 p=subprocess.run([verifier,str(root/'oversized.statement'),str(root/'valid.proof')],cwd=root,capture_output=True,text=True,timeout=30)
 rows.append({'case':'oversized','expected':2,'exit':p.returncode,'stdout':p.stdout.strip(),'stderr':p.stderr.strip()})
 print(json.dumps(rows,indent=2));assert all(r['exit']==r['expected'] for r in rows)
 assert 'wire count limit' in rows[-1]['stderr'],'oversized fixture must exercise count bound'
 for label,expected in [('exp-valid',0),('exp-branch',2),('exp-memory',2)]:
  p=subprocess.run([verifier,'--exp',str(root/'exp-valid.statement'),str(root/(label+'.proof'))],cwd=root,capture_output=True,text=True,timeout=30)
  row={'case':label,'expected':expected,'exit':p.returncode,'stdout':p.stdout.strip(),'stderr':p.stderr.strip()};rows.append(row)
  print(json.dumps(row));assert p.returncode==expected
 for flag,prefix in [('--division','div'),('--positive-sqrt','sqrt')]:
  for suffix in ['valid','relation','positivity','source','trailing' if prefix=='div' else 'truncated']:
   statement=root/(prefix+('-source' if suffix=='source' else '-valid')+'.statement')
   proof=root/(prefix+('-valid' if suffix=='source' else '-'+suffix)+'.proof')
   expected=0 if suffix=='valid' else 2
   p=subprocess.run([verifier,flag,str(statement),str(proof)],cwd=root,capture_output=True,text=True,timeout=30)
   row={'case':prefix+'-'+suffix,'expected':expected,'exit':p.returncode,'stdout':p.stdout.strip(),'stderr':p.stderr.strip()};rows.append(row)
   print(json.dumps(row));assert p.returncode==expected
 # The verifier executable must contain no prover entry points.
 nm=subprocess.run(['nm','-C',verifier],capture_output=True,text=True,check=True)
 forbidden=['proveBundle(', 'proveGkr(', 'proveInput(', 'proveMemory(', 'proveRange(', 'proveProduct(', 'proveSumcheck(', 'proveExp(', 'proveRounding(', 'proveMaximum(', 'proveMatrix(', 'proveDivision(', 'provePositiveSqrt(', 'zkgpt::r1::open(']
 assert not any(x in nm.stdout for x in forbidden),'prover symbol in standalone verifier'
 (root/'results.json').write_text(json.dumps(rows,indent=2))
 manifest={p.name:{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in root.iterdir() if p.is_file()}
 (root/'manifest.json').write_text(json.dumps(manifest,indent=2))
 print('Artifacts retained:',root)
 print('PASS independent',len(rows),'cases; no prover entry points; no witness/model files in verifier working directory')
