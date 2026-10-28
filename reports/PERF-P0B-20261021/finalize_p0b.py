from pathlib import Path
import os,sys,json,hashlib,tarfile,shutil,subprocess,datetime,difflib,re,statistics
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(r);os.sched_setaffinity(0,[0,1,2,3]);d=r/'reports/PERF-P0B-20261021';logs=r/'logs/PERF-P0B-20261021'
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(1<<20),b''):h.update(b)
 return h.hexdigest()
def dump(n,v):(d/n).write_text(json.dumps(v,indent=2,ensure_ascii=False)+'\n')
before=json.loads((d/'BEFORE_SOURCE_SHA.json').read_text())
files=sorted({p for folder in ['src','3rd','deps'] for p in Path(folder).rglob('*') if p.is_file() and '.git' not in p.parts and '__pycache__' not in p.parts}|{p for p in Path('.').iterdir() if p.is_file()})
after={str(p):sha(p) for p in files};tree=hashlib.sha256(json.dumps(after,sort_keys=True,separators=(',',':')).encode()).hexdigest()
changed=sorted(p for p in set(before)|set(after) if before.get(p)!=after.get(p));assert changed==['src/paper_r1/CMakeLists.txt','src/paper_r1/mapped_input.cpp','src/paper_r1/tests_mapped_kernel.cpp'],changed
assert len(after)==575
dump('AFTER_SOURCE_SHA.json',after);(d/'AFTER_SOURCE_TREE_SHA.txt').write_text(tree+'\n')
patch=[]
with tarfile.open(d/'before-source.tar.gz') as tf:
 for name in changed:
  old=tf.extractfile(name).read().decode() if name in before else ''
  patch.extend(difflib.unified_diff(old.splitlines(True),Path(name).read_text().splitlines(True),fromfile='a/'+name if name in before else '/dev/null',tofile='b/'+name))
(d/'P0B_SOURCE.patch').write_text(''.join(patch))
with tarfile.open(d/'after-source.tar.gz','w:gz',compresslevel=1) as tf:
 for p in files:tf.add(p,arcname=str(p),recursive=False)
protected={}
for name in ['FROZEN_PARAMETER_PROFILE_SHA.json','PROTECTED_APPROVAL_ARTIFACT_SHA.json','PROTECTED_P0A_SHA.json']:
 values=json.loads((d/name).read_text());assert all(sha(p)==v for p,v in values.items()),name;protected[name]={'entries':len(values),'all_unchanged':True}
old=(d/'mapped_input.p0a.cpp').read_text();new=Path('src/paper_r1/mapped_input.cpp').read_text()
unchanged={}
for label,start,end in [('batch','struct Batch {','#ifndef R1_VERIFY_ONLY\nstd::vector<Fr> kernel'),('dense_prover_kernel','std::vector<Fr> kernel','Fr kernelValue('),('tail','    // Product of two eq polynomials:','} // namespace'),('prove_verify_suffix','} // namespace',None)]:
 a=old[old.index(start):old.index(end,old.index(start)) if end else len(old)];b=new[new.index(start):new.index(end,new.index(start)) if end else len(new)];assert a==b,label;unchanged[label]=hashlib.sha256(a.encode()).hexdigest()
for name in ['src/paper_r1/core.cpp','src/paper_r1/core.hpp','src/paper_r1/pc.cpp','src/paper_r1/tests_g1_validation.cpp']:
 assert before[name]==after[name];unchanged[name]=after[name]
dump('UNCHANGED_PROTOCOL_AND_HISTORY.json',{'protected':protected,'byte_identical_regions_or_files':unchanged,'only_source_changes':changed,'DP':json.loads((d/'DP_BODY_UNCHANGED.json').read_text())})
# Read-only demonstration that old source authorization cannot transfer to this tree.
sys.path.insert(0,str(r/'reports/R6B-OPT2-20261021'));import r6b_resources
try:r6b_resources.identity_check({'root':str(r),'identity_files':{},'source_sha':'2961ae709eb5123b85427d768964a7b353caac331763700ac54eac5b2bde1da8'})
except Exception as e:
 assert 'current source tree changed' in str(e);old_scope={'old_P0A_identity_rejected':True,'reason':str(e),'read_only':True,'historical_scope_modified':False,'new_permit_generated':False}
else:raise AssertionError('old source identity incorrectly accepted')
dump('OLD_IDENTITY_REJECTION.json',old_scope)
bins={}
base={**json.loads((d/'BEFORE_BINARY_DEPENDENCY_SHA.json').read_text()),**json.loads((d/'BEFORE_SMALL_INTEGRATION_SHA.json').read_text())}
for p in sorted(set(base)|{'build/release/src/paper_r1/p0b_kernel_tests'}):
 dest=d/'current-bytes'/p;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dest);bins[p]={'sha256':sha(p),'bytes':Path(p).stat().st_size}
