"""Independent SHA256/canonical LE rejection reference; no C++ imports."""
import hashlib,struct,sys
from pathlib import Path
q=16798108731015832284940804142231733909759579603404752749028378864165570215949
def event(tag,seq,label,payload):
 label=label.encode();return tag.encode()+struct.pack('<QQ',seq,len(label))+label+struct.pack('<Q',len(payload))+payload
def reference():
 state=hashlib.sha256(b'zkGPT.R7.FS.canonical.SHA256.v1')
 for t,s,l,p in [('I',0,'header',bytes([1,2,3])),('I',1,'public-instance',bytes([4,5])),('P',2,'m',bytes([7,8]))]:state.update(event(t,s,l,p))
 for counter in range(4096):
  seed=state.digest()+struct.pack('<QQ',0,counter)
  c=hashlib.sha256(b'SHA256-REJECT-LE32-NONZERO-v1'+event('H',3,'c',seed)).digest()
  if 0<int.from_bytes(c,'little')<q:break
 else:raise AssertionError('rejection budget')
 challenge=event('C',3,'c',c);state.update(challenge);audit=hashlib.sha256(b'zkGPT.R7.FS.challenge-audit.v1'+challenge)
 return ' '.join([state.hexdigest(),audit.hexdigest(),hashlib.sha256(c).hexdigest(),str(counter)])
if __name__=='__main__':
 expected=reference();actual=next(l.removeprefix('REFERENCE ') for l in Path(sys.argv[1]).read_text().splitlines() if l.startswith('REFERENCE '));assert actual==expected,(actual,expected)
 print('PASS independent Python/C++ SHA256 transcript, exact nonzero Fr rejection and challenge audit',expected)
