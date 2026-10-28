from pathlib import Path
import json,itertools,collections,resource,time,math,hashlib
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd()==r
out=r/'reports/R7-STAGE1-TASK0A-20261022-v1/research'
resource.setrlimit(resource.RLIMIT_CPU,(60,60));resource.setrlimit(resource.RLIMIT_AS,(512*1024**2,512*1024**2))
started=time.monotonic()
def sig(q,T,queries,reads,final):
 left=collections.Counter((a,v,0) for a,v in enumerate(T))
 left.update((a,v,(c+1)%q) for (a,v),c in zip(queries,reads))
 right=collections.Counter((a,v,c) for (a,v),c in zip(queries,reads))
 right.update((a,v,c) for a,(v,c) in enumerate(zip(T,final)))
 return left==right
experiments=[]
for q in [2,3]:
 T=(0,1);pairs=list(itertools.product(range(q),repeat=2))
 for N in range(q+1):
  tested=accepted=wrong=0;example=None
  for queries in itertools.product(pairs,repeat=N):
   good=all(a<len(T) and v==T[a] for a,v in queries)
   for reads in itertools.product(range(q),repeat=N):
    for final in itertools.product(range(q),repeat=len(T)):
     tested+=1
     if sig(q,T,queries,reads,final):
      accepted+=1
      if not good:
       wrong+=1
       if example is None:example={'queries':queries,'reads':reads,'final':final}
  assert wrong==0 if N<q else wrong>0
  experiments.append({'q':q,'N':N,'table':T,'assignments_tested':tested,'formal_multiset_accept':accepted,'wrong_membership_accept':wrong,'first_wrong':example})
def compositions(n,k,prefix=()):
 if k==1:yield prefix+(n,);return
 for x in range(n+1):yield from compositions(n-x,k-1,prefix+(x,))
cycles=[]
for q in [2,3,5,7]:
 for N in range(q+1):
  total=balanced=0
  for counts in compositions(N,q):
   total+=1
   if all(counts[c]==counts[(c-1)%q] for c in range(q)):balanced+=1
  assert balanced==(1 if N in [0,q] else 0)
  cycles.append({'q':q,'N':N,'edge_multiset_count':total,'balanced_count':balanced})
q=7;T=(2,3);queries=((0,2),(0,2));reads=(1,0);final=(2,0);assert sig(q,T,queries,reads,final)
def fingerprint(q,a,v,c,g,t):return (g*g*a+g*v+c-t)%q
def roots(q,T,queries,reads,final,g,t):
 prod=lambda xs:math.prod(xs)%q
 return {'initial':prod(fingerprint(q,a,v,0,g,t) for a,v in enumerate(T)),
 'write':prod(fingerprint(q,a,v,(c+1)%q,g,t) for (a,v),c in zip(queries,reads)),
 'read':prod(fingerprint(q,a,v,c,g,t) for (a,v),c in zip(queries,reads)),
 'final':prod(fingerprint(q,a,v,c,g,t) for a,(v,c) in enumerate(zip(T,final)))}
q=5;T=(0,);queries=((0,1),);reads=(0,);final=(1,)
assert not sig(q,T,queries,reads,final)
passing=[]
for g,t in itertools.product(range(q),repeat=2):
 p=roots(q,T,queries,reads,final,g,t)
 if p['initial']*p['write']%q==p['read']*p['final']%q:passing.append({'gamma':g,'tau':t,'roots':p})
assert len(passing)==q and all(x['gamma']==0 for x in passing)
# Symbolic products in Fq[gamma,tau], independent of factor multiset comparison.
def multiply(p,a,v,c,q):
 o={}
 for (dg,dt),coef in p.items():
  for x,y,k in [(2,0,a),(1,0,v),(0,0,c),(0,1,-1)]:
   key=(dg+x,dt+y);o[key]=(o.get(key,0)+coef*k)%q
 return {key:v for key,v in o.items() if v}
def polynomial(q,factors):
 p={(0,0):1}
 for a,v,c in factors:p=multiply(p,a,v,c,q)
 return p
symbolic=[]
for q in [2,3,5,7]:
 a,v=0,1
 left=[(0,0,0)]+[(a,v,(c+1)%q) for c in range(q)]
 right=[(a,v,c) for c in range(q)]+[(0,0,0)]
 assert polynomial(q,left)==polynomial(q,right)
 symbolic.append({'q':q,'N':q,'wrong_pair':[a,v],'read':list(range(q)),'polynomial_identity':True})
j={'scope':'small-prime algebra only; no model witness, PC/MSM, proof or crypto verifier','exhaustive':experiments,'balanced_edge_multisets':cycles,'nonchronological_valid_example':{'q':7,'table':[2,3],'queries':[[0,2],[0,2]],'reads':[1,0],'final':[2,0]},'single_point_false_accept':{'q':5,'N':1,'table':[0],'queries':[[0,1]],'reads':[0],'final':[1],'formal_identity':False,'symbolic_difference':'-gamma','passing_points':passing,'passing_count':len(passing),'point_count':25},'N_equals_q_symbolic_counterexamples':symbolic,'elapsed_seconds':time.monotonic()-started}
(out/'MALICIOUS_COUNT_EXPERIMENTS.json').write_text(json.dumps(j,indent=2)+'\n')
print(json.dumps({'exhaustive_assignments':sum(x['assignments_tested'] for x in experiments),'wrong_below_characteristic':sum(x['wrong_membership_accept'] for x in experiments if x['N']<x['q']),'wrong_at_characteristic':[(x['q'],x['wrong_membership_accept']) for x in experiments if x['N']==x['q']],'elapsed_seconds':j['elapsed_seconds']},indent=2))
