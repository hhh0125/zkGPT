from pathlib import Path
import os,sys,json,hashlib,shutil,subprocess,difflib,datetime
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(r);os.sched_setaffinity(0,[0,1,2,3])
prior=r/'reports/PERF-P0B-20261021';d=r/'reports/PERF-P0B-ONEBLOCK-20261021';logs=r/'logs/PERF-P0B-ONEBLOCK-20261021';d.mkdir(exist_ok=False);logs.mkdir(exist_ok=False)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def dump(n,v):(d/n).write_text(json.dumps(v,indent=2,ensure_ascii=False)+'\n')
assert json.loads((r/'reports/migration-20261020/MIGRATION_RESULT.json').read_text())['success']
expected='5c7053817202c418cd63947e8b75d83e7d04f4279abfe6b9f64cda708a6ae416'
source=json.loads((prior/'AFTER_SOURCE_SHA.json').read_text());current={str(p.relative_to(r)):sha(p) for folder in ['src','3rd','deps'] for p in (r/folder).rglob('*') if p.is_file() and '.git' not in p.parts and '__pycache__' not in p.parts};current.update({p.name:sha(p) for p in r.iterdir() if p.is_file()});assert current==source
assert hashlib.sha256(json.dumps(current,sort_keys=True,separators=(',',':')).encode()).hexdigest()==expected
for name in ['FROZEN_PARAMETER_PROFILE_SHA.json','PROTECTED_APPROVAL_ARTIFACT_SHA.json','PROTECTED_P0A_SHA.json']:
 data=json.loads((prior/name).read_text());assert all(sha(p)==v for p,v in data.items()),name;shutil.copy2(prior/name,d/name)
for name,row in json.loads((prior/'AFTER_BINARY_DEPENDENCY_SHA.json').read_text()).items():assert sha(name)==row['sha256'],name
for name,row in json.loads((prior/'BEFORE_BINARY_DEPENDENCY_SHA.json').read_text()).items():assert sha(row['copy'])==row['sha256'],name
identities={str(r/name):row['sha256'] for name,row in json.loads((prior/'AFTER_BINARY_DEPENDENCY_SHA.json').read_text()).items()}
for name,row in json.loads((prior/'BEFORE_BINARY_DEPENDENCY_SHA.json').read_text()).items():identities[row['copy']]=row['sha256']
for name,values in [('model.proof',(5683187,'59df6b40265249bcb6a861ed27c0ef6461d88993ffa80d362086cda8672c4ca4')),('model.statement',(3174795,'926615c1e21744bd271e1b225e908dea9abc234d2fb7c860e2b8c3d4977cb4fb'))]:
 p=prior/'reference/frozen-oneblock'/name;assert p.stat().st_size==values[0] and sha(p)==values[1];identities[str(p)]=values[1]
for name in ['oneblock_resource_policy.py','oneblock_supervisor.py']:shutil.copy2(prior/name,d/name);identities[str(d/name)]=sha(d/name)
sys.path.insert(0,str(r/'reports/R6B-OPT2-20261021'));import r6b_resources
sys.path.insert(0,str(d));from oneblock_resource_policy import check
for mem in [24,8]:
 row=r6b_resources.snapshot(r);row['WARN_other_user_large_jobs']=check(row,mem);dump('INITIAL_P'+str(mem)+'_GATE.json',row);print('INITIAL_HARD_GATE PASS memoryGiB',mem,'WARN_foreign',len(row['WARN_other_user_large_jobs']),flush=True)
