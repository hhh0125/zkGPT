"""Generate a native Git patch without Windows newline double-translation."""
from pathlib import Path
import tarfile,subprocess,json,base64,hashlib,difflib
R=Path(__file__).resolve().parents[1];S=R/'reports/R7-PUBLIC-BUILD-20261113-v1';before=S/'patch-before';parts=[];changes=[]
with tarfile.open(S/'baseline-source.tar.gz') as t:
 for m in t.getmembers():
  old=t.extractfile(m).read();p=R/m.name
  if p.read_bytes()==old:continue
  # Keep the baseline newline convention for touched source files.
  if old.count(b'\r\n')==old.count(b'\n') and b'\r\n' in old:
   p.write_bytes(p.read_bytes().replace(b'\r\n',b'\n').replace(b'\n',b'\r\n'))
  changes.append(m.name);original=before/m.name;original.parent.mkdir(parents=True,exist_ok=True);original.write_bytes(old)
  a=original.relative_to(R).as_posix();cmd=['git','diff','--no-index','--binary','--no-color','--src-prefix=a/','--dst-prefix=b/','--',a,m.name];result=subprocess.run(cmd,cwd=R,capture_output=True);assert result.returncode==1,(m.name,result.stderr)
  parts.append(result.stdout.replace(('a/'+before.relative_to(R).as_posix()+'/').encode(),b'a/'))
name='src/paper_r7/public_build_tests.cpp';text=(R/name).read_text();assert text.endswith('\n');parts.append(''.join(difflib.unified_diff([],text.splitlines(True),fromfile='/dev/null',tofile='b/'+name)).encode())
(S/'source.patch').write_bytes(b''.join(parts));(S/'FINAL_CHANGED_SOURCE.json').write_text(json.dumps(changes+[name],indent=2))
# Update the queued final synchronization to preserve original line endings.
payload=json.loads((S/'final-source-payload.json').read_text());deployed=json.loads((S/'DEPLOYED_SHA256.json').read_text())
for n in changes:
 data=(R/n).read_bytes();h=hashlib.sha256(data).hexdigest()
 if h!=deployed[n] or n in payload:payload[n]={'before':deployed[n],'after':h,'data':base64.b64encode(data).decode()}
(S/'final-source-payload.json').write_text(json.dumps(payload))
cmd=['git','-c','safe.directory='+R.as_posix(),'apply','--check','--reverse',str(S/'source.patch')];p=subprocess.run(cmd,cwd=R,capture_output=True);(S/'PATCH_REVERSE_CHECK.log').write_bytes(p.stdout+p.stderr);assert p.returncode==0,p.stderr
print('Native patch reverse-check passed; source files',len(changes)+1,'queued synchronization files',len(payload))
