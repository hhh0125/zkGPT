from pathlib import Path
import os,sys,subprocess,shutil,json,hashlib,csv,re,statistics,tarfile,datetime
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(r);os.sched_setaffinity(0,[0,1,2,3]);d=r/'reports/PERF-P0A-20261021';logs=r/'logs/PERF-P0A-20261021'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def dump(n,x):(d/n).write_text(json.dumps(x,indent=2,ensure_ascii=False)+'\n')
ns={};exec(compile(Path('reports/R6A/helper.py').read_text(),'limiter','exec'),ns);ns['r']=d;ns['logs']=logs
ns['run']('20-final-focused-counts',[str(r/'build/release/src/paper_r1/p0a_g1_validation_tests')],timeout=90,mem_gib=8,disk_gib=64,reserve_gib=32)
sys.path.insert(0,str(r/'reports/R6B-OPT2-20261021'));import r6b_resources
try:r6b_resources.identity_check({'root':str(r),'identity_files':{},'source_sha':(d/'BEFORE_SOURCE_TREE_SHA.txt').read_text().strip()})
except Exception as e:
 assert str(e)=='current source tree changed';dump('OLD_SOURCE_IDENTITY_REJECTED.json',{'old_source_identity_check':'REJECTED','reason':str(e),'historical_scope_edited':False,'permit_generated':False,'approval_flow_executed':False})
else:raise AssertionError('old source identity unexpectedly accepted')
dump('DRIVER_DEPENDENCY_SHA.json',{str(p):sha(p) for p in [Path('reports/R6A/helper.py'),Path('reports/R6B-OPT2-20261021/r6b_resources.py'),Path('reports/R6B-OPT2-20261021/r6b_common.py')]})
p=subprocess.run([sys.executable,str(d/'run_oneblock_when_gate_passes.py')],capture_output=True,text=True,timeout=30);(logs/'21-oneblock-default-gate-only.log').write_text(p.stdout+p.stderr);assert p.returncode==3 and 'unapproved visible competing large job' in p.stdout
reference=d/'reference';reference.mkdir()
historical=[]
for stage,names in [('R6B-OPT2-20261021',['final-oneblock-prover','final-oneblock-independent']),('PERF-AUDIT-20261021',['audit-prover','audit-verifier'])]:
 for name in names:
  meta=Path('reports')/stage/(name+'.json');row=json.loads(meta.read_text());dest=reference/stage;dest.mkdir(exist_ok=True);shutil.copy2(meta,dest/meta.name)
  for kind in ['log','time']:shutil.copy2(row[kind],dest/Path(row[kind]).name)
  tim=Path(row['time']).read_text();historical.append({'workload':'R5B_oneblock','variant':'historical_full' if stage.startswith('R6B') else 'historical_diagnostic','role':'prover' if 'prover' in name else 'verifier','status':'HISTORICAL_NOT_RERUN','supervisor_wall_s':row['seconds'],'time_wall':re.search(r'Elapsed .*: (\S+)',tim).group(1),'user_cpu_s':re.search(r'User time .*: ([\d.]+)',tim).group(1),'system_cpu_s':re.search(r'System time .*: ([\d.]+)',tim).group(1),'peak_RSS_KiB':re.search(r'Maximum resident set size .*: (\d+)',tim).group(1),'group_peak_observed_bytes':row['observed_process_group_peak_bytes'],'binary_sha256':row['binary_sha256'],'source':str(meta)})
for mode in ['before_matched_diagnostic','after_matched_diagnostic','after_production']:
 for role in ['prover','verifier']:historical.append({'workload':'R5B_oneblock','variant':mode,'role':role,'status':'NOT_RUN_RESOURCE_GATE_REFUSED'})
with (d/'ONEBLOCK_BEFORE_AFTER_PERFORMANCE.csv').open('w') as f:
 w=csv.DictWriter(f,fieldnames=list(historical[0]));w.writeheader();w.writerows(historical)
for name in ['model.proof','model.statement']:
 src=r/'reports/R6B-OPT2-20261021/final-oneblock'/name;dest=reference/'frozen-oneblock';dest.mkdir(exist_ok=True);shutil.copy2(src,dest/name)
