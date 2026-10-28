from pathlib import Path
import struct,json,hashlib,collections,resource
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd()==r
out=r/'reports/R7-STAGE1-TASK0A-20261022-v1/research'
resource.setrlimit(resource.RLIMIT_CPU,(30,30));resource.setrlimit(resource.RLIMIT_AS,(512*1024**2,512*1024**2))
q=16798108731015832284940804142231733909759579603404752749028378864165570215949
class Reader:
 def __init__(self,b,width=8):self.b=b;self.i=0;self.width=width;self.events=[];self.fr=0;self.g1=0
 def raw(self,n):
  assert 0<=n<=len(self.b)-self.i;(a,self.i)=(self.i,self.i+n);return self.b[a:self.i]
 def u(self):return int.from_bytes(self.raw(self.width),'little')
 def text(self):return self.raw(self.u()).decode()
 def blob(self):return self.raw(self.u())
 def f(self,n=1):
  for _ in range(n):assert int.from_bytes(self.raw(32),'little')<q
  self.fr+=n
 def g(self,n=1):self.raw(32*n);self.g1+=n
 def event(self,path,start,**kw):self.events.append({'path':path,'offset':start,'bytes':self.i-start,**kw})
 def done(self):assert self.i==len(self.b),(self.i,len(self.b))
def opening(x,path,old=False):
 at=x.i
 if not old:x.f()
 c=x.u();x.g(2*c);x.f();x.event(path,at,kind='opening_old' if old else 'opening',ipa_rounds=c)
def sc(x,path,expecteddegree=None,expectedrounds=None):
 at=x.i;d=x.u();widths=[]
 for i in range(d):
  k=x.u();widths.append(k);assert k in [2,3,4]
  if expecteddegree is not None:assert k==expecteddegree+1
  x.f(k)
 if expectedrounds is not None:assert d==expectedrounds
 x.event(path,at,kind='sumcheck',rounds=d,widths=widths)
def pc(x,path):
 at=x.i;role=x.text();actual,padded,rows,cols,layout=(x.u() for _ in range(5));count=x.u()
 assert padded>0 and padded&(padded-1)==0 and rows*cols==padded and count==rows and actual<=padded
 x.g(count);x.event(path,at,kind='commitment',role=role,actual=actual,padded=padded,rows=rows,cols=cols,layout=layout)
def tree(x,path):
 at=x.i;d=x.u()
 for i in range(d):sc(x,path+'/'+str(i),3,i);x.f(2)
 x.event(path,at,kind='product_tree',depth=d,endpoint_scalars=2*d)
def memory(x,path):
 pc(x,path+'/read');pc(x,path+'/final');at=x.i;x.f(3);x.event(path+'/roots',at,kind='roots',count=3)
 for k in range(3):
  tree(x,path+'/tree/'+str(k));n=x.u();assert n==(1 if k==2 else 3)
  for j in range(n):opening(x,path+'/leaf/'+str(k)+'/'+str(j))
def ran(x,path):
 H=x.u();assert 1<=H<=14
 for h in range(H):pc(x,path+'/digitPC/'+str(h))
 opening(x,path+'/query');assert x.u()==H
 for h in range(H):opening(x,path+'/digitOpen/'+str(h))
 assert x.u()==H
 for h in range(H):memory(x,path+'/memory/'+str(h))
 sc(x,path+'/tailSC',2);opening(x,path+'/tailOpen')
def parseGKR(b):
 x=Reader(b,4);assert x.raw(4)==b'ZKGP' and x.u()==3;x.f();L=x.u();layers=[]
 for _ in range(L):
  at=x.i;layer=x.u();ty=x.u();ns=[]
  for role in ['phase1','phase2','matrix']+(['matrix_v'] if ty==11 else []):
   n=x.u();x.f(3*n);ns.append(n)
  x.f(4+(2 if ty==11 else 0));row={'layer':layer,'type':ty,'rounds':ns};layers.append(row);x.event('gkr/layer/'+str(layer),at,kind='gkr_layer',**row)
 n=x.u();x.f(3*n);x.f(2);c=x.u();x.g(2*c);x.f(2);x.done()
 QN=sum(sum(v['rounds'][:2]) for v in layers if v['type']!=11);QF=sum(sum(v['rounds'][2:]) for v in layers if v['type']==11)
 F=sum(v['type']==11 for v in layers);N=L-F
 assert len(b)==180+148*N+216*F+96*(QN+QF+n)+64*c
 return {'bytes':len(b),'layers':layers,'normal_quadratic_rounds':QN,'matrix_quadratic_rounds':QF,'internal_aggregation_rounds':n,'legacy_ipa_rounds':c,'Fr_fields':x.fr,'G1_fields':x.g1,'all_bytes_consumed':True,'events':x.events}
