"""Read-only checkpoint provenance, exact quantized re-export, and raw F32-weight float64 block reference."""
from pathlib import Path
import os,resource,json,struct,hashlib,time,urllib.request
os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
resource.setrlimit(resource.RLIMIT_AS,(6*1024**3,6*1024**3))
import numpy as np
from parse_statement import parse
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd()==ROOT
OUT=Path(__file__).resolve().parent;start=time.monotonic()
rev='607a30d783dfa663caf39e06633721c8d4cfcd7e'
checkpoint=Path('/home/yttan/.cache/huggingface/hub/models--gpt2/snapshots')/rev/'model.safetensors'
def sha_file(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(8*1024*1024),b''):h.update(b)
 return h.hexdigest()
checkpoint_sha=sha_file(checkpoint);assert checkpoint_sha=='248dfc3911869ec493c76e65bf2fcf7f615828b0254c12b473182f0f81d3a707'
with checkpoint.open('rb') as f:
 n=struct.unpack('<Q',f.read(8))[0];header=json.loads(f.read(n));offset=8+n
def tensor(name):
 spec=header[name];assert spec['dtype']=='F32'
 return np.memmap(checkpoint,dtype='<f4',mode='r',offset=offset+spec['data_offsets'][0],shape=tuple(spec['shape']))
def quant(a,scale):
 q=np.rint(a/scale);assert np.isfinite(q).all() and np.max(np.abs(q))<2**31
 return np.ascontiguousarray(q.astype('<i4'))
matches=[]
def compare(name,b):
 actual=(ROOT/'data/gpt2_int'/name).read_bytes();ok=b==actual
 matches.append({'file':name,'byte_identical':ok,'expected_sha256':hashlib.sha256(b).hexdigest(),'actual_sha256':hashlib.sha256(actual).hexdigest()});assert ok,name
for block in range(12):
 for k,suffix in enumerate(['attn.c_attn','attn.c_proj','mlp.c_fc','mlp.c_proj']):
  w=quant(tensor(f'h.{block}.{suffix}.weight').T,2.**-10);b=quant(tensor(f'h.{block}.{suffix}.bias'),10*2.**-20)
  compare(f'fc_{4*block+k}.bin',struct.pack('<ii',*w.shape)+w.tobytes())
  compare(f'fc_{4*block+k}_bias.bin',struct.pack('<i',b.size)+b.tobytes())
 for k,suffix in enumerate(['ln_1','ln_2']):
  g=quant(tensor(f'h.{block}.{suffix}.weight'),2.**-10);b=quant(tensor(f'h.{block}.{suffix}.bias'),2.**-10)
  compare(f'ln_{2*block+k}.bin',struct.pack('<5i',g.size,1,-10,1,-10)+g.tobytes()+b.tobytes())
public={}
url=f'https://huggingface.co/api/models/openai-community/gpt2/revision/{rev}?blobs=true'
try:
 with urllib.request.urlopen(url,timeout=30) as response: raw=response.read()
 (OUT/'HF_PINNED_METADATA.json').write_bytes(raw);meta=json.loads(raw)
 model=next(v for v in meta['siblings'] if v['rfilename']=='model.safetensors')
 assert meta['sha']==rev and model['lfs']['sha256']==checkpoint_sha and model['lfs']['size']==checkpoint.stat().st_size
 public={'status':'PASS','url':url,'metadata_sha256':hashlib.sha256(raw).hexdigest(),'revision':meta['sha'],'LFS':model['lfs']}
except Exception as e: public={'status':'BLOCKED','url':url,'error':repr(e)}
config=checkpoint.parent/'config.json';cfg=json.loads(config.read_text());assert cfg['n_embd']==768 and cfg['n_head']==12 and cfg['n_layer']==12 and cfg['layer_norm_epsilon']==1e-5
_,xb,yb=parse(ROOT/'reports/R6-FULL12-P0AB-20261021-v1-full12/model.statement')
x=np.frombuffer(xb,dtype='<i8').reshape(32,1024)[:,:768].astype(np.float64)*(5/512)
y=np.frombuffer(yb,dtype='<i8').reshape(32,1024)[:,:768].astype(np.float64)*(5/512)
traces=[]
def norm(x,block,name):
 a=x-x.mean(axis=-1,keepdims=True);z=a/np.sqrt(np.mean(a*a,axis=-1,keepdims=True)+cfg['layer_norm_epsilon'])
 return z*np.asarray(tensor(f'h.{block}.{name}.weight'),dtype=np.float64)+tensor(f'h.{block}.{name}.bias')
