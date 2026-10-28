# R6B-OPT2 内存核算与优化结果

已完成真实源码修改、最终版本oneblock证明和空目录独立Verifier验证。Full12仅运行真实builder metadata；没有full12 witness/proof、有效permit或A/B/C/D正式授权操作。推荐下一轮评估 **P96GiB/V96GiB**；本轮到此停止。

## 七项验收回答

1. **109.22GiB中的确认重复为8GiB原始存储预算，含25%余量为10GiB。** R1 mapped planner的4M包括immutable callerMaster，该Master又包含于R6的cc_w_plus_caller_master=2M。只能去重一次。旧预算另外4.773GiB是val0历史增长容量包络的收紧，不是另一项确认重复；不能把全部24.773GiB预算下降称为RSS节省。
2. **Graph三倍不是三个同时存活的实际图。** public预检图移出builder后在内部scope销毁，随后创建pr.C；GKRBuilder::C是同一pr.C的const引用。真实一份容量5.431GiB；仍保留两份额外容量10.862GiB作为安全冗余，不把free后allocator立即归还RSS当作前提。没有为了数字目标删除这部分预留。
3. **cc.w和caller已安全共享同一只读用途存储。** 新owner是prover内的padded vector，cc.w为借用指针，caller为const vector&，原有prover_commit及生成器不变。全流程值不变测试、实际地址和proof字节对照支持这一点。cc.w保留旧非const接口，因此结论限定于已检查的工程路径；其他legacy调用默认仍使用原raw-owner路径。
4. **val[0]已提前按实际builder的actual大小reserve一次。** 不再进行padding resize及shrink_to_fit临时复制，保持reserve到commit同一地址和capacity，最终size必须等于hint。注意旧OPT在证明前已经compact到actual，因此4.773GiB是旧证明阶段保守容量包络收紧，不能冒充新删掉的长期证明缓冲。实际构造/commit临时峰值也因此受控。
5. **真实oneblock RSS 4.888649→4.389965GiB，下降0.498684GiB（10.20%）。** Full12预算109.222867→84.450086GiB：10GiB确认去重、10GiB来自少一份8GiB物理Master并含25%、4.772781GiB为上述capacity包络收紧。真正full12物理/RSS收益未实测。
6. **建议P96，当前没有依据必须P128。** 证明估算84.316499GiB，序列化84.450086GiB；96还余11.549914GiB。这是在保留extra2graph、2GiB杂项及25%后得到。基础对象/工作区容量包络54.590874GiB，额外graph10.862326GiB，杂项2GiB，再加25%即84.316499GiB。P128还余43.549914GiB，属于进一步预留，不作为默认目标。96是有依据的规划选择，尚无full12实测保证，超限应停止而非自动加预算或重试。
7. **最终oneblock独立Verifier ACCEPT。** statement/proof与既有OPT及原R5B normal-v2逐字节相同，使用最终归档二进制SHA。Profile、full12 fingerprint、120参数、anchor及manifest保持冻结值。未宣称full12证明通过。

## 阶段预算（GiB，估算）

|阶段|OPT|OPT2|
|---|---:|---:|
|graph|5.931|5.931|
|witness|21.525|17.707|
|commit|27.069|23.251|
|prove|109.089|84.316|
|verify|61.784|61.784|
|serialize|109.223|84.450|
|artifact_disk|0.196|0.196|

这些是实际277层builder和源码生命周期推导，不是oneblock乘12，也不是各阶段实测RSS。序列化继续采用完整证明峰值加4份proof上界，没有利用GKR临时数组已消亡来进一步压低数值。

## Lookup与主要对象

generic R1 planner仍为41.339996GiB（retained children9.339996 + inclusive mapped4M32）。R6去除已在全局计数的immutable M，增量为33.339996GiB；加全局共享M后正好41.339996GiB。mapped的增量3M中，kernel和folding witness占2M，**GKRBuilder beta_g.clear()后保留的capacity占另1M**，直至proveR2作用域结束；没有空闲的8GiB可再删除。局部Range/Exp工作区10.000244GiB及全部查询不变。Child总计9.339996GiB，其中W8GiB。val0为actual5.941053GiB，中间层约1.269GiB，int32参数约0.610GiB，具体字节见JSON。

真实地址、拥有者和存活区间见OBJECT_ADDRESS_LIFETIME_EVIDENCE.json；地址来自最终真实oneblock，不伪造禁止分配的full12 witness地址。GKR两代folding最大1.5M与coeff1M重叠，其他输入端口域上界4194304fields，已在3M/2GiB杂项和保留余量内核算。证明返回后builder、mapped工作区销毁，长期pr/NN/children保留到main退出。PC/IPA按行列缩减，没有额外完整W副本；序列化预算未删。

## 实测CPU、I/O与分配

Prover wall 471.03→463.37s；CPU用户/内核 476.40/14.09→471.19/10.77s。Verifier wall308.09s，RSS1.367GiB。Prover文件系统输入25416→0、输出18320→18240（512byte块），缓存与共享调度条件不同，不能据此认定稳定CPU/I/O加速。无磁盘spill。

真实oneblock主要witness分配对象3→2。普通new/new[]受控小域真实commit测试：请求10→9，Master-sized7→6，请求bytes恰少1024。这是分配结构验证，不能冒充全oneblock进程malloc总次数；directmalloc、alignednew和allocator开销未覆盖。较早诊断版本曾多4个小字符串分配，已通过enabled guard移除，原始日志保留。

## 验证、身份与限制

Release69/69；ASan/UBSan定向4/4（prover.cpp+ownership和mapped_input.cpp+span，非全依赖sanitizer）；14项span/size/overflow负向边界，以及commit发布前分配失败保留旧owner、发布后诊断bad_alloc仍返回有效新owner。既有受限3block数值连接与负向测试在Release回归内通过，没有多block证明。门禁23/23、50subcases，仅模拟。所有2486项受保护历史SHA不变。所有失败/成功原始日志均保留。

源树 ff09226c16454a6060252a6fc61554a4b052dcf7926409d1f21edcfb7d9434f9；门禁 6d14e86f97bb551381b0b4ef062f26232ca8cfde43b19fcfff4639ca141e6c0f；推荐身份 R6B-OPT2-20261021-v1。旧OPT草案/二进制/许可身份失效；新记录只包含身份及候选上限，不生成生产范围草案、permit或用户批准SHA。

最终资源快照：P96检查 REFUSED: unapproved visible competing large job，P128检查 REFUSED: unapproved visible competing large job。这不是独占预留；共享竞争、cgroup/RLIMIT/PID、磁盘以及身份必须在未来另获授权后最后一刻重查。Full12 block3..11实际数值域、完整proof、独立验证、峰值与时长仍未实测。