(d/'AUTHORIZATION_AND_PLAN.md').write_text('''# 本轮授权及执行计划

用户明确授权2026-10-21一次P0B真实R5B oneblock验收，源码5c7053817202c418cd63947e8b75d83e7d04f4279abfe6b9f64cda708a6ae416。一次生产Prover；新V验新、P0A冻结旧V验新、新V验旧；真实产物必要负向；仅验证器P0A/P0B等价诊断，不再生成诊断证明。无full12/P1/permit/旧scope重绑。任一硬门禁/证明/验证/字节/负向失败立即停止，不重试。

1.核对完整源码、所有生产二进制/依赖、126参数/Profile/anchor/manifest、历史保护及冻结证明，原full12资源脚本不变；P24/V8门禁。
2.仅在reports新建等价诊断TU/库/二进制：原kernel函数体改名后包装一次chrono/clock计时，旧DP入口计数/新宏计数。生产源码和二进制保持不变。小fixture验证诊断链接正确。
3.默认gate-only；显式--execute-oneblock写入不可重复token，生产输出leaf必须不存在由Prover建立。生产一次，新旧独立验证三次，证明字节比较。外部operator-trust-v2.anchor SHA和engineering-model-manifest-v1.txt显式传参，验证器从空工作目录运行。
4.真实负向复用已审阅P0A六种篡改/七次拒绝；确认实际REJECT文本不是usage。资源限制所有子验证同一V8，遇错停止。
5.冻结证明在P0A和P0B诊断Verifier各验证一次，得到真实challenge kernel耗时/CPU/调用/命中/结果，严格与生产wall分开。生产只有一次Prover。
6.逐项汇总实际wall/user/sys/RSS/资源/频率，比较P0A完整生产基线310.965864/169.688897；诊断只对诊断。完整归档Windows恢复读取核验。

Ruling:复用已批准执行计划；本轮不修改生产协议或实现，测试编排和只读诊断属于明确授权。固定工作区、不建worktree、不提交/删除历史；本报告目录为唯一新增阶段。外部大任务WARN规则仅用本阶段复制的oneblock policy，不修改正式full12门禁。
''')
header='''#include <chrono>
#include <ctime>
#include <iostream>
#include <iomanip>
struct P0BDiagHits {unsigned long long singleton=0,contiguous=0,fallback=0;std::size_t cache_peak_bytes=0;};
static thread_local P0BDiagHits p0b_diag_hits;
static thread_local unsigned long long p0b_diag_calls=0;
#define P0B_KERNEL_HIT(kind) (++p0b_diag_hits.kind)
#define P0B_KERNEL_CACHE(bytes) (p0b_diag_hits.cache_peak_bytes=std::max(p0b_diag_hits.cache_peak_bytes,std::size_t(bytes)))
'''
commands=[]
def build(name,args):
 commands.append(args);p=subprocess.run(args,capture_output=True,text=True);(logs/(name+'.log')).write_text(' '.join(args)+'\n'+p.stdout+p.stderr);assert p.returncode==0,(name,(p.stdout+p.stderr)[-3000:]);print(name,'PASS',flush=True)
