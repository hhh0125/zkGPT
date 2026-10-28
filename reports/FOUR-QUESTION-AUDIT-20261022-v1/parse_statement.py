"""Independent structural/data parser only; performs NO cryptographic verification."""
from pathlib import Path
import struct,hashlib,json,sys
P=16798108731015832284940804142231733909759579603404752749028378864165570215949
class Reader:
 def __init__(self,b): self.b=b;self.at=0
 def raw(self,n):
  assert 0<=n<=len(self.b)-self.at
  b=self.b[self.at:self.at+n];self.at+=n;return b
 def u(self): return int.from_bytes(self.raw(8),'little')
 def blob(self): return self.raw(self.u())
 def text(self): return self.blob().decode()
 def fr(self):
  x=int.from_bytes(self.raw(32),'little');assert x<P;return x
 def pc(self):
  s={'role':self.text(),'actual':self.u(),'padded':self.u(),'rows':self.u(),'columns':self.u(),'layout':self.u()}
  assert s['rows']*s['columns']==s['padded'];assert self.u()==s['rows']
  s['rows_sha256']=hashlib.sha256(self.raw(32*s['rows'])).hexdigest();return s
 def done(self): assert self.at==len(self.b),(self.at,len(self.b))
def parse(path):
 r=Reader(Path(path).read_bytes());assert r.text()=='ZKGPTR2' and r.u()==3 and r.text()=='statement'
 assert r.u()==2
 shape=[r.u() for _ in range(6)];log=r.u();domain=r.text();columns=r.u();fingerprint=r.raw(32).hex();output_eval=r.fr();context=r.raw(32).hex()
 rows=r.u();r.raw(rows*32);q=Reader(r.blob());r.done()
 assert q.text()=='ZKGPTR1' and q.u()==1 and q.u()==12
 lookup_context=q.text();master=q.pc();children=[]
 for _ in range(q.u()):
  c=q.pc();c['spans']=[{'offset':q.u(),'count':q.u(),'bias':str(q.fr())} for _ in range(q.u())];children.append(c)
 public=[(q.u(),q.fr()) for _ in range(q.u())]
 ranges=[{'child':q.u(),'id':q.text(),'bits':q.u()} for _ in range(q.u())]
 exps=[{'index_child':q.u(),'value_child':q.u(),'id':q.text(),'profile':q.u()} for _ in range(q.u())];q.done()
 x=public[:32768];y=public[32768:65536]
 assert [a for a,v in x]==list(range(32768))
 assert [a for a,v in y]==list(range(y[0][0],y[0][0]+32768))
 signed=lambda v:v if v<P//2 else v-P
 xb=b''.join(struct.pack('<q',signed(v)) for a,v in x);yb=b''.join(struct.pack('<q',signed(v)) for a,v in y)
 return {'model_shape':shape,'master_log':log,'domain':domain,'columns':columns,'fingerprint':fingerprint,'output_evaluation':str(output_eval),'integration_context':context,'lookup_context':lookup_context,'master':master,'children':children,'ranges':ranges,'exps':exps,'public_count':len(public),'X_count':len(x),'Y_count':len(y),'Y_start':y[0][0],'constants_count':len(public)-65536,'X_sha256':hashlib.sha256(xb).hexdigest(),'Y_sha256':hashlib.sha256(yb).hexdigest(),'X_padding_zero':all(signed(x[i][1])==0 for i in range(32768) if i%1024>=768),'Y_padding_zero':all(signed(y[i][1])==0 for i in range(32768) if i%1024>=768),'full_wire_consumed':True},xb,yb
if __name__=='__main__':
 path,out=map(Path,sys.argv[1:]);out.mkdir(exist_ok=True,parents=True)
 meta,x,y=parse(path)
 (out/'STATEMENT_PARSED.json').write_text(json.dumps(meta,indent=2)+'\n')
 (out/'parsed_X.bin').write_bytes(x);(out/'parsed_Y.bin').write_bytes(y)
 archived=path.parent
 assert x==(archived/'FULL12_PUBLIC_X_SIGNED64_LE.bin').read_bytes()
 assert y==(archived/'FULL12_FINAL_BLOCK_Y_SIGNED64_LE.bin').read_bytes()
 j=json.loads((archived/'FULL12_FINAL_BLOCK_Y.json').read_text())
 vals=struct.unpack('<32768q',y)
 assert j['values_active']==[list(vals[i*1024:i*1024+768]) for i in range(32)]
 print(json.dumps({k:v for k,v in meta.items() if k not in ['children','master']},indent=2))
