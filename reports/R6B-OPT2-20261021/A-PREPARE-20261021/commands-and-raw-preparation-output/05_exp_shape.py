from pathlib import Path
import re,hashlib,struct
for name in ['src/exp_table_values.inc','src/paper_r1/exp_table.inc']:
 s=Path(name).read_text();s=re.sub(r'//[^\n]*','',s)
 if '{' in s:s=s.split('{',1)[1].split('}',1)[0]
 v=list(map(int,re.findall(r'\d+',s)));print(name,len(v),v[-10:]);print('raw',hashlib.sha256(b''.join(struct.pack('<I',n) for n in v)).hexdigest(),'clamp',hashlib.sha256(b''.join(struct.pack('<I',max(1,n)) for n in v)).hexdigest())
print(Path('src/exp_lookup.cpp').read_text()[:2700])