common=['c++','-pthread','-O3','-DNDEBUG','-std=gnu++17','-Wall','-Wextra','-Wpedantic','-Werror','-DMCLBN_NO_AUTOLINK','-DMCL_NO_AUTOLINK','-DMCL_USE_LLVM=1','-DR1_VERIFY_ONLY=1','-Isrc/paper_r1','-I3rd/mcl/include']
for mode in ['p0a','p0b']:
 folder=d/mode;folder.mkdir();original=(prior/'mapped_input.p0a.cpp').read_text() if mode=='p0a' else (r/'src/paper_r1/mapped_input.cpp').read_text()
 if mode=='p0a':assert hashlib.sha256(original.encode()).hexdigest()==json.loads((prior/'BEFORE_SOURCE_SHA.json').read_text())['src/paper_r1/mapped_input.cpp']
 modified=original
 if mode=='p0a':
  start=modified.index('Fr spanKernel(');brace=modified.index('{',start)+1;modified=modified[:brace]+'\n    P0B_KERNEL_HIT(fallback);'+modified[brace:]
 assert modified.count('Fr kernelValue(')==1;modified=modified.replace('Fr kernelValue(','Fr kernelValueImpl(',1)
 wrapper='''Fr kernelValue(const MappedInputSpec &s,const Batch &b,const std::vector<Fr> &r,const std::vector<CheckedInputClaim> &checked) {
 p0b_diag_hits={};++p0b_diag_calls;const auto start=std::chrono::steady_clock::now();const auto cpu=std::clock();
 const Fr value=kernelValueImpl(s,b,r,checked);
 const double wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count(),cpu_s=double(std::clock()-cpu)/CLOCKS_PER_SEC;
 std::cerr<<std::setprecision(12)<<"P0B_REAL_KERNEL mode=MODE calls="<<p0b_diag_calls<<" wall_s="<<wall<<" cpu_s="<<cpu_s<<" singleton_hits="<<p0b_diag_hits.singleton<<" contiguous_hits="<<p0b_diag_hits.contiguous<<" fallback_hits="<<p0b_diag_hits.fallback<<" cache_peak_bytes="<<p0b_diag_hits.cache_peak_bytes<<" result_hex=";
 for(auto byte:encode(value))std::cerr<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(byte);
 std::cerr<<std::dec<<std::setfill(' ')<<'\\n';return value;
}
'''.replace('MODE',mode)
 assert modified.count('} // namespace\n#ifndef R1_VERIFY_ONLY')==1;modified=modified.replace('} // namespace\n#ifndef R1_VERIFY_ONLY',wrapper+'} // namespace\n#ifndef R1_VERIFY_ONLY',1)
 modified=header+modified;src=folder/'mapped_input.cpp';src.write_text(modified);(folder/'diagnostic.patch').write_text(''.join(difflib.unified_diff(original.splitlines(True),modified.splitlines(True),fromfile=mode+'/production',tofile=mode+'/diagnostic')))
 obj=folder/'mapped_input.cpp.o';build('01-'+mode+'-compile',common+['-c',str(src),'-o',str(obj)])
 oldlib=prior/'frozen/build/release/src/paper_r1/libpaper_r1_verifier.a' if mode=='p0a' else r/'build/release/src/paper_r1/libpaper_r1_verifier.a';lib=folder/'libpaper_r1_verifier.a';shutil.copy2(oldlib,lib);build('02-'+mode+'-archive',['ar','r',str(lib),str(obj)])
 libs=['build/release/src/paper_r2/libpaper_r2_verifier.a','build/release/src/libgpt_lib.a','deps/cnpy/lib/libcnpy.a','-lz',str(lib),'build/release/lib/libmclbn256.so.1.74','build/release/lib/libmcl.so.1.74','-lpthread','-Wl,-rpath,'+str(r/'build/release/lib')]
 for target,main in [('r5b_model_verify','r5b_model_verify.cpp.o'),('r2_verify','verify_main.cpp.o')]:
  binary=folder/target;build('03-'+mode+'-'+target+'-link',['c++','-pthread','-O3','-DNDEBUG','build/release/src/paper_r2/CMakeFiles/'+target+'.dir/'+main]+libs+['-o',str(binary)]);identities[str(binary)]=sha(binary)
 identfiles=[src,obj,lib];identities.update({str(p):sha(p) for p in identfiles})
dump('DIAGNOSTIC_BUILD_COMMANDS.json',commands)
dump('IDENTITY_FILES.json',identities);dump('SOURCE_IDENTITY.json',{'source_sha256':expected,'files':source})
for p in [r/'reports/R6B-OPT2-20261021/r6b_resources.py',r/'reports/R6A/driver.py']:
 if p.is_file():identities[str(p)]=sha(p)
dump('IDENTITY_FILES.json',identities)
ns={};exec(compile((d/'oneblock_supervisor.py').read_text(),'unchanged supervisor','exec'),ns);ns['r']=d;ns['logs']=logs
fixture=prior/'small-integration-compatibility/after'
for mode in ['p0a','p0b']:
 row=r6b_resources.snapshot(r);row['WARN_other_user_large_jobs']=check(row,8);dump('04-'+mode+'-small-gate.json',row)
 ns['run']('04-'+mode+'-small-diagnostic',[str(d/mode/'r2_verify'),'--fixture',str(fixture/'valid.statement'),str(fixture/'valid.proof')],timeout=1200,mem_gib=8,disk_gib=64,reserve_gib=32)
 text=(logs/('04-'+mode+'-small-diagnostic.log')).read_text();assert 'ACCEPT R2 fixture' in text and 'P0B_REAL_KERNEL' in text and 'usage:' not in text
assert all(sha(r/p)==v for p,v in source.items())
dump('READY.json',{'source_sha256':expected,'hard_gates_passed':True,'small_diagnostic_equivalence_passed':True,'production_run_started':False,'no_full12_no_P1_no_permit':True})
(d/'progress.md').write_text('Plan: AUTHORIZATION_AND_PLAN.md. Preparation identity/P24V8gates/diagnosticTUbuild+smallfixture equivalence complete. No production run yet. User separately authorized exactly one realoneblock acceptance. No retries.\n')
print('READY_FOR_EXPLICIT_SINGLE_EXECUTION',flush=True)
