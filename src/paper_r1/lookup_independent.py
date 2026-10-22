import subprocess,pathlib,tempfile,sys,json
with tempfile.TemporaryDirectory(prefix='r2-lookup-') as temp:
 root=pathlib.Path(temp)
 p=subprocess.run([sys.argv[1],temp],capture_output=True,text=True)
 if p.returncode:raise RuntimeError(p.stdout+p.stderr)
 records=[]
 for statement in sorted(root.glob('*.statement')):
  name=statement.stem;proof=root/(name+'.proof')
  p=subprocess.run([sys.argv[2],str(statement),str(proof)],capture_output=True,text=True)
  expected=0 if name=='valid' else 2
  records.append({'name':name,'expected':expected,'actual':p.returncode,'output':p.stdout+p.stderr})
  print(json.dumps(records[-1]))
  if p.returncode!=expected:raise RuntimeError('unexpected verdict: '+name)
 if len(records)<14:raise RuntimeError('missing independent cases')
 print('PASS independent lookup',len(records),'cases')