def parseLookup(b):
 x=Reader(b)
 # R1 header: text magic, u64 version, u64 kind, then digest32.
 magic=x.text();ver=x.u();kind=x.u();assert magic=='ZKGPTR1' and ver==1 and kind==13,(magic,ver,kind)
 x.raw(32);nr=x.u();assert nr==13
 for j in range(nr):ran(x,'range/'+str(j))
 ne=x.u();assert ne==1
 for j in range(ne):
  prefix='exp/'+str(j)
  for k in range(4):pc(x,prefix+'/helper/'+str(k))
  for k in range(4):ran(x,prefix+'/range/'+str(k))
  for k in range(2):
   sc(x,prefix+'/branchSC/'+str(k),3)
   for h in range(3):opening(x,prefix+'/branchOpen/'+str(k)+'/'+str(h))
  for k in range(2):opening(x,prefix+'/valueBinding/'+str(k))
  memory(x,prefix+'/memory')
 nc=x.u();assert nc==16
 for k in range(nc):opening(x,'mapped/child/'+str(k))
 sc(x,'mapped/SC',2);opening(x,'mapped/master');x.done()
 C=collections.Counter(v['kind'] for v in x.events)
 S=collections.Counter()
 for v in x.events:
  if v['kind']=='sumcheck':
   for w in v['widths']:S[w-1]+=1
 return {'bytes':len(b),'Fr_fields':x.fr,'G1_fields':x.g1,'kinds':dict(C),'sumcheck_rounds_by_degree':dict(S),'ipa_rounds':sum(v['ipa_rounds'] for v in x.events if v['kind']=='opening'),'all_bytes_consumed':True,'events':x.events}
outputs={}
for tag,rel in [('ONEBLOCK','reports/R6-FULL12-P0AB-20261021/r6-oneblock-smoke/model.proof'),('FULL12','reports/R6-FULL12-P0AB-20261021-v1-full12/model.proof')]:
 b=(r/rel).read_bytes();x=Reader(b);assert x.text()=='ZKGPTR2' and x.u()==3 and x.text()=='proof';x.raw(32);g=parseGKR(x.blob());opening(x,'bridge/new');opening(x,'bridge/old',True);look=parseLookup(x.blob());x.done();c=g['legacy_ipa_rounds'];assert len(b)==196+g['bytes']+128*c+look['bytes']
 z={'source_artifact':rel,'sha256':hashlib.sha256(b).hexdigest(),'bytes':len(b),'GKR':g,'lookup':look,'outer_events':x.events,'outer_Fr':x.fr,'outer_G1':x.g1,'all_bytes_consumed':True,'cryptographic_verification_executed':False}
 assert g['normal_quadratic_rounds']==(543 if tag=='ONEBLOCK' else 6516)
 assert look['kinds']['opening']==890
 assert look['kinds']['commitment']==315 and look['kinds']['roots']==104 and look['kinds']['product_tree']==312
 outputs[tag]=z
one={v['layer']:v for v in outputs['ONEBLOCK']['GKR']['layers']}
for v in outputs['FULL12']['GKR']['layers']:
 k=(v['layer']-1)%23+1;assert v['type']==one[k]['type'] and v['rounds']==one[k]['rounds']
report={'scope':'read-only structural parser of existing R6 artifacts; no Prover, Verifier, witness or MSM','Fr_canonical_checked':True,'G1_bytes_only_no_group_math':True,'per_block_round_shapes_equal':True,'artifacts':outputs}
(out/'EXISTING_PROOF_MESSAGE_INVENTORY.json').write_text(json.dumps(report,indent=2)+'\n')
for tag,z in outputs.items():print(tag,{k:z[k] for k in ['bytes','sha256']},'GKR',z['GKR']['bytes'],'lookup',z['lookup']['bytes'],'normal_rounds',z['GKR']['normal_quadratic_rounds'],'lookupFr',z['lookup']['Fr_fields'],'lookupG1',z['lookup']['G1_fields'],'SC',z['lookup']['sumcheck_rounds_by_degree'])
