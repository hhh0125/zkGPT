"""Synthetic public metadata only; no witness, commitment or proof generation."""
from pathlib import Path
import struct,json,hashlib
HERE=Path(__file__).resolve().parent;data=HERE/'test-data';P=16798108731015832284940804142231733909759579603404752749028378864165570215949
def u(x):return struct.pack('<Q',x)
def text(s):b=s.encode();return u(len(b))+b
def fr(x):return (x%P).to_bytes(32,'little')
plan=json.loads((HERE.parent/'R6A/FULL12_FINAL_PLAN.json').read_text());anchor=(HERE.parent/'R6A/full12-operator-trust-v1.anchor').read_text().splitlines();X=json.loads((data/'public_X.json').read_text())
def pc(role,actual,padded,rows,columns,layout,points=None):return text(role)+b''.join(u(v) for v in [actual,padded,rows,columns,layout,rows])+(points if points is not None else bytes(rows*32))
def statement(kind='good'):
 context='R6-R5B-engineering-candidate-NOT-PAPER-APPROVED/'+anchor[5]+'/'+plan['circuit']
 if kind=='context':context=context.replace('r6-op-model/12','r6-op-model/1')
 lookup=text('ZKGPTR1')+u(1)+u(12)+text(context)+pc('r2/model/master',199348640,268435456,16384,16384,1)+u(len(plan['children']))
 for child in plan['children']:
  points=None
  if child['role']=='r2/model/W':
   points=b''.join(bytes.fromhex(x) for x in anchor[8:])
   if kind=='W':points=bytes(32)+points[32:]
  lookup+=pc(child['role'],child['actual'],child['padded'],child['rows'],child['columns'],0,points)+u(len(child['spans']))
  for s in child['spans']:lookup+=u(s['offset'])+u(s['count'])+bytes.fromhex(s['bias'])
 constants=set();zero=[]
 for r in plan['ranges']:
  n=r['name'];reserve=20 if n.startswith('round_') and n.endswith('_q') else 10 if ((n.startswith(('layer_norm_','gelu_')) and n.endswith('_y')) or (n.startswith('softmax_') and n.endswith('_sumE'))) else 0
  if reserve:at=r['offset']-reserve;constants.add(at);zero.extend(range(at+1,at+reserve))
 public=[]
 for at,v in enumerate(X):public.append((at,v+(1 if (kind=='X' and at==15329) or (kind=='padding' and at==768) else 0)))
 public += [(plan['connections'][-1]['output']+j,0) for j in range(32768)]
 public += [(j,1) for j in sorted(constants)]+[(j,0) for j in zero]
 lookup+=u(len(public))+b''.join(u(at)+fr(v) for at,v in public)
 groups=sorted(set((r['bits'],r['signed']) for r in plan['ranges']));lookup+=u(len(groups))
 for j,(bits,sgn) in enumerate(groups):lookup+=u(j)+text('r2/model/range/'+str(j))+u(bits)
 lookup+=u(1)+u(len(groups))+u(len(groups)+1)+text('r2/model/Exp')+u(1)
 shape=[32,12,12,64,768,3072]
 if kind=='shape':shape[1]=1
 out=text('ZKGPTR2')+u(3)+text('statement')+u(2)+b''.join(u(v) for v in shape)+u(28)+text('zkGPT/main')+u(16384)+bytes.fromhex(plan['circuit'])+fr(0)+b'\x01'*32+u(16384)+bytes(16384*32)+u(len(lookup))+lookup
 return out
for name in ['good','X','padding','context','W','shape']:
 p=data/('full12-synthetic.statement' if name=='good' else 'bad-'+name+'.statement');assert not p.exists();p.write_bytes(statement(name))
for name,b in [('truncated',statement()[:-40]),('trailing',statement()+b'bad')]:
 p=data/('bad-'+name+'.statement');assert not p.exists();p.write_bytes(b)
print('Synthetic metadata fixtures only, eight statements; no proof or model program')
