from analyze import *
def md(name,text):(OUT/name).write_text(text.strip()+'\n',encoding='utf-8')
def table(headers,rows):return '\n'.join(['| '+' | '.join(headers)+' |','|'+'|'.join(['---']*len(headers))+'|']+['| '+' | '.join(map(str,r))+' |'for r in rows])
def fmt(v):return f'{v:.6f}'if isinstance(v,float)else str(v)
def main():
 d=read(OUT/'DERIVED_SUMMARY.json');allrec={(s,p):read(OUT/f'parsed/{s}-{p}.json')for s in ['full12','oneblock']for p in ['P','V']}
 def x(s,p,t):return one(allrec[s,p],t)
 def source(n):return 'src/paper_r7/'+n
 phase=[]
 for ph in x('full12','P','CENSUS')['phases']:
  v=next(z for z in x('full12','V','CENSUS')['phases']if z['name']==ph['name'])
  phase.append([ph['name'],fmt(ph['seconds']),fmt(v['seconds']),f'{100*ph["seconds"]/d["full12-P"]["process_wall"]:.2f}%',f'{100*v["seconds"]/d["full12-V"]["process_wall"]:.2f}%'])
 phase.append(['命名阶段外差额',fmt(d['full12-P']['phase_remainder']),fmt(d['full12-V']['phase_remainder']),'2.97%','9.81%'])
 md('PERFORMANCE_EXECUTIVE_SUMMARY.md',f'''
# R7 Compact Independent v3 性能分析

报告日期：2026-11-05（Asia/Shanghai）。这是现有真实运行的证据分析，未重新执行证明、验证、攻击测试或 benchmark。未修改任何产品源码、协议或发布状态；`may_enter_stage2=false`。

**主要发现：Full12 P 的首要实测耗时是 Protocol1 2391.090 秒和 Native 1890.690 秒；Protocol1 的非 Compact Opening 差额现已基本解释。V 的慢点主要是 claim 映射准备和终点 K 构造/折叠，而非 31 轮消息处理。A.3 的巨大直接生成元折叠工作未包含在 MSM wrapper 计时中。Native 子协议与 A.3 内部操作仍缺独立时间，不能宣称已完成全部函数的实测归因。**

## 全程时间去向（问题 1、2）

Full12 RESULT 实测 P wall `{d['full12-P']['process_wall']:.9f}` 秒，CPU `{d['full12-P']['process_cpu']:.6f}` 秒；V wall `{d['full12-V']['process_wall']:.9f}` 秒，CPU `{d['full12-V']['process_cpu']:.6f}` 秒。CPU 是 user+system；不能与 wall 相加。

{table(['阶段','P wall 秒','V wall 秒','P 占全程','V 占全程'],phase)}

命名阶段外包含 native plan/statement/layout 初始化、生成元身份计算、FS 准备、最终 ledger/FS 和退出清理等。源码给出候选调用路径，但没有独立时间；181.225070/110.486506 秒不是“纯 I/O”或“纯生成元生成”。census 的 `seconds`（P 6106.51/V 1125.85）与外部进程 wall 是不同计时边界。

## 两个关键差额（问题 3、4）

`COMPACT_QUERY` 是 mapped Opening 内层计时，`UNIFIED_OPENING` 是更大的外层包络。用户提供的差额减的是前者。

| Full12 非 Compact Opening 的互斥组成 | P wall 秒 | V wall 秒 | 证据类型 |
|---|---:|---:|---|
| prepareClaim 映射/权重/偏置/冻结 | 358.118000 | 293.653000 | OPT3A_AUTH.prepare_wall_seconds |
| unified::reduce（含终点） | 1373.970000 | 105.312000 | UNIFIED_SUMCHECK |
| 外层 Opening 超出 COMPACT_QUERY | 3.603000 | 3.345600 | 差额 |
| layout、聚合、缓存析构等未单独计时部分 | 4.993000 | 6.662700 | 差额 |
| **合计** | **1740.684000** | **408.973300** | 父子边界核对 |

P Sumcheck 1373.970 秒进一步拆为：U 扫描/收缩 652.305500，U-K 乘积 200.387181，K 初建/回退重建 355.983000，K 折叠（含其文件 I/O）129.443000，其他未分配约 35.851319 秒。上述是互斥计时区域求和；K I/O 55.3904 秒已嵌在这些区域内，不能再次相加。

V 的 31 轮消息时间合计只有 0.223719 秒；含终点的 105.312 秒中，45 次 kernel 构造共 79.196865 秒，余约 25.891416 秒是终点折叠/组合和 setup/terminal 的未分离差额。不能把它全部叫“每轮 Sumcheck”或全部叫纯 fold。

## 其余必须回答的问题

5. **Native 哪个子协议最慢：尚不能实测排名。** 原日志只有 P 1890.690/V 431.892 秒整体。证明结构确认 13 个顶层 Range、1 个顶层 Exp，Exp 内另有 4 个 Range；合计 104 次 Memory/Lasso、312 次 Product。Lasso 是 hiddenMemory 的实现身份，不能作为第五个独立加总项。没有各子协议/各 block 时间，CSV 保留 NOT_MEASURED。

6. **IPA 513.015 秒花在哪里：** 已确认是 148 次 prooflog 的 inclusive 总 wall，包含向量复制、交叉内积、左右 MSM、逐元素生成元/标量折叠、挑战、序列化/校验及最终关系。源码和帧验证出其中 9,699,032 次直接生成元标量乘和 4,849,516 次生成元加法；各部分实际秒数仍 NOT_MEASURED，不能把 513.015 秒全部归到 MSM 或直接标量乘。

7. **可复用工作：** generator 已按尺寸缓存，但返回副本；每 term 又复制 `g=bases`、`aa=a`，每轮新建左右标量/生成元向量。可研究 immutable 基缓存、scratch 复用、切片视图和公共系数子结构复用。各 term 的转录上下文和挑战不同，挑战相关的 generator fold/weights 不能直接跨 term 共享；不合并或删除 Knowledge Guard。

8. **全部 MSM 时间占比：未知。** 可测的 Full12 wrapper CPU 为 P 166.488/V 80.5799 秒，分别约为进程 CPU 的 {166.488/d['full12-P']['process_cpu']*100:.2f}%/{80.5799/d['full12-V']['process_cpu']*100:.2f}%。P 排除了 commitment worker 的 697,472 次 MSM（714,211,328 terms），也排除了直接标量乘；V 调用/terms census 完整吻合 wrapper，但仍没有 MSM wall。不能用 CPU/wall 比值声称 wall 占比。

9. **内存访问是否主要瓶颈：** 反复 U 扫描确有 652.3055 秒实测，是大开销；运行计数公式记录约 708.536 GB 的 U 字节访问量。它不是硬件测得 DRAM traffic，扫描也包括 Fr 乘加、线程同步、补零访问等。无 PMU/cache miss/NUMA/DRAM 带宽证据，不能认定 bandwidth-bound。

10. **下一轮优先函数：** `quadraticRoundStreaming/Weights::dot`、`Commitments::prepareClaim/kernel`、`PublicK::Impl::with/advance`、`hyrax_a3::prooflog` 和 `single::fold/readRow`。Native 的 `hiddenRange/hiddenExp/hiddenMemory/hiddenProduct/expressionSumcheck` 先补互斥计时再决定具体优化点。见 OPTIMIZATION_OPPORTUNITIES.md，所有收益数字保持未知。

11. **实证：** 进程/阶段 wall、Full12 阶段 CPU、逐轮时间、K I/O API 字节及时间、缓存构建数、A.3 两大阶段、wrapper CPU bins、RSS、proof 大小与帧结构、当前 Windows/Linux 源码 SHA 一致。未把旧验证日志当作本轮新验收。

12. **结构推导/未知：** IPA 基折叠操作数来自源码公式与帧维度，候选重复复制来自源码；Native 内部分摊、IPA 热点独占秒数、全部 MSM wall、cache/DRAM/NUMA 因果和优化收益未知。

## 历史同口径对比

HISTORICAL_COMPARISON.csv 重新解析旧原始日志及 RESULT/P.json，统一使用外部进程 wall；原“Single 9703.100”是历史展示精度，原 RESULT 实值为 9703.068977 秒。Full12 Compact 相对 Single：P wall 降约 {100*(1-d['full12-P']['process_wall']/9703.068977412768):.2f}%，V 降约 {100*(1-d['full12-V']['process_wall']/2572.198366705328):.2f}%，proof 从 325,337,395 增至 526,435,844 字节（约 +61.81%）。Oneblock P 反而由 910.311857 增至 1050.861347 秒，不能只报告 Full12 改善。

共享服务器、不同日期/负载，不是受控加速比。Balanced 与 Single 的外部模型 Anchor 声明不同，表中的时间不建立安全等价。完整命令、线程、affinity、load、二进制/依赖/参数身份在 raw/*/ENVIRONMENT.json、P/V.json、IDENTITY_BEFORE/AFTER.json。

## 复查入口

- 全量计时记录：EXISTING_TIMING_INVENTORY.csv（每个 timer 含 log 行号）；逐轮和逐 root 原 JSON：parsed/。
- 原始证据副本：raw/；历史对照：historical/；源码：ANALYZED_SOURCE.zip 及 SOURCE_RESTORE_READ.json。
- 当前源码已逐文件与 Linux SHA 比较，并与运行时 IDENTITY_BEFORE 的重合文件比较；proof/statement 已在 Windows 逐字节 SHA 核验。Linux Git 的祖先 main 并不等于这份未跟踪快照的身份。
- 限制与后续测量边界：MEASUREMENT_LIMITATIONS.md；完整变化清单：CHANGE_MANIFEST.md。
''')
 md('PROTOCOL1_BOTTLENECK.md','''
# Protocol1 调用链与互斥分解

真实入口：classified_controller.inc:106 → Commitments::authenticateUnified，classified_commitments.cpp:165-211。

顺序为 layout/statement 绑定 → 1502 次 prepareClaim → rho/initial 聚合 → PublicK 构造及 sealed storage 身份检查 → unified::reduce → PublicK report/reset → single::open → 完成绑定。仅 P 拥有 K cache 和 U 私有源；V 通过 kernel_at(point) 独立构造终点，不接触 sealed U。

prepareClaim 的 timer 在进入函数时开始，在循环、digest 比较、Builder::finish、权重缓存、OPT3B_PLAN 输出后停止（classified_commitments.cpp:38-52）。因此 358.118/293.653 秒是该循环内的直接 wall；不是全程序 OPT2_AUTH.map_cpu_seconds。

UNIFIED_SUMCHECK（unified_input.cpp:70-96）包住所有 round 和终点。P 每轮 quadraticRoundStreaming → exchangeHidden（三个系数）→ recurrence Sigma → coin → PublicK::advance → initial handle combination。最后 P 使用 PublicK::endpoint；V 调用 kernel_at：每 root kernel + fold(K,q) + selector。

COMPACT_QUERY 从 single_hyrax.cpp:149 附近的 identity/binding 开始；它前面的 validatePublic/validateSecret 没有计入该内部 timer。外层 UNIFIED_OPENING 在 singlePlan/descriptor 再验证之前开始，因此二者相差 P 3.603/V 3.3456 秒，不是同名的 Opening timer。内部 preparation 只计 fold(p,t,s)，不包含 mappedQueryTerms 生成、A3 statement、guard/core 或 stitch。

Full12 可复查等式（秒，原日志精度）：

```
P phase 2391.090 = 358.118 + 1373.970 + 654.009 + 4.993
P non-COMPACT 1740.684 = 358.118 + 1373.970 + (654.009-650.406) + 4.993
P reduce 1373.970 = 652.305500 + 200.387181 + 355.983 + 129.443 + 35.851319
V phase 485.077 = 293.653 + 105.312 + 79.4493 + 6.6627
V non-COMPACT 408.9733 = 293.653 + 105.312 + (79.4493-76.1037) + 6.6627
V reduce 105.312 = 0.223719 + 79.196865 + 25.891416
```

P 最后一个残差不是纯 Sumcheck arithmetic，也不是纯 I/O：包含初始 cache 文件落盘、部分 provider bookkeeping/校验、setup、Sigma、输出计时日志等。K_io_wall_seconds 有部分位于 K advance 和 U-K callback 内，因此不能再加到等式右侧。

31 轮 P 时间合计 1373.9623 秒；总 reduce 减之约 0.0077 秒只是舍入后差额，不能据此宣称精确的终点函数延迟。V round 合计 0.22371918 秒，剩余终点显著昂贵。Oneblock 的同类等式及全部 nested timers 在 PROTOCOL1_BREAKDOWN.csv；四个 disjoint_top_partition 行才允许直接合计。

P 第 0/1/2 轮分别 273.315/268.345/212.437 秒；后续随着 K 状态缩小下降，但 U 保持逐轮重扫。完整 31/27 轮均已导出，未给无 timer 的单独 challenge/folding 发明时间。
''')
 krows=[];kernels=[]
 for s in ['full12','oneblock']:
  for p in ['P','V']:
   seen=set();first=repeat=0
   for r in tag(allrec[s,p],'OPT2_KERNEL'):
    q=r['data'];rebuild=q['root']in seen;seen.add(q['root']);first+=0 if rebuild else q['wall_seconds'];repeat+=q['wall_seconds']if rebuild else 0
    kernels.append(dict(scope=s,process=p,root=q['root'],visit='fallback_rebuild'if rebuild else'first_build',**{k:v for k,v in q.items()if k!='root'},measurement_source=ref(s,p,r),measurement_kind='DIRECT_TIMER_WITH_SEQUENCE_CLASSIFICATION'))
   z=d[s+'-'+p];k=z['K']
   krows.append([s,p,fmt(first),fmt(repeat),k['K_initial_builds']if k else'无 P cache',k['fallback_rebuild_calls']if k else'不适用'])
 csvout('KERNEL_CALL_PROFILE.csv',kernels)
 md('PUBLIC_K_PROFILE.md',f'''
# Public K

源码：public_k.cpp:35-49 File，52-109 PublicK::Impl；classified_commitments.cpp:185-189 builder/endpoint。

{table(['scope','进程','首次 kernel wall 合计','回退 kernel wall 合计','K_initial_builds','fallback_rebuild_calls'],krows)}

KERNEL_CALL_PROFILE.csv 保留每次 root kernel 的 wall/CPU/coordinates 和原日志行号，first/rebuild 按同 root 的出现次序分类。P Full12 115 次非公共 kernel：45 个首次 +70 次重建；K_initial_builds=116 另含公共 X 的全零 K。首次 kernel 132.530580 秒、重建 223.437170 秒。K_initial_build_seconds 355.983 秒比 kernel timer 总和稍多，因为包含 builder 返回/长度检查/公共零根等。

Full12 K_fold_wall 129.443 秒（CPU126.660），fold_coordinates 2,109,804,452，state_read_coordinates 1,447,104,813；I/O API read 36,775,662,720/write 18,387,827,424 字节，累计 I/O timer55.3904 秒。RAM/file admissions5/41 是累计入缓存次数，不是全程并发根数；rebuild_root_visits70 与 fallback 重建次数吻合。缓存预算 RAM512MiB/file4GiB，file peak4GiB。K_RAM_peak=1,010,259,296 字节包括临时 builder/metadata/scratch，不能与512MiB cache payload budget直接比较判定越界。

Oneblock 初建7 次，无 fallback；K_fold_wall19.0855 秒，I/O read8,589,934,592/write4,294,967,168 字节，I/O11.9627 秒。K builder峰值512MiB，RAM计数峰值672,928,504 字节，file峰值2GiB。

Impl::with 初建后按当前 prefix 折叠，再尝试 RAM、O_TMPFILE 和 fallback；大 buffer 在复制缩短后可能与原 builder 暂时共存。Impl::advance 在挑战后原地折叠 RAM/file 状态，文件路径包括 checksum、pread/pwrite、fdatasync、posix_fadvise。单独 initial-build 和 fallback-build 的精确 PublicK timer 未分开，但 kernel 序列允许上述实测子集分类。K 折叠 CPU/wall 包含文件开销；K I/O 是重叠子集，不重复加总。

V 无 K cache，不执行 advance。V 的 45 次 kernel79.196865 秒发生在 reduce 的终点，随后 fold、selector 和聚合；其余约25.891416 秒不能全部命名纯 fold。Full12 OPT2_AUTH.kernel 355.956/78.9718 是 CPU；OPT3A_AUTH.kernel 355.968/79.1968 是 wall（输出精度导致与逐调用求和末位有差异）。

候选：更有效的缓存准入/预算与重建避免、同样系数的只读结构缓存；必须保留 session/order/checksum 验证及资源边界。未更改预算或数值参数，也未测得提高预算的收益。
''')
 md('MAPPING_PROFILE.md','''
# Claim 映射与逻辑/物理查询

两种映射应分开：

1. `Commitments::mapDigest/defer/prepareClaim/kernel`（classified_commitments.cpp:38-68、142、165-195）认证原始 logical view。Full12 claims1502，map_calls3004，map_coordinates5,254,189,792；defer 和 authentication 各完整映射一次。OPT2_AUTH.map_cpu_seconds=P723.266/V548.035 是这两遍的全程累计 CPU，含前面 GKR/Native/handles 阶段中的 defer，不能从 P1 wall 减掉。OPT3A_AUTH.prepare_wall=P358.118/V293.653 仅 authentication 这一遍。

2. `single::mappedQueryTerms`（single_hyrax.cpp:94-108）把全局 logical MLE 查询分成物理 rank-one terms。按 PhysicalRun 拆 leading/trailing edge 和因 logical-column shift 产生的 carry；Full12148 项，Oneblock19 项。Basis 先生成 low/high 分离权重，避免长度2^31的 coefficient array；logical_coefficient_array_bytes=0。

Full12 OPT3B_PLAN 显示1311项冻结到 RAM、191项冻结到 file，没有 mapping fallback；末条累计 map RAM2,623,560 字节、file782,724,040 字节。Oneblock921/15，RAM462,000/file63,651,800。它们是 mapping plan storage，与 PublicK 的512MiB/4GiB缓存不是同一个对象。当前 unified single 路径早返回前未打印 OPT3B_STORAGE，因此没有完整 mapping file API read bytes/wall。不能拿 plan 文件大小替代 I/O量。

kernel(P)115 次，6503632656 matched/frozen coordinates；kernel(V)45 次，2461511840 coordinates；fallback_map_queries=0。即使 map 已冻结，K fallback 仍会重复累积相同根的聚合系数。不能把“无 map callback fallback”误解成“没有 K 重建”。

mappedQueryTerms 使用 callback streaming；每项分配 left/right（right 长columns，可能大量零）、edge至少重建 right，carry 项共享 low 的数学子结构但仍建独立向量。`single::fold` 对每项分配列 witness、逐行 readRow 清零/拼接、rowCombination；Full12 preparation63.682 秒（V0.468176），包含 MSM和私有fold，不等于 decomposition 单独时间。已有独立 MEASURE.log 记录 full12 mapping generation median0.0826524 秒、identity median3.36205秒（三次重复）；这些是历史 public-data benchmark，不可作为该进程内部排除 callback 的实测秒数来相减。

重复计算候选包括按相同 right 模式缓存不可变系数、scratch容量复用、避免逐 term 复制同尺寸generator、避免反复读相同物理行。必须先比较 exact coefficient/row-range；各项 proof 上下文/挑战不同，不能共享挑战相关折叠，也不能把 public 参数缓存替代 fresh guard。没有在本轮实施这些候选。
''')
 md('NATIVE_PROTOCOL_BOTTLENECK.md','''
# Native 协议：路径、次数与未测范围

整体 Full12 P1890.690/V431.892 秒，Oneblock P658.724/V209.850 秒。时间来自四份原始 census；Full12 CPU P1890.34/V430.821 秒。不能用 P 的单线程近似推导各协议 wall。

真实调用链（oneblock_lookup.cpp）：

```
classified_controller.inc:101
  for(ctx.plan.ranges): hiddenRange(..., prepared=true)             :49
    digit/reconstruction/evaluate
    每个9bit digit: hiddenMemory                                  :39
      complete-table identity + fingerprint + multiset Sigma
      三次 hiddenProduct                                         :23
        product tree build + 每层 expressionSumcheck + leaf auth
    zero-tail expressionSumcheck
  for(ctx.plan.exps): hiddenExp(..., prepared=true)                 :57
    hiddenRange(index,32), hiddenRange(x,13), selector(1), delta(32)
    两个 branch expressionSumcheck
    native-E/value-binding
    hiddenMemory(indexed Exp table) → hiddenProduct ×3
```

`hiddenMemory` 的 statement 是 R7-HIDDEN-LASSO-v1。Memory 与 Lasso 是同一执行调用，Range/Exp 包含它，Product 又是它的子调用。不能将五者inclusive相加。

已读取两份原 proof 的完整帧到 footer/EOF，核验 proof SHA；不运行密码学验证。每个 scope 帧均确认：13顶层Range +1顶层Exp；Exp另调用4Range；104个Memory/Lasso；312个Product（按唯一product role的level0帧）。计数是完整帧与源码对应，不是静态猜测。原始角色、sequence、byte offset 在 parsed/*-proof-structure.json；NATIVE_PROTOCOL_BREAKDOWN.csv 另列14个顶层角色的帧数/字节，但这些字节不能代替耗时。

prepared=true 跳过 hiddenRange/hiddenExp/hiddenMemory 中的部分 witness/helper重建；准备实际在 frozen-witness-and-prechallenge-lookup 的 classified::prepareLookup。把它重新计入 Native 会跨阶段重复。native factory 取 pcs.view，opening 被 defer 到统一认证；此 Native phase 不是即时重复执行148次A3的阶段。

Full12 native role 是全局 r7/r2/model/range/{0..12} 与 r7/r2/model/Exp，不是12组可独立相加的block计时。GKR对12block执行不能推出 Native14个顶层关系也逐block独立计时。BLOCK_LEVEL_TIMINGS.csv 的每block×协议项保留NOT_MEASURED；原日志无法把全局视图恢复成每block时间，不会除以12。

没有各顶层Range/Exp、Memory/Product、polynomial/table/Sigma/serialization的单独timer，所以“哪个子协议最慢”当前未知。源码显示 product tree中保留叶/层向量，重复fingerprint求值、publicMLE/weights和expressionSumcheck折叠都值得测；没有证据可给其独占秒数排序。

后续最小测量：在新独立构建中仅对顶层Range/Exp与nested Memory/Product/expressionSumcheck置线程安全聚合timer，记录inclusive和child-exclusive，保留role/padded/table尺寸；对固定已存Oneblock proof先仅重放V（历史334.249秒、RSS约1.14GiB、4core配置，实际新负载未知），再视需要生成P（历史1050.861秒、RSS约4.80GiB）。本轮选择完成现有证据分析，没有新增源码埋点、没有启动该重放，更没有Full12重跑；此处是后续测量建议。
''')
 md('A3_IPA_HOTSPOTS.md','''
# A.3 guard / IPA

源码 hyrax_a3.cpp:20-34 knowledge，37-68 prooflog，72-80 generators，83-89 dotProduct；single_hyrax.cpp:144-164 mapped open。A3_PROFILE.csv 有 aggregate timer和显式NOT_MEASURED内部项；A3_TERM_LEVEL_METRICS.csv 有每term帧维度/round/call/代码导出操作数，没有把总时间平均分配给term。

|scope|P guard wall|P IPA wall|V guard wall|V IPA wall|terms|每项维度/轮数|
|---|---:|---:|---:|---:|---:|---|
|Full12|65.3161|513.015|31.9587|33.8359|148|32768 / 15|
|Oneblock|4.2387|30.6463|1.63497|1.79245|19|8192 / 13|

这两个 timer只从 knowledge 进入计起，到 prooflog 返回结束。dotProduct 前面的 statement/binding/scalar编码、generators retrieval不在guard/core中。生成元在guard外检索，但prooflog内`g=bases`复制算入core。完整mapped opening减 preparation/guard/core 后，Full12 P约8.3929秒、V约9.840924秒是其他opening工作，不是“纯序列化”。Oneblock对应1.9794/2.990117秒。

Knowledge：P创建n个fresh mask scalar，wideMSM(g,d)+Hrv、scalarCommit(dy,ry)，生成挑战后编码n+3 responses；P和V都Reader解码responses；V执行wideMSM(g,z)和两条group equation。每term独立guard没有删除。Full12 guard payload155,212,928字节、core161,024字节；公开a系数另有148×32768×32=155,189,248字节，已包含在proof总大小内，不能再加为额外proof。

IPA P：初始 x/witness和g/aa复制、dot(x,a)；每轮新建xl/xr/al/ar/gl/gr，计算两次cross dot与两个wideMSM，生成挑战及inverse；随后每个i执行`g[i]*inv + g[i+half]*c`，并折叠x/aa与blind。最终构造Schnorr响应。宽度依次32768→1，总half=32767，因此每term生成元折叠65,534次直接G1标量乘、32,767次加法；148项合计9,699,032/4,849,516。左右cross MSM每term30 calls、65,534 terms，合计4440 calls、9,699,032 terms。上述是确切源码路径计数（不含表达式优化或群库内部基域运算），不是耗时分配。

IPA V：不做P的逐步generator fold。重建15个challenge，对n个weights做每轮乘法，wideMSM(bases,weights)一次，再dot(a,weights)，执行最终方程；也进行U+=L*c²+R*c^-²等直接群运算。n×log2(n)权重乘法结构重复，但challenge各term不同，不能缓存最终weights跨term。

generator 已以n为key缓存；`generators()`返回vector副本；公共基相同但每项进入prooflog又复制一次，且V目前也建立g/aa副本（后续独立权重路径使用bases/a）。公共immutable基/unused副本、切片视图、scratch容量复用是低语义风险候选，但MCL mulVec暴露mutable base参数，修改前必须确认借用/别名安全。相同right mask或low公共系数可比较后共享immutable表示，不能共享证明挑战、mask或guard。

Writer::point调用group并encode，Reader::point调用decodePoint，后者再次curve/group/order检查。P也通过exchange读取自己发送的公共点；decodeHandle结束又encode，存在反复编码/校验路径。不能称其“无意义”或自动删去：这些是输入边界约束，改变时要证明可信边界等价，并保留恶意point拒绝。

513.015秒尚无内部采样或子计时。生成元折叠是强结构热点候选，但当前不能声称它占core的某个百分比。每项耗时、inner-product耗时、allocations、group validation独占成本仍NOT_MEASURED。没有新增仪器开销校准数据。
''')
 md('GROUP_OPERATION_PROFILE.md','''
# MSM 与直接群运算范围

源码锚点：crypto.cpp:38 msm、39 scalarCommit；hyrax_a3.cpp:9 wideMSM、37-68 prooflog；streaming_master.cpp:20 masterMSM；single_hyrax.cpp:112-123 commit；依赖3rd/mcl/include/mcl/ec.hpp:2062 mulVec（依赖源码在上一阶段dependencies归档）。

|scope/role|全census calls|wrapper calls|全terms|wrapper terms|wrapper CPU秒|
|---|---:|---:|---:|---:|---:|
|Full12 P|1426753|729281|730247709|16036381|166.488|
|Full12 V|276607|276607|10290249|10290249|80.5799|
|Oneblock P|522432|457328|68061591|1395095|62.1364|
|Oneblock V|176144|176144|678067|678067|17.1582|

P未计时wrapper的差额恰好等于single::commit自行归并的worker calls/terms：Full12697472/714211328，Oneblock65104/66666496。commitment分别499.154/43.6014秒，是包含生成元检索、随机blind、readRow清零/拼接/标量合法性检查、基复制、MSM和结果存储的inclusive总wall，不能把它全叫MSM wall或用它补全wrapper CPU。

masterMSM接受size≤16384，复用thread_local1024点scratch；每1024 chunk复制基后调用mulVec；一个逻辑masterMSM可产生多个wrapper/census调用。single::commit绕过MSMTrace，以每worker1024点scratch直接mulVec，结束后合并计数，因此不会造成当前MSMTrace静态bins的并发写入。MSMTrace本身不是原子全局计数器，若以后将wrapped路径并行化需先修复聚合安全，不能把当前thread-local allocation counters当成全进程allocation。

crypto::scalarCommit是两个基的msm，是size2 bins的大来源；wideMSM直接调用G1::mulVec。MCL当前源码mulVec先尝试已安装GLV hook，失败再mulVecN；mulVecGLVT按n分small/large，size1还可能用单点路径。当前归档提供编译配置/依赖SHA，但无每次内部branch trace，所以不能以泛称Pippenger替代实际路径，也不能把2项wrapper都称为2次独立标量乘。

原bins为1、2、3-8、9-64、65-1024、>1024。用户要求的1和2-8可精确合并，其余边界穿过原bins；MSM_PROFILE.csv同时保留原bins及请求bins，缺失项明确NOT_MEASURED。wrapper只有CLOCK_PROCESS_CPUTIME_ID，没有wall，平均CPU latency=bin CPU/calls，不能命名平均wall latency；百分比仅占同wrapper CPU。Full12 size2 CPU P65.8595/V17.6554秒，>1024 CPU P92.2821/V62.4575秒。不能跨census与wrapper再次相加。

直接群运算单列：IPA generator folding（见A3报告）、U/L/R更新、knowledge和final equation中的H/G/commitment标量乘、single::commit每行H*blind和point addition，均不属于MSMTrace bin。point serialization/deserialize、isValid/isValidOrder/curve checks走crypto.cpp:9-30，暂无独立timer/census。群运算wall总占比未知；把P166.488 CPU秒除6107.571 wall得到“2.7%总MSM”是错误口径。
''')
 md('MEMORY_IO_ANALYSIS.md','''
# 内存与 I/O

Full12矩阵32768×32768，physical actual714184720、padding359557104。该shape是commitment寻址域，不是源码实际分配了完整2^30个Fr的dense矩阵；commit/readRow按行拼装。逻辑域2^31也未materialize。sizeof(Fr)=32由运行指标及归档类型配置确认。

|指标|Full12 P|Full12 V|性质|
|---|---:|---:|---|
|peak RSS KiB|52054100|6228680|getrusage/进程监测实测|
|约GiB|49.643|5.940|RSS单位换算，不是新测量|
|seal阶段结束RSS KiB|35983848|5538932|阶段末statm|
|U source字节累计|708536253952|无U访问|代码在每round按span.stored×32累计，非PMU实际读流量|
|K API read bytes|36775662720|无K cache|pread成功返回字节累计|
|K API write bytes|18387827424|无K cache|pwrite成功返回字节累计|
|K I/O累计wall秒|55.3904|不适用|transfer/evict/advice计时子集|
|FS proof file I/O秒|0.640318|0.106977|FS buffer API计时，嵌套于process/phases|

P峰值出现在all-classified-roots-prechallenge-seal阶段：其累计peak从29444156涨到52054100KiB，阶段末降为35983848KiB。源码single_commitments.cpp把prepared helper重新pack为source values，然后清空prepared；witness/model/packed source/临时buffer可能共存，但没有allocation timeline，不能把峰值全部归因到某一对象。RSS包含resident而非虚拟地址数，file page cache通常不等于进程RSS。

single_hyrax.cpp:52 readRow每行out.assign(columns,0)，扫描runs并拼接私有值；commit worker循环每个物理row（含未触及tail行）都有该清零，只有touched row做分块MSM。4workers scalar scratch bound4194304字节，G1scratch另计；这是源码边界而非整个commit额外内存实测。fold每term重新建witness列buffer和row scratch，并按left遍历相同source；carry/edge可能重叠读同物理行。preparation63.682秒包含这种工作以及rowCombination MSM，不能单独算memory copy耗时。

u_streaming.cpp:56-109每round建一个≤4worker pool、shared低位weights和tile，直接读sealed spans；每round重新访问整个U，而不是把庞大private folded input复制进cache。31round记录P U_scan652.3055 wall/1190.8485 CPU秒。日志U_bytes_read是span公式计数，scan包含Fr乘加、线程调度join和padding处理，不可将708.536GB/652.3055直接标为实测DRAM bandwidth；cache命中、Fr临时读写、低位weights流量都没有硬件测量。

PublicK缓存512MiB/4GiB和mapping-plan spill782724040字节为不同系统。K初建向量最大512MiB；K累计kernel cpp_allocated_bytes=60733587456（115个kernel traces，P），V23152623616（45个），这是tracing thread内ordinary new请求的累计字节，不是峰值存活量或malloc实际承诺量。K_RAM_peak1010259296含builder临时量，K_file_peak4294967296；U_TOTAL_EXTRA_RAM_BYTES是源码预检上界，不是RSS分配测量。

hyrax_a3.cpp:37复制bases/aa/x，每round新建左右标量和point向量，形成分配/复制候选；当前无字节实际copy profiler。MSM allocation bins为0仅覆盖trace生命周期ordinary C++ new，排除caller construction、worker、C malloc、aligned allocation，不表示全程零分配。

环境固定affinity=[0,1,2,3]、ZKGPT_THREADS=4、U_THREADS=4、SINGLE_COMMIT_THREADS=4，Full12启动load约97，Oneblock环境原件另存。没有perworker NUMA placement、page locality、cache miss或DRAM计数。当前没有证据判断NUMA主因、swap主因或memory-bandwidth上限。I/O bytes是API成功transfer，可能经page cache，不能叫NVMe/磁盘物理字节。FS file time小也不能排除K/cache或mapping I/O开销。
''')
 ranks=[]
 for ph in sorted(x('full12','P','CENSUS')['phases'],key=lambda t:-t['seconds']):ranks.append([ph['name'],fmt(ph['seconds']),'阶段inclusive；只在此层排名'])
 md('BOTTLENECK_RANKING.md',f'''
# 瓶颈排名（按证据层级）

{table(['Full12 P阶段','wall秒','解释'],ranks)}

Full12 V：Protocol1485.077 > Native431.892 > GKR53.5301 > source32.4829。阶段外110.486506另列，内部未知，不强塞给最高阶段。

Protocol1内部可比较的互斥P子区：U扫描652.3055；A3 core513.015；claim prepare358.118；K build355.983；U-K products200.387181；K fold129.443；guard65.3161；mapped fold prep63.682。它们处于不同父区域但不相互包含；其余plumbing、query生成、序列化、校验及残差另列，不将这些行当作完整process partition。V为prepare293.653、K kernel79.196865、A3 core33.8359、guard31.9587，其他终点等差额另列。

Native整体大于上述单函数，但子函数没有timer，暂不发布“Range最慢”或“Product占x%”。A3内部direct generator folding计数巨大，尚未测独占秒，列为高优先验证假设。MSM wall和memory bandwidth未测，不把它们列为已证实的主因。

Oneblock结构不同：Native658.724占P全程约62.68%，Protocol1200.716约19.10%；Oneblock无K回退，不能用其各项百分比线性外推Full12。
''')
 md('OPTIMIZATION_OPPORTUNITIES.md','''
# 下一轮优化候选与验证证据

本轮没有优化实现，也没有给出无依据的收益预测。所有候选必须保持Statement/Transcript/Proof、model binding、安全状态和Knowledge Guard约束。

|优先|函数/位置|实证动机|候选与所需验证|
|---|---|---|---|
|1|quadraticRoundStreaming / Weights::dot，u_streaming.cpp|652.3055秒scan+200.387181秒product；31轮重复U|用采样区分Fr/调度/访存；检查tile/小任务调度、只读权重复用；先局部同基线benchmark，不能更改fold语义|
|1|Commitments::prepareClaim，classified_commitments.cpp:38|P358.118/V293.653秒；每claim再次完整digest/map|保留完整revalidation和challenge顺序，优化固定canonical map表示、散列分块与Builder存储；不能直接跳过第二遍身份验证|
|1|PublicK::Impl::with / kernel|70次fallback kernel约223.43717秒|先评估cache准入和根访问次序；预算调整是后续明确实验，不在本轮自动改变；必须保留checksum/session和内存安全|
|1测量|hiddenRange/Exp/Memory/Product / expressionSumcheck|Native1890.690秒整体尚未定位|先顶层+nested aggregate timers，分离inclusive/exclusive及role；不能仅用proof字节预测时间|
|2|hyrax_a3::prooflog|513.015秒core，9.699M direct G1折叠乘法|先core内采样/子计时；研究严格等价的scratch/slices/基表示，保留每term fresh guard及challenge；不要直接合并148项|
|2|single::fold/readRow|prep63.682秒；每term重读/清零|按exact row/range和right结构评估scratch或只读系数缓存；private memory峰值需预算；不得把物理padding证明语义改变|
|2|kernel_at + fold(K,q) V终点|79.196865秒kernel+约25.89秒其余终点|单独测dense fold和selector，不把residual误当纯fold；保留V独立public计算|
|3|generators()/prooflog copies、Reader/Writer point|重复cache返回copy及encode/decode/validation路径|测分配/编码/阶检查；仅可信immutable边界可考虑cache，必须保持malicious point rejection|

先复用当前结果的身份和原命令。下一轮如加timer，应函数/轮边界聚合、线程安全、独立开关；测量开销用同fixture带/不带计时的短benchmark量化，随机挑战完全由原协议生成。不能用本轮既有仪器的未校准开销宣称零扰动。无需先启动Full12；已有日志已解答关键Protocol1差额，先测Oneblock Native/V与A3局部fixture。
''')
 limitations='''
# 测量边界与缺失指标

- `DIRECT`：日志timer直接输出；`SUM_DISJOINT`：互斥区域/轮求和；`DIFFERENCE`：父减已知子，残差保留未测成分；`PROOF_STRUCTURE_PLUS_SOURCE_EXACT_COUNT_NOT_TIME`：帧维度/调用路径公式计数，不是时间；`NOT_MEASURED`：缺证据。
- wall为steady_clock或外部monotonic进程计时；CPU为CLOCK_PROCESS_CPUTIME_ID或wait4/getrusage user+sys，包含本进程worker，非线程独占CPU。P并行阶段CPU可大于wall。原日志没有绝对start/end时间戳，库存保留NOT_MEASURED并提供elapsed/行号。
- phase为顺序互斥顶层区；内部timer inclusive，不直接重复相加。root timing包括provider内的K构建，U_ROUND汇总ROOT；K io又嵌于advance/provider。aggregate OPT2_AUTH跨多个phase，不能当作P1独占。Full12 phase CPU在timer结束后采集，包住少量phase报告操作，边界不完全等于wall。
- 现有timer输出默认流精度有限（大时间约0.005秒舍入）；差额与小residual只保留原精度意义，不能由CSV小数位宣称亚毫秒准确。bin CPU总计与逐bin和可能末位不一致。无controlled repeats，不提供CI或显著性。
- Native内部、各block、逐term wall、A3 fold/内积/cross MSM独占秒、direct scalar mul/add总计时、point validation/encode时间、MSM wall、指定新bin边界、cache miss/NUMA/DRAM带宽均未测。
- 已有ordinary new counters仅统计tracing thread和trace内；malloc/aligned/调用前构造/worker不覆盖。K cumulative allocated bytes不是peak allocation，U byte及Fr-operation counter是源码按维度累积，不是PMU traffic或每条指令计数。
- proof帧解析只读取字节和标签、确认footer/EOF及SHA，不重新验证方程。旧V日志/RESULT用于历史验证身份，未把本轮结构扫描称为新协议acceptance。
- 没有新增计时埋点、sampling profiler、局部benchmark或新Oneblock/Full12运行。本轮优先复用已足够解释P1的现有31/27轮记录；Native与A3剩余问题保留实测空缺，给出下一轮最小测量范围，而不在当前分析中改变执行路径。原有埋点的clock_gettime、逐root日志和new hook开销没有独立校准，不宣称忽略不计。
- 共享服务器不同历史负载/日期/配置；Full12环境load约97，affinity4核，Oneblock另外记录；无同负载对照。Balanced Anchor声明不同，不作安全等价解读，运行历史branch名也不能替代源码内容SHA。
- Windows当前源码与Linux读取的src/CMake SHA吻合；源码与运行记录重合文件匹配；完整源码包不包含模型参数大文件与依赖库实体，恢复需RETENTION_LINKS.json指向的已验证旧归档及身份记录。本轮不会删除任何副本。
'''
 md('MEASUREMENT_LIMITATIONS.md',limitations);md('EXISTING_TIMING_LIMITATIONS.md',limitations)
 md('CHANGE_MANIFEST.md','''
# 变化清单

唯一新增目录：reports/R7-SINGLE-COMPACT-PERF-ANALYSIS-20261105-v1（Windows及固定Linux workspace中的镜像）。

产品src、CMake、参数、Statement、Transcript、Proof、历史实验原文件均未编辑。无Git commit/push/pull/fetch/reset/clean/rebase/merge；无构建/证明/验证/恶意测试运行；无目录删除或兄弟source copy。没有新增instrumentation，may_enter_stage2=false与BLOCKED_CRYPTOGRAPHIC_DESIGN保持原证据值。

新增：remote_preflight.py/collect_remote.py（只读Linux identity/log收集）、analyze.py（raw日志及proof结构解析、恒等式与SHA核验）、write_reports.py（生成说明）、verify.py（交付检查）、publish_report.py（唯一新目录镜像与读回）、中文报告、CSV、parsed JSON、raw/historical证据副本、ANALYZED_SOURCE.zip（纯source字节，不是执行副本）、SHA与restore-read收据。没有复制SSH配置、私钥或凭据。

证据来源和原路径在REMOTE_PREFLIGHT.json、raw/IDENTITY_BEFORE.json和RETENTION_LINKS.json。Full12/Oneblock原proof/statement SHA已核验，继续使用原Windows证据归档。Linux祖先Git branch main、Windows branch r7/remove-full-base-registration-20261029；两者不是运行内容等同的依据，使用源码SHA。

重现（Windows，在本工作目录树中）：python analyze.py；python write_reports.py；python verify.py。这仅解析和生成本分析目录中的结果，不启动密码学程序。remote_preflight.py仅能在固定Linux目录执行。publish_report.py仅用于报告文件镜像，不部署产品。

最终检查结果：VERIFICATION.json；交付包SHA、逐成员restore-read及Linux镜像读回：DELIVERY_RESTORE_READ.json、REMOTE_DELIVERY.json（生成后可见）。历史完整proof/statement/dependencies的恢复位置保存在RETENTION_LINKS.json，不依赖已删除的R3/R4/R5A远程快照。
''')
 # Supplementary exact boundary inventories.
 md('TIMER_SOURCE_MAP.md','''
# 源码计时点索引

全部文件均在ANALYZED_SOURCE.zip，以SOURCE_IDENTITY_VERIFIED.json记录SHA。

|记录|源码|边界|
|---|---|---|
|PHASE_BEGIN/COMPLETE/CPU + census.phases|oneblock_main.cpp:229-234，classified_controller.inc:124|fn()包络；CPU仅Full12输出|
|SINGLE_HYRAX_COMMIT|single_hyrax.cpp:112-123|blind/generator/row worker整个commit|
|OPT2/OPT3A_AUTH|classified_commitments.cpp:38-68,142,146-148|mapDigest CPU+prepareClaim CPU累计；wall只有prepare；kernel累计|
|OPT3B_PLAN|classified_commitments.cpp:51|单claim storage状态，无单claimwall|
|OPT2_KERNEL|classified_commitments.cpp:55-68|每次dense result分配+冻结map遍历系数累积|
|OPT3C_K|public_k.cpp:35-109|build/fold/io不同嵌套边界，参PUBLIC_K_PROFILE|
|OPT3D_U_ROOT/ROUND|u_streaming.cpp:56-109，unified_input.cpp:80|ROOT含provider；ROUND聚合scan/product counters|
|OPT3C_ROUND|unified_input.cpp:77-86|该轮poly+Sigma+coin+advance+combine|
|UNIFIED_SUMCHECK|unified_input.cpp:70-96|含独立terminal kernel_at/endpoint|
|COMPACT_QUERY|single_hyrax.cpp:144-164|validateSecret之后到stitch/equality；preparation只fold callback|
|UNIFIED_OPENING|classified_commitments.cpp:195-201|含singlePlan/descriptor/open及完成绑定|
|A3_TIMING|hyrax_a3.cpp:88,99；classified_controller.inc|knowledge和prooflog累加；排除A3 statement/basis retrieval|
|OPT2_MSM|opt2_metrics.cpp:13-27|wrapped区域CPU，六个旧bins，非wall|
|FS hash/file_io|fs_io.cpp|FS内部API，不含K/mapping/cache总体|

原始timer数值与每条行号由EXISTING_TIMING_INVENTORY.csv及parsed/*-[PV].json提供；同一记录的多个timer也分别列出，不将它们默认视为互斥。
''')
 print('Reports generated')
if __name__=='__main__':main()