proofs={str(p.relative_to(d)):{'bytes':p.stat().st_size,'sha256':sha(p)} for p in [*reference.glob('frozen-oneblock/*'),*d.glob('compatibility/*/valid.*')]}
dump('PROOF_STATEMENT_IDENTITY.json',{'artifacts':proofs,'small_new_old_byte_comparison':'IDENTICAL','real_oneblock_new_proof':'NOT_GENERATED_RESOURCE_GATE_REFUSED','real_cross_verification':'NOT_RUN'})
rows=list(csv.DictReader((d/'MICRO_FUNCTION_TIME_BREAKDOWN.csv').open()));med={}
for name in ['shape','encode','decodeG1']:
 med[name]={mode:statistics.median(float(x['exclusive']) for x in rows if x['function']==name and x['mode']==mode) for mode in ['before','after']}
env=json.loads((d/'ENVIRONMENT_FINAL.json').read_text());freq=[v for s in env['frequency_load_samples'] for v in s['frequency_kHz'].values()]
small=json.loads((d/'SMALL_COMPATIBILITY_RESULTS.json').read_text());focused=(logs/'20-final-focused-counts.log').read_text();count=int(re.search(r'PASS P0A G1 (\d+) checks',focused).group(1))
tests={'focused_checks':count,'focused_pass':True,'regression_pass':66,'regression_total':66,'regression_wall_s':73.57,'excluded':['r6_builder_full12','r6_finite_threeblock','r6_actual_schema','r6_trust_metadata'],'exclude_reason':'Do not execute full12/multiblock construction; these schema/metadata paths are outside G1-only change. Exclusion does not imply failures.','small_cross_positive':4,'small_cross_negative_rejections':64,'compatibility_total_operations':len(small['results']),'oneblock_cross_verification':'PENDING_RESOURCE_GATE','review_counterexample_red':'14-review-red-test.log','review_fix_green':'16-review-green-test.log','formal_protocol_security_proof_claimed':False}
dump('TEST_RESULTS.json',tests)
security='''# P0-A 群验证安全边界

只适用于当前冻结 MCL BN254 **G1**。这是 MCL 的 b=2 曲线，不是 Ethereum BN_SNARK1 的 b=3 曲线。运行时逐次检查 curve ID、p、Fr 模数、G1 order、a=0、b=2、specialA=Zero、specialB=GenericB、Jacobi 模式、非 ETH 序列化和 32 字节长度。任一不匹配即保留旧式 `isValid() && isValidOrder()` 路径。未改变 MCL verifyOrder 标志、断言或编译检查。

u=-4647714815446351873；p=16798108731015832284940804142231733909889187121439069848933715426072753864723；r=16798108731015832284940804142231733909759579603404752749028378864165570215949。

E: y²=x³+2 over Fp。BN 参数给出 p=36u⁴+36u³+24u²+6u+1，r=36u⁴+36u³+18u²+6u+1，trace=6u²+1，#E(Fp)=p+1-trace=r，cofactor=1。另以固定非零点 P=(-1,1) 的未约减 generic [r]P=O 作独立运行检查。给定已公布的素数 p/r，Lagrange 定理得 r | #E，Hasse 区间内唯一 r 的正倍数为 r，故 #E=r。这避免只凭曲线名称或标量域相等推断 cofactor。CURVE_ARITHMETIC.json 给出整数检查；SymPy 的 >64-bit primality 检查只是对公布素数的交叉检查，不宣称生成了形式化素性证书。参数来源见冻结 `3rd/mcl/include/mcl/curve_type.h`、`bn.hpp`，以及 [MCL API](https://github.com/herumi/mcl/blob/master/api.md)。

因此，对**当前参数下字段合法且在 E 上**的 G1 对象，r 倍为无穷远已由曲线群阶蕴含；独立 order 乘法不再增加判定信息。有效点的加、减、标量乘和 MSM 结果仍在群内。实现比单纯信任闭包更保守：每次 encode、shape、生成元检查都重查当前对象的 x/y/z 字段合法性和 MCL 原 Jacobi 曲线方程。不存在对象地址缓存、validated 标志、跨角色授权或“只检查一次”的不可变性假设；对象变化、释放后地址复用都重新检查。仅 p/r 数学常量静态保存，曲线状态每次读取。

外部字节必须长度准确、严格字段坐标范围、平方根存在及正确 parity、on-curve、规范 encode roundtrip。无穷远使用原全零编码；无效 Jacobian 无穷远拒绝；可合法表示同一无穷远的非标准 Jacobian 与原编码一致。禁止非规范坐标、trailing/truncated 字节及破坏的群点。当前 G1 cofactor1 使外部输入的 subgroup 条件同样由 on-curve 蕴含；不是把外部输入当可信内部对象。

G2 未修改：测试实际找到 on-curve 非 subgroup G2 并保留拒绝。BN_SNARK1 使用旧严格 order 路径；当前 256-bit 构建拒绝 BLS12_381 初始化。不得把本结论推广到任意 G2/BLS12 或其他 b/p/r 配置。MCL 是全局曲线配置；仍要求已有工程前提：线程启动前完成初始化，不支持并发重初始化或任意写坏底层 MCL 运算表。无新增缓存的并发失效问题，也不宣称对全局状态数据竞争提供保护。

PCSpec、role、尺寸、生成元非零、Sumcheck、Lasso、IPA/opening、W 绑定和独立验证均未删减。一个“在曲线上但内容错误”的承诺仍须通过原协议关系验证；通过群元素合法性不等于通过证明。
'''
(d/'SECURITY_BOUNDARY.md').write_text(security)
review='''# 独立审查与修正

按 requesting-code-review skill 由全新只读 reviewer `/root/p0a_security_review` 审查初始补丁和测试，不向其提供完整会话历史。无 Critical；发现 Important：仅匹配 a_/b_ 不足以固定 MCL 曲线方程分派。specialB_=Plus4 时 (0,2,1) 属于 b=4 方程且阶为 3；旧严格 order 拒绝，初始快速路径误接受。已先加入反例并记录 RED（14-review-red-test.log），再要求 specialA=Zero/specialB=GenericB，记录 GREEN 及最终 66 项回归。

审查另指出显式表示边界覆盖不足，原为 Minor，按本轮 codec 安全要求提升为 Important 并同次修复：signed-zero、内部字段超范围、合法/非法 Jacobian 无穷远、非归一化 projective 点以及确保 nonsquare 样例实际找到。修复后 focused 与回归均通过。初始和修复后微基准分目录保存，最终报告只采用 measurement-final。

审查没有证明整个原协议安全、没有完成真实 oneblock 或本地归档核查，也没有认可并发 MCL 重初始化；这些范围分别标为待门禁、由主代理独立核验、已有工程前提。没有把审查意见当成真实运行证据。
'''
(d/'INDEPENDENT_REVIEW_AND_FIXES.md').write_text(review)
report=f'''# zkGPT PERF-P0A 结果（2026-10-21）

**状态：源码实现与小规模验证完成；真实 R5B oneblock 验收和整体加速测量未完成，因既有资源门禁拒绝。不能据此宣布 P0-A 已通过真实工作负载验收。** 本轮停止于 P0-A，不实施 P0-B，不启动 full12，不生成 permit，不进入 A/B/C/D。

## 改动和身份

只改 `src/paper_r1/core.cpp/core.hpp/pc.cpp`，另增测试文件和 CMake 测试目标，共 5 个文件。见 P0A_SOURCE.patch、AFTER_SOURCE_SHA.json、NEW_IDENTITY_SHA.json。没有 Profile/模型/公开输入/Exp/关系变更；126 项冻结参数与 Profile 身份、{len(json.loads((d/'PROTECTED_APPROVAL_ARTIFACT_SHA.json').read_text()))} 项历史 scope/proof/statement/permit 等保护文件 SHA 全部未变。没有 git reset/clean/merge，没有 sibling 源码副本；原目录未删除。

当前 MCL 的 `isValid()` 和 compressed `deserialize()` 自身会执行 order 检查，故旧 encode 非零点实际 2 次、旧 decode 实际 5 次。不能只删显式调用。新 helper 严格限定 BN254 G1 cofactor1 条件，每次重查对象字段与曲线方程，encode 保留 MCL 原 serialize；decode 保留相同压缩格式、严格字段 decoder、MCL getYfromX 及 canonical roundtrip，避开数学冗余的 r 乘法。无对象缓存，无 validated 生命周期或跨角色复用状态。详细论证见 SECURITY_BOUNDARY.md。

源码树新 SHA：`{json.loads((d/'NEW_IDENTITY_SHA.json').read_text())['source_tree_sha256']}`。新 R5B Prover：`{json.loads((d/'NEW_IDENTITY_SHA.json').read_text())['production_binaries']['build/release/src/paper_r2/r5b_model_prove']['sha256']}`；Verifier：`{json.loads((d/'NEW_IDENTITY_SHA.json').read_text())['production_binaries']['build/release/src/paper_r2/r5b_model_verify']['sha256']}`。旧源码 identity 检查已明确拒绝新树；没有改写旧 scope 或把批准转移到新二进制。R6 二进制因链接共同库也有新身份，不能沿用旧 A 阶段 scope。

## 正确性证据

- 最终 focused 测试 {count} 个检查通过，包含规范编码差分、正常点/无穷远/边界、非规范与 off-curve、同地址突变/复用、错误承诺/角色/opening、G2 与其他曲线隔离，以及 FS digest/challenge 一致性。
- 最终相关回归 66/66 通过，73.57s。明确排除 r6_builder_full12、r6_finite_threeblock、r6_actual_schema、r6_trust_metadata；没有把未运行项目记为通过。保留 r6_builder_oneblock、R5B trusted W、独立 verifier、Sumcheck/Lasso/codec/恶意样例测试。
- 独立的小型 Range+indexed Exp+mapped-input（不是 GPT oneblock）由冻结旧库及新库分别生成，proof/statement 逐字节一致。旧/新独立 verifier × 旧/新 valid proof 共 4 次接受；16 类负向样例 × 4 组共 64 次拒绝，包含损坏 opening、commitment、Sumcheck、Exp memory、模型上下文、映射、padding、truncated/trailing 等。原始日志及全部样例字节在 compatibility/ 与 logs/compatibility/。
- 审查发现 specialB dispatch 的真实反例，先失败再修复；最终限定 specialA/specialB。见 INDEPENDENT_REVIEW_AND_FIXES.md。测试通过不替代真实 oneblock 的下述待办。

## 性能：只报告实际测得的结果

最终等价插桩微基准，各跑 4 次，4 逻辑 CPU affinity=0–3；单进程主串行工作，8 GiB 测试上限，64 GiB 磁盘/32 GiB 共享内存预留。固定 64 点、2000 encode、2000 decode、200×64-row shape+append，包含初始化 commit 的额外编码统计。下表是每进程总 exclusive 秒的中位数，包含插桩开销；decode exclusive 扣除嵌套 encode。不是每次调用的延迟，也不是整体 Prover/Verifier 加速。

| 函数 | 调用数/进程 | 旧 exclusive 中位秒 | 新 exclusive 中位秒 |
|---|---:|---:|---:|
| shape | 200 | {med['shape']['before']:.6f} | {med['shape']['after']:.6f} |
| encodeG1（日志名 encode） | 16864 | {med['encode']['before']:.6f} | {med['encode']['after']:.6f} |
| decodeG1 | 2000 | {med['decodeG1']['before']:.6f} | {med['decodeG1']['after']:.6f} |

每进程旧 actual_order=65,458，新=0；新 logical_order_skipped=65,458（按旧调用图等价冗余检查次数计数），fast_points=31,729，fast_decodes=2,000。三段循环的旧 order 次数分别为 4,000、10,000、51,200，余 258 来自初始化 commit。新逐点 on-curve helper 成功次数与 fast_points 一致，仍包含 shape 与编码的重复方程检查。没有删除这些廉价检查。完整 4 次 raw wall/CPU/RSS、inclusive/exclusive、调用与跳过次数见 MICRO_FUNCTION_TIME_BREAKDOWN.csv 及 measurement-final/ 原始日志。

微基准 /usr/bin/time 进程 wall：旧 3.41/4.00/4.39/3.85s，新 0.18/0.18/0.12/0.18s；user CPU、system CPU 和 RSS 逐次列于 CSV。短运行 RSS_group 采样会漏掉峰值，不能当真实峰值；以 time -v 的 ru_maxrss 为主。原资源 supervisor elapsed 含约 0.25s 轮询开销，单列，不能与进程 wall 混用。初始 pre-review 诊断证据另存，不混入本表。

服务器为 Xeon Platinum 8352V，既有 Release -O3 -DNDEBUG 配置未改，不新增关闭检查的选项。第 4 组采到的 sysfs 频率范围 {min(freq) if freq else 'UNAVAILABLE'}–{max(freq) if freq else 'UNAVAILABLE'} kHz；只是瞬时报告，不保证有效频率或独占。共享竞争存在，四次耗时有变化；历史运行未采集匹配频率，无法剥离频率/竞争影响。ENVIRONMENT_FINAL.json 留存时间戳、负载、CPU/内核；最终二进制与依赖身份分别记录。

历史原始完整 oneblock P=463.373s、V=308.094s；历史诊断 P=427.956s、V=298.330s **分别保留**，附旧 time-v CPU/RSS 与日志。不能相减当作 P0-A 收益。ONEBLOCK_BEFORE_AFTER_PERFORMANCE.csv 的所有新真实运行列为 NOT_RUN_RESOURCE_GATE_REFUSED，未填造时间或峰值。PROOF_STATEMENT_IDENTITY.json 给出旧真实 proof/statement 与小型新旧样例 bytes/SHA；未生成新的真实 oneblock proof，故未声称真实文件逐字节相同。

## 资源门禁和剩余验收

INITIAL_RESOURCE_GATE、RESOURCE_GATE_AFTER_REVIEW、RESOURCE_GATE_FINAL 及默认待执行脚本的 gate-only 日志均保留。最新观测：UID1010 的 PID3694800 python3，RSS=42,101,026,816 bytes，约39.2 GiB。尽管可用内存和磁盘充裕，既有 r6b_resources.resource_check 仍以 `unapproved visible competing large job` 拒绝 P24/V8；allowed_competing=[]，未豁免、杀进程或改门禁。

已准备 run_oneblock_when_gate_passes.py，默认只检查 P24/V8 门禁；`--execute-oneblock` 才会在固定目录、每步新鲜 gate 通过后，单次串行运行等价插桩前后对照与新生产 P/V，并做旧独立 V 接受新证明、新 V 接受冻结证明、真实 proof/statement 与既有原件逐字节比较。限定4CPU、P24/V8、64/32预留及原 oneblock timeout，失败即停不重试、不生成任何 scope/permit，不访问 full12 执行入口。真实执行尚未发生，后续仅在门禁允许时执行，不能仅凭本报告认定验收通过。

**结论：P0-A 已在受测 G1 路径消除重复 order 乘法，且小型字节/FS/负向兼容性通过；真实整体加速和既有 GPT oneblock 的三向独立验收仍待门禁。当前不推进 P0-B 或 full12。**
'''
(d/'P0A_REPORT.md').write_text(report)
(d/'RESTORE_P0A.md').write_text('''# Windows 归档恢复说明

远端唯一源码目录为 /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work。不要在旧 zkGPT 或新 sibling 树恢复源码；不要覆盖历史 proof/statement/scope。归档包含修改前完整源码、最终完整源码、源补丁、before/after 二进制和库、配置/参数身份、120 份参数字节、原始测试/负向/测量日志、最小 statement/proof 及旧真实 oneblock statement/proof。

用 EVIDENCE_MANIFEST.json 逐文件 SHA 校验；before-source.tar.gz 的逐文件身份见 BEFORE_SOURCE_SHA.json，after-source.tar.gz 见 AFTER_SOURCE_SHA.json。解压做 restore-read 不代表批准运行；新二进制没有继承旧 scope，未生成 permit。所有路径均为归档相对路径，不含 SSH 配置、私钥或凭据。远端证据没有删除。本地 WINDOWS_ARCHIVE_VERIFICATION.json 单独记录 SHA/成员恢复读取结果，不能把它当真实 oneblock 验收。
''')
with (d/'progress.md').open('a') as f:f.write(f'Final: focused {count} checks PASS; final relevant CTest66/66 PASS; small old/new cross byte equality and4 accepts/64rejects PASS. Final micro4 paired repetitions archived. Real oneblock blocked by unapproved competitor, no full workload launched. Historical scope/parameters rechecked unchanged; source identity rejects old gate; no permit/full12/P0B. Windows archive verification pending.\n')
print('REPORTS WRITTEN',count,'focused checks',med,flush=True)
