"""Spec-driven integer reference: NumPy matrix math and Python unbounded integers.
No native binary, witness generator, model runtime, or production helper import.
"""
import os,resource,time,json,hashlib,math,struct,re
from pathlib import Path
os.sched_setaffinity(0, sorted(os.sched_getaffinity(0))[:4])
resource.setrlimit(resource.RLIMIT_AS,(6*1024**3,6*1024**3))
import numpy as np
from decimal import Decimal,localcontext,ROUND_HALF_UP
from parse_statement import parse
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')
OUT=Path(__file__).resolve().parent
assert Path.cwd()==ROOT
START=time.monotonic()
sha=lambda b:hashlib.sha256(b).hexdigest()
delivery=ROOT/'reports/R6-FULL12-P0AB-20261021/D-EXECUTION-20261022'
statement=ROOT/'reports/R6-FULL12-P0AB-20261021-v1-full12/model.statement'
assert sha(statement.read_bytes())=='6a73f7a6d71a0c8b4f9d19a08ef4cbf68e4730491d4bf5377b0ae170c7e52958'
meta,xb,yb=parse(statement)
assert xb==(delivery/'FULL12_PUBLIC_X_SIGNED64_LE.bin').read_bytes()
assert yb==(delivery/'FULL12_FINAL_BLOCK_Y_SIGNED64_LE.bin').read_bytes()
x=np.frombuffer(xb,dtype='<i8').reshape(32,1024)[:,:768].copy()
target=np.frombuffer(yb,dtype='<i8').reshape(32,1024)
trace=[];bounds=[];parameter_hashes={};numeric=[]
def integer_round(n,d):
 # Euclidean quotient/remainder; handles negative ties without float.
 q,r=divmod(int(n),int(d));return q+int(2*r>=int(d))
def checked64(v):
 a=np.asarray(v,dtype=object)
 assert all(-(1<<63)<=int(z)<(1<<63) for z in a.flat)
 return a.astype(np.int64)
def record(block,name,a):
 a=checked64(a);raw=a.astype('<i8').tobytes()
 trace.append({'block':block,'operator':name,'shape':list(a.shape),'min':int(a.min()),'max':int(a.max()),'sha256_i64le':sha(raw),'elements':a.size})
 np.save(OUT/('block%02d_%s.npy'%(block,name)),a,allow_pickle=False)
 return a
def params(name):
 b=(ROOT/'data/gpt2_int'/name).read_bytes();parameter_hashes[name]=sha(b);return b