dump('AFTER_BINARY_DEPENDENCY_SHA.json',bins)
for p in [Path('build/release/CMakeCache.txt'),*Path('build/release/src/paper_r1').rglob('flags.make'),*Path('build/release/src/paper_r1').rglob('link.txt'),*Path('build/release/src/paper_r2').rglob('flags.make'),*Path('build/release/src/paper_r2').rglob('link.txt')]:
 dest=d/'after-build-configuration'/p;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dest)
ldd=subprocess.check_output(['ldd','build/release/src/paper_r2/r5b_model_verify'],text=True) if Path('build/release/src/paper_r2/r5b_model_verify').exists() else subprocess.check_output(['ldd','build/release/src/paper_r2/r5b_model_verify'],text=True)
runtime={}
for token in ldd.split():
 if token.startswith('/') and Path(token).is_file():
  p=Path(token);dest=d/'runtime-libraries'/p.relative_to('/');dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dest);runtime[token]={'sha256':sha(p),'bytes':p.stat().st_size}
dump('SYSTEM_RUNTIME_IDENTITY.json',{'ldd':ldd,'files':runtime})
def measure(name):
 meta=json.loads((d/(name+'.json')).read_text());text=(logs/(name+'.log')).read_text();time=(logs/(name+'.time')).read_text()
 fields=dict(re.findall(r'(\w+)=([^\s]+)',text.splitlines()[0])) if name.startswith(('04-','11-')) else {}
 return {'id':name,'started_utc':meta['started_utc'],'supervisor_wall_s':meta['seconds'],'kernel_wall_s':float(fields['wall_s']) if fields else None,'kernel_cpu_s':float(fields['cpu_s']) if fields else None,'user_s':float(re.search(r'User time \(seconds\): ([\d.]+)',time).group(1)),'system_s':float(re.search(r'System time \(seconds\): ([\d.]+)',time).group(1)),'max_rss_kib':int(re.search(r'Maximum resident set size \(kbytes\): (\d+)',time).group(1)),'observed_process_group_peak_bytes':meta['observed_process_group_peak_bytes'],'kernel_fields':fields,'exit_code':meta['exit_code'],'PASS':meta['PASS']}
baseline=measure('04-p0a-kernel-baseline');runs=[measure('11-p0b-isolated-kernel-'+str(i)) for i in range(1,4)]
assert all(x['kernel_fields']['result_hex']==baseline['kernel_fields']['result_hex'] for x in runs)
assert all(x['kernel_fields']['singleton_hits']=='3714264' and x['kernel_fields']['contiguous_hits']=='2021' and x['kernel_fields']['fallback_hits']=='759' for x in runs)
median=statistics.median(x['kernel_wall_s'] for x in runs)
compat=json.loads((d/'SMALL_COMPATIBILITY_RESULTS.json').read_text());assert len(compat['results'])==56 and all(x['PASS'] for x in compat['results'])
assert compat['valid_bytes_identical']
for mode in ['before','after']:
 for filename in ['valid.proof','valid.statement']:
  p=d/'small-integration-compatibility'/mode/filename;assert sha(p)==compat['artifacts'][mode+'/'+filename]['sha256']
for filename in ['valid.proof','valid.statement']:assert (d/'small-integration-compatibility/before'/filename).read_bytes()==(d/'small-integration-compatibility/after'/filename).read_bytes()
for name in ['14-small-release-regression','18-asan-mapped-protocol','18-ubsan-mapped-protocol','23-review-fix-release','25-review-fix-asan','25-review-fix-ubsan']:
 assert json.loads((d/(name+'.json')).read_text())['PASS'],name
