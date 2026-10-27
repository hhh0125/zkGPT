"""Trusted test-only fixture setup. Secret file is under build/, never reports/."""
from pathlib import Path
import os,sys,subprocess,json,hashlib
from r7_sync_safety import no_links
root=Path.cwd().resolve();stage=Path(sys.argv[1]);assert stage.parent==root/'reports' and stage.name.startswith(('R7-HYRAX-A3-STAGE2-RUN-','R7-STAGE3A-RUN-','R7-STAGE3B1-RUN-'))
private=root/'build/r7-stage2-private'/stage.name/'fixture.private';no_links(root,private);private.parent.mkdir(parents=True,exist_ok=True,mode=0o700);os.chmod(private.parent,0o700)
paths=[private,stage/'registry.bin',stage/'alternate-registry.bin',stage/'bad-tail-registry.bin'];fds=[]
try:
 for i,path in enumerate(paths):
  no_links(root,path);fds.append(os.open(path,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600 if i==0 else 0o644))
 r=subprocess.run([str(root/'build/release/src/paper_r7/r7_s2_fixture'),*[str(fd) for fd in fds]],pass_fds=tuple(fds),capture_output=True,text=True,timeout=160)
 print(r.stdout,end='');print(r.stderr,end='',file=sys.stderr)
 if r.returncode:raise SystemExit(r.returncode)
 info=json.loads(r.stdout.strip());assert info['registry_digest']==hashlib.sha256((stage/'registry.bin').read_bytes()).hexdigest()
 info['private_fixture_location']='build/r7-stage2-private/<this-run>/fixture.private; excluded from export'
 (stage/'FIXTURE.json').write_text(json.dumps(info,indent=2))
finally:
 for fd in fds:os.close(fd)