def fc(a,k):
 b=params('fc_%d.bin'%k);nout,nin=struct.unpack('<ii',b[:8]);w=np.frombuffer(b,dtype='<i4',offset=8).reshape(nout,nin).astype(np.int64)
 b=params('fc_%d_bias.bin'%k);n,=struct.unpack('<i',b[:4]);bias=np.frombuffer(b,dtype='<i4',offset=4).astype(np.int64);assert n==nout
 bound=int(np.max(np.abs(a)))*int(np.max(np.abs(w)))*nin+int(np.max(np.abs(bias)))
 assert bound<(1<<62)
 bounds.append({'fc':k,'input_absmax':int(np.max(np.abs(a))),'weight_absmax':int(np.max(np.abs(w))),'bias_absmax':int(np.max(np.abs(bias))),'accumulator_abs_bound':bound})
 sums=a@w.T+bias
 return (sums//1024+(sums%1024>=512)).astype(np.int64)
def norm(a,k):
 b=params('ln_%d.bin'%k);n,wc,we,bc,be=struct.unpack('<5i',b[:20]);assert (n,wc,we,bc,be)==(768,1,-10,1,-10)
 v=np.frombuffer(b,dtype='<i4',offset=20).astype(np.int64);g,beta=v[:768],v[768:];out=[];ideal=[]
 for row in a:
  # Evaluate the declared rational epsilon and fixed root policy with unbounded ints.
  row_sum=sum(map(int,row))
  centered=[768*int(z)-row_sum for z in row]
  variance_numerator=sum(z*z for z in centered)
  root=math.isqrt(60000000*(78125*variance_numerator+3710851743744))
  assert root>0
  out.append([integer_round(60000000*int(g[j])*centered[j]+int(beta[j])*root,10*root) for j in range(768)])
  real=row.astype(np.float64)*(5/512)
  ideal.append(((real-real.mean())/np.sqrt(np.mean((real-real.mean())**2)+1e-5)*(g/1024)+beta/1024)/(5/512))
 result=checked64(out);delta=result.astype(float)-np.asarray(ideal)
 numeric.append({'LN':k,'vs_unrounded_real_on_same_input_max_abs_activation_units':float(np.max(np.abs(delta))),'gamma_absmax':int(np.max(np.abs(g))),'beta_absmax':int(np.max(np.abs(beta)))})
 return result
with localcontext() as ctx:
 ctx.prec=80
 table=np.asarray([max(int((Decimal(2)**20*(-Decimal(i)/512).exp()).to_integral_value(rounding=ROUND_HALF_UP)),1) for i in range(8192)],dtype=np.int64)
assert sha(table.astype('<u4').tobytes())=='062c686f919b356c9f8c47acada933bc2683c13ac8886a0cb881239e807f21dc'
def attention(qkv,block):
 q,k,v=np.split(qkv,3,axis=1);q=q.reshape(32,12,64).transpose(1,0,2);k=k.reshape(32,12,64).transpose(1,0,2);v=v.reshape(32,12,64).transpose(1,0,2)
 bound=int(np.max(np.abs(q)))*int(np.max(np.abs(k)))*64*25;assert bound<(1<<62)
 raw=q@k.transpose(0,2,1);scores=raw*25//4096+(raw*25%4096>=2048)
 output=np.empty((12,32,64),dtype=np.int64);gaps=[];expvals=[];softmax_deviation=[]
 for h in range(12):
  for i in range(32):
   sr=scores[h,i,:i+1];gap=sr.max()-sr;assert int(gap.max())<2**32
   e=table[np.minimum(gap,8191)];denom=int(e.sum());assert denom>0
   weighted=e@v[h,:i+1];assert int(e.sum())*int(np.max(np.abs(v[h,:i+1])))<1<<62
   output[h,i]=checked64([integer_round(z,denom) for z in weighted]);gaps.extend(gap.tolist());expvals.extend(e.tolist())
   f=raw[h,i,:i+1].astype(float)*(25/(2**21));f=np.exp(f-f.max());f=f/f.sum()
   softmax_deviation.extend(np.abs(e/e.sum()-f).tolist())
 numeric.append({'Attention':block,'gap_min':min(gaps),'gap_max':max(gaps),'clamp_tail_queries':sum(g>=8191 for g in gaps),'max_probability_difference_from_real_softmax_same_qkv':max(softmax_deviation)})
 record(block,'attention_gaps',np.asarray(gaps));record(block,'attention_exp',np.asarray(expvals))
 return output.transpose(1,0,2).reshape(32,768)
def gelu(a,block):
 # Scalar polynomial from the frozen Profile, independent of witness construction.
 values=[]
 for z in a.flat:
  z=int(z);u=abs(z)
  numerator=4194304*(z+u)
  if u<=246: numerator+=u*(-75*u*u+34720*u-4002420)
  values.append(integer_round(numerator,8388608))
 out=checked64(values).reshape(a.shape)
 real=a.astype(float)*(5/512);ideal=0.5*real*(1+np.tanh(np.sqrt(2/np.pi)*(real+0.044715*real**3)))
 numeric.append({'GELU':block,'max_abs_real_error_same_input':float(np.max(np.abs(out*(5/512)-ideal))),'rmse_real':float(np.sqrt(np.mean((out*(5/512)-ideal)**2))),'inside_count':int(np.count_nonzero(np.abs(a)<=246))})
 return out
for block in range(12):
 n=record(block,'LN1',norm(x,2*block));qkv=record(block,'QKV',fc(n,4*block));att=record(block,'Attention',attention(qkv,block))
 projected=record(block,'AttentionProjection',fc(att,4*block+1));res=record(block,'AttentionResidual',checked64(x.astype(object)+projected.astype(object)))
 n=record(block,'LN2',norm(res,2*block+1));hidden=record(block,'MLPUp',fc(n,4*block+2));g=record(block,'GELU',gelu(hidden,block));down=record(block,'MLPDown',fc(g,4*block+3))
 x=record(block,'BlockOutput',checked64(res.astype(object)+down.astype(object)))
 print('REFERENCE block',block+1,'seconds',round(time.monotonic()-START,3),flush=True)
final=np.pad(x,((0,0),(0,256)));actual=target;different=np.argwhere(final!=actual)
delta=final.astype(object)-actual.astype(object)
stats={'status':'PASS' if not len(different) else 'GAP','total_elements':32768,'active_elements':24576,'padding_elements':8192,'mismatch_elements':len(different),'active_mismatch_elements':int(np.count_nonzero(final[:,:768]!=actual[:,:768])),'first_mismatch':None if not len(different) else {'row':int(different[0,0]),'column':int(different[0,1]),'reference':int(final[tuple(different[0])]),'statement':int(actual[tuple(different[0])])},'max_absolute_integer_difference':max(abs(int(z)) for z in delta.flat),'reference_Y_sha256':sha(final.astype('<i8').tobytes()),'statement_Y_sha256':sha(yb),'wall_seconds':time.monotonic()-START,'affinity':sorted(os.sched_getaffinity(0)),'address_space_limit_bytes':resource.getrlimit(resource.RLIMIT_AS)[0],'max_RSS_KiB':resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,'parameters_read':len(parameter_hashes),'parameter_SHA256':parameter_hashes,'matrix_safety_bounds':bounds,'operator_profile_deviation':numeric,'operator_trace':trace,'comparison_limit':'Actual intermediate full12 witness arrays were not found; per-op traces are independently computed references, not direct comparisons with actual witness. Only final public Y has all-element actual comparison. No original checkpoint inference claim.'}
(OUT/'REFERENCE_COMPARISON.json').write_text(json.dumps(stats,indent=2)+'\n');(OUT/'REFERENCE_Y_SIGNED64_LE.bin').write_bytes(final.astype('<i8').tobytes());print(json.dumps({k:v for k,v in stats.items() if k not in ['operator_trace','matrix_safety_bounds','operator_profile_deviation','parameter_SHA256']},indent=2))