def affine(x,block,name): return x@np.asarray(tensor(f'h.{block}.{name}.weight'),dtype=np.float64)+tensor(f'h.{block}.{name}.bias')
for block in range(12):
 n=norm(x,block,'ln_1');q,k,v=np.split(affine(n,block,'attn.c_attn'),3,axis=1)
 q=q.reshape(32,12,64).transpose(1,0,2);k=k.reshape(32,12,64).transpose(1,0,2);v=v.reshape(32,12,64).transpose(1,0,2)
 s=(q@k.transpose(0,2,1))/8
 s=np.where(np.arange(32)[None,None,:]<=np.arange(32)[None,:,None],s,-np.inf);s=np.exp(s-s.max(axis=-1,keepdims=True));s/=s.sum(axis=-1,keepdims=True)
 a=(s@v).transpose(1,0,2).reshape(32,768);x=x+affine(a,block,'attn.c_proj')
 h=affine(norm(x,block,'ln_2'),block,'mlp.c_fc');h=0.5*h*(1+np.tanh(np.sqrt(2/np.pi)*(h+0.044715*h**3)));x=x+affine(h,block,'mlp.c_proj')
 np.save(OUT/f'checkpoint_block{block:02d}.npy',x,allow_pickle=False)
 qref=np.load(OUT/f'block{block:02d}_BlockOutput.npy').astype(np.float64)*(5/512);delta=qref-x
 traces.append({'block':block+1,'max_abs_real_difference':float(np.max(np.abs(delta))),'rmse_real':float(np.sqrt(np.mean(delta*delta)))})
delta=y-x;roundtrip=np.floor(x/(5/512)+0.5).astype(np.int64);target=np.frombuffer(yb,dtype='<i8').reshape(32,1024)[:,:768];m=np.argwhere(roundtrip!=target)
result={'status':'PASS','checkpoint':str(checkpoint),'resolved_path':str(checkpoint.resolve()),'checkpoint_bytes':checkpoint.stat().st_size,'checkpoint_SHA256':checkpoint_sha,'public_LFS_identity':public,'cached_config_SHA256':sha_file(config),'config':cfg,'quantized_files_checked':len(matches),'quantized_files_matched':sum(v['byte_identical'] for v in matches),'parameter_results':matches,'reference_semantics':'Raw hosted checkpoint F32 tensors evaluated in NumPy float64, OpenAI GPT-2 tanh-GELU, real softmax, epsilon1e-5, 1/sqrt64 attention, two residuals. Input is public quantized activation dequantized by5/512. Stops at block12 output, before ln_f/head. This is an independent mathematical surrogate, not bit-identical TensorFlow/PyTorch float32 execution.','final_comparison':{'status':'GAP','active_elements':24576,'max_abs_real_difference':float(np.max(np.abs(delta))),'rmse_real':float(np.sqrt(np.mean(delta*delta))),'integer_requantized_mismatch_elements':len(m),'first_requantized_mismatch':None if not len(m) else {'row':int(m[0,0]),'column':int(m[0,1]),'checkpoint_reference_quantized':int(roundtrip[tuple(m[0])]),'statement_Y':int(target[tuple(m[0])])}},'block_comparisons':traces,'wall_seconds':time.monotonic()-start,'max_RSS_KiB':resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,'affinity':sorted(os.sched_getaffinity(0)),'limits':['No original OpenAI TensorFlow release tensor-by-tensor provenance verification.','No historical generation execution provenance is inferred.','No approved whole-model approximation error budget or IEEE runtime parity claim.']}
(OUT/'CHECKPOINT_REFERENCE.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k not in ['parameter_results','config','block_comparisons']},indent=2))