for name in ['23-review-fix-release','25-review-fix-asan','25-review-fix-ubsan']:assert 'PASS P0B exact-field kernel checks=3902' in (logs/(name+'.log')).read_text()
assert '100% tests passed, 0 tests failed out of 27' in (logs/'14-small-release-regression.log').read_text()
resources=[json.loads(p.read_text()) for p in d.glob('*-resource.json')]
jobs={str(x['pid']):x for row in resources for x in row['visible_large_jobs']}
current=r6b_resources.snapshot(r)
freq={str(p):p.read_text().strip() for cpu in [0,1,2,3] for name in ['scaling_governor','scaling_cur_freq','cpuinfo_max_freq','cpuinfo_min_freq'] for p in [Path('/sys/devices/system/cpu/cpu'+str(cpu)+'/cpufreq')/name] if p.exists()}
dump('RESOURCE_AND_FREQUENCY_SUMMARY.json',{'snapshots':len(resources),'minimum_effective_memory_bytes':min(x['effective_memory_bytes'] for x in resources),'minimum_disk_free_bytes':min(x['disk_free_bytes'] for x in resources),'all_affinity_0_1_2_3':all(x['affinity']==[0,1,2,3] for x in resources),'large_jobs_WARN_only':list(jobs.values()),'current_snapshot':current,'frequency_after_tests':freq,'loadavg_after_tests':Path('/proc/loadavg').read_text().strip(),'baseline_and_benchmark_frequency_not_sampled':True,'frequency_difference_cannot_be_reconstructed':True,'no_other_process_changed':True})
result={'status':'SMALL_TESTS_PASS_REAL_ONEBLOCK_NOT_AUTHORIZED','source_before':'2961ae709eb5123b85427d768964a7b353caac331763700ac54eac5b2bde1da8','source_after':tree,'changed_source_files':changed,'baseline_isolated':baseline,'after_isolated_runs':runs,'after_isolated_kernel_wall_median_s':median,'isolated_function_observed_speedup':baseline['kernel_wall_s']/median,'measurement_scope':'kernelValue on frozen real public layout and synthetic evaluation points; NOT complete Prover/Verifier or frozen proof transcript','exact_result_equal':True,'selected_release_tests':27,'selected_release_failed':0,'differential_assertions_final':3902,'targeted_ASan_UBSan_pass':True,'small_fixture_cross_accepts':4,'small_fixture_negative_rejects':52,'small_fixture_proof_statement_bytes_identical':True,'P0B_real_oneblock_prover_wall_s':None,'P0B_real_oneblock_verifier_wall_s':None,'P0B_real_oneblock_CPU_RSS':None,'P0B_real_oneblock_proof_statement_comparison':None,'P0B_real_oneblock_cross_verification':None,'real_end_to_end_speedup_measured':False,'full_P0B_PASS':False,'P1_started':False,'full12_run':False,'permit_generated':False,'old_scope_rebound':False}
dump('P0B_RESULT.json',result)
dump('NEW_IDENTITY_SHA.json',{'source_tree_sha256':tree,'source_files':len(after),'source_patch_sha256':sha(d/'P0B_SOURCE.patch'),'binaries_and_dependencies':bins,'old_authorization_not_inherited':True})
rows=[]
for x in [baseline]+runs:rows.append(f"| {x['id']} | {x['kernel_wall_s']:.9f} | {x['kernel_cpu_s']:.6f} | {x['supervisor_wall_s']:.3f} | {x['user_s']:.2f} / {x['system_s']:.2f} | {x['max_rss_kib']} |")
report=f'''# P0-B mapped-input kernel 开发与小规模验收

状态：**小规模 PASS；真实 R5B oneblock 未授权、未运行，完整 P0-B 待验收。** 本轮没有取得或声称真实端到端加速。停止于 P0-B，未启动 P1、full12，未生成 permit，未转移历史 scope。

## 实际性能

先测未修改的 P0-A 实现，再修改源码。相同4CPU affinity、相同公开布局/冻结 statement、相同确定性全域点和系数、相同编译优化与依赖。布局包含51条 checked terminal claims、3,716,994 checked spans（3,714,264 singleton），保留每个 FCONN 的两个点；其中额外第二点 claims 共8条，另有50条 child spans。所有3717044次span求值保持顺序和结果。

以下只测一次 `kernelValue`，域点为合成挑战流，**没有回放冻结 proof transcript，也没有执行完整独立 Verifier**。计时包括该函数内的 equality 表准备/销毁；没有嵌套子热点计时，不能称为排除了 spanKernel 的 strict-exclusive CPU 时间。进程 wall/CPU/RSS还包含 statement解析和布局读取。基线只测1次，优化后3次；不是统计置信区间。

| 运行 | kernel wall(s) | kernel CPU(s) | 进程 supervisor wall(s) | 进程 user/system(s) | max RSS(KiB) |
|---|---:|---:|---:|---:|---:|
{chr(10).join(rows)}

kernel wall中位数：112.634413521 → {median:.9f}s。此函数观测比值 {baseline['kernel_wall_s']/median:.3f}× **不能作为完整 Verifier 加速比**。P0-A历史完整 P310.965864s/V169.688897s仅作为历史参考，不能减去隔离函数差值来宣称 P0-B端到端结果。原始OPT2 P463.373/V308.094及插桩诊断427.956/298.330同样没有参与本次收益计算。

每次优化后 singleton / contiguous / fallback 命中：3,714,264 / 2,021 / 759；合计3,717,044，与旧 DP 调用数完全相同。旧 DP 全部3,717,044次。计数只在诊断编译生效。表容量峰值512KiB；这是 equality表容量，不是总分配/RSS。结果32bytes均为 `{baseline['kernel_fields']['result_hex']}`。布局SHA见 LAYOUT_BENCH_IDENTITY.json。

## 实现及等价边界

仅三个源码文件变化：mapped_input.cpp、独立差分测试、CMake测试注册。单点以 child/master Equality 权重乘积求值；2..16长度直接累加；较长区间最多32个共同对齐的dyadic blocks；奇偶错位、超出32块或私有无效范围走原 DP。原DP函数仅改名，函数体逐字节不变。

对共同按2^k对齐的区间，低k位完整遍历，求和为 `product_{{j<k}}((1-c_j)(1-r_j)+c_j*r_j)`；高位固定，乘以双方相应Equality因子。低/高位可分离，因此与原域内逐项求和严格相等。无除法，0/1坐标和消失因子均适用；有效范围限制保证k不超过双方维度。重复索引继续独立累加，bias在原batch处理；zero-tail与最终等式原样保留。

缓存不是全局映射：每次kernelValue拥有master点快照；每个当前child/claim拥有子点快照，当前对象结束即销毁。逻辑键是完整有序Fr坐标和维度，索引只作用于该不可变快照；没有地址键、跨调用/跨claim/跨角色持久状态。维度上限28，与原有效PC域边界一致；两个split表对象容量合计最多2MiB，另有O(d)坐标/低位pair向量及构造临时分配。最终测试验证同地址外部点突变、快照所有权、并发独立调用和实际2MiB表边界。

所有51 terminal、FCONN双点、spans顺序、lambda累加、bias、padding、零尾、最终等式及Fiat–Shamir绑定保留。Batch、dense prover kernel、tail、prove/verify suffix均做逐字节区段核对。R2/GKR/Range/Exp/Lasso、编码、Profile、参数、X/Y/W锚、P0-A群校验文件均未改变。正常小fixture proof/statement逐字节一致；真实模型产物兼容性仍须oneblock验收。

## 正确性证据

- RED阶段旧DP通过3891项数学检查，仅预期的“快路径未启用”断言失败；GREEN后加强至3902项：singleton/连续/重复索引/bias/padding/zero-tail/空边界/大域28维/随机全域点，与原DP及dense参考比较。
- 明确测试偏移0/2、长度64恰32块使用dyadic路径；长度66超限回退DP。缓存测试使用非零权重并比较坐标快照，避免零权重断言失效。
- 全release构建成功，选定基础/小fixture回归27/27通过；没有声称全部项目测试或真实模型通过。
- 定向ASan、UBSan各自运行kernel与small mapped-protocol，均无发现；测试加强后两种sanitizer的kernel各3902项再次通过。覆盖修改TU，未把未修改MCL汇编等称为全量插桩。ASan仅小测试因shadow虚拟地址不设RLIMIT_AS，实际RSS仍8GiB、4CPU、32GiB共享内存与64GiB磁盘保留；不是放宽生产Verifier8GiB限制。
- 小型4FCONN/16terminal fixture：旧V/旧proof、旧V/新proof、新V/旧proof、新V/新proof全部ACCEPT；13种负向×4组=52次REJECT，检查实际ACCEPT/REJECT文本且无usage错误。
- 小型有效statement为1296bytes，SHA `932bd27d8a484d896cdf8220dbf6bd8db1622f0254a191e2ac452565719faf17`；proof为87949bytes，SHA `3fa2a9e05dc8433861d00f3ac53c910b6b6111620ed454492252781f2cfed14c`，旧新完全相同。
- 初次交叉验证脚本漏传--fixture，20-small-*只得到CLI用法错误，全部留存但不计接受/拒绝证据；修正后21-fixture-*为有效56项。另22构建日志记录错误的目标名，22b使用实际目标后通过。均为测试编排问题，没有协议拒绝不兼容。
- 一次独立只读源码审查无Critical/Important；两项Minor测试缺口已补齐。见INDEPENDENT_REVIEW.md。

## 资源与身份

运行使用CPU0..3，测试不超过8GiB RSS，300秒定向超时，每次预检及运行监控保留32GiB有效可用内存和64GiB磁盘，检查cgroup、RLIMIT、PID及实际容量。其他用户大进程只WARN，未改动其进程；逐次快照与原始time均归档。RESOURCE_AND_FREQUENCY_SUMMARY.json汇总可见竞争。基线/基准当时频率没有采样，无法补推频率差；测试后频率/governor/load只作环境记录。共享主机并非独占，毫秒级结果有调度/竞争不确定性。

旧源码SHA：2961ae709eb5123b85427d768964a7b353caac331763700ac54eac5b2bde1da8。

新源码SHA：`{tree}`（575files）。所有新二进制/依赖身份见NEW_IDENTITY_SHA.json。126项参数/Profile/anchor/manifest、129项历史审批、P0-A历史保护产物重新SHA核验不变。旧身份只读检查明确拒绝新源码，历史scope没有改写或重新绑定。

## 后续真实验收

只有另行明确授权一次P0-B真实oneblock后，才可在最新硬门禁通过时运行：4CPU/P24GiB/V8GiB、32GiB共享内存/64GiB磁盘保留、原1500/1200s超时、其他用户大型任务仅oneblock-only WARN。新scope身份须独立绑定本次SHA，不能继承旧full12批准；无permit、full12或P1自动流程。失败停止，不自动重试。

具体待验收项见REAL_ONEBLOCK_ACCEPTANCE_PENDING.md：新P生成真实proof/statement与冻结原件逐字节比较；新V验新、冻结P0A旧V验新、新V验冻结旧均ACCEPT；必要真实篡改全部REJECT；保存完整P/Vwall/user/sys/RSS及实际kernel计时/命中。真实端到端P/Vwall、CPU、RSS、字节和独立验证目前均为NOT_RUN_NOT_AUTHORIZED。完成这些后再决定P1。
'''
(d/'P0B_PERFORMANCE_REPORT.md').write_text(report)
(d/'KERNEL_EQUIVALENCE_AND_CACHE_BOUNDARY.md').write_text(report[report.index('## 实现及等价边界'):report.index('## 正确性证据')])
(d/'REAL_ONEBLOCK_ACCEPTANCE_PENDING.md').write_text(f'''# P0-B真实oneblock待授权验收

本文件是可审阅验收范围，不是permit，不会触发运行。本轮没有授权真实oneblock Prover或Verifier。

候选源码SHA {tree}；二进制精确SHA见NEW_IDENTITY_SHA.json。仅固定zkGPT-work。旧P0A独立Verifier已冻结于本阶段frozen/，SHA见BEFORE_BINARY_DEPENDENCY_SHA.json。原P0A完整proof/statement见reference/frozen-oneblock/。

收到单独一次明确授权后：先只读核对本候选源码/二进制/依赖/全部参数身份，再做最新P24/V8 cgroup/RLIMIT/PID/容量门禁；CPU0..3，保留32GiB有效可用内存和64GiB磁盘，原P1500/V1200超时。用户已批准oneblock-only其他用户大型任务WARN，不终止修改他人进程，不更改正式full12门禁。输出使用全新单次leaf且不覆盖原件，遇错即停，不重试、不等待自动启动。

1. 新生产Prover生成真实R5B oneblock文件，独立记录wall/user/system/峰值RSS。
2. 与冻结statement/proof逐字节比较并记录尺寸和SHA；任何不兼容停止。
3. 新独立Verifier接受新proof；冻结P0A独立Verifier接受新proof；新Verifier接受冻结旧proof。
4. 必要真实产物篡改应全部REJECT，拒绝必须来自验证而非usage错误。
5. 保持生产wall与诊断kernel计时分开；不把不同运行时间差/函数比值等同端到端加速。若需轻量诊断只允许kernel计时/计数，协议路径不变，输出必须与生产字节一致。
6. 完整证据、配置、身份、proof/statement、日志与资源/频率采样归档Windows并SHA恢复读取验证。无full12、P1、permit或旧scope转移。
''')
(d/'progress.md').write_text((d/'progress.md').read_text()+'\nCompleted all authorized small-development tasks, read-only review minor tests corrected and3902assertions release/ASan/UBSan PASS. Correctedsmallcross56runs expected,4ACCEPT52REJECT. RealoneblockNOTAUTHORIZED; nofull12/P1/permit/scopeinheritance. Finalidentities/protectedhistoryverified; Windowsarchiveverificationpending.\n')
print(json.dumps({'status':result['status'],'source_after':tree,'kernel_before_s':baseline['kernel_wall_s'],'kernel_after_median_s':median,'small_cross_accepts':4,'small_negative_rejects':52,'changes':changed},indent=2),flush=True)
