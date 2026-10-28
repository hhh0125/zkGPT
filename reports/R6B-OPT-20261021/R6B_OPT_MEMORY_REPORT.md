# R6B-OPT 内存优化结果

已实际修改源码、构建、测试和生成oneblock真实证明。full12序列化峰值保守预算由149.22GiB降为109.22GiB；这是实图/生命周期推导，仍无full12实测。建议新候选上限P128GiB、V96GiB，下一轮单独批准，不沿用旧192GiB草案。

## 六项验收回答

1. 原149GiB来自：lookup planner78,748,221,440bytes（十进制78.75GB，73.34GiB）+val0增长余量+中间层值+整数矩阵+两份Master+3graph保守项+2GiB杂项，再加25%安全余量；序列化再加4份proof上界。不能把所有估算项说成同时实际RSS。
2. 主要实际热点是映射输入Sumcheck整域复制、pr.val0/cc.w/callerMaster多份长期驻留、W/Child完整向量。Child合计9.34GiB，其中W8GiB。图容量5.43GiB；预算保留3graph=16.29GiB，但GKRBuilder持const引用，并无额外图复制。
3. 已优化：逐项核对affine gather和全padding，消除临时完整W/Child；kernel移动进既有Sumcheck因素容器，只保留一份必要的折叠witness副本；消除initializer-list两轮复制及kernel滞留；错误消息每child构造一次。没有增加另一套Sumcheck/GKR/Lasso。
4. 真实oneblock RSS 6.390→4.889GiB，下降1.502GiB（23.50%）。2^20 mapped测试额外普通C++new峰值160.01→64.01MiB，RSS229.30→133.62MiB；计数不含directmalloc/alignednew/分配器开销/既有存储。完整oneblockwall 465.76→471.03s，未显示整体提速；共享服务器调度/采样也影响wall。
5. 新full12 prove109.09GiB、serialize109.22GiB；来自真实277layer/612terminal/199348640Master实际值/268435456padding及完整查询。lookup 73.34→41.34GiB。旧mapped8M预算改4M（实际3M+1M余量），局部Range/Exp峰值10.00GiB未缩减。总预算下降40GiB包含安全余量变化，不能宣称full12实测省40GiB。
6. 值得进入新版本正式授权审查，128GiB候选上限比新峰值多18.78GiB；当前并未获得执行许可。共享资源快照不是独占预留，未批准并发大进程仍被门禁拒绝。block3..11真实数值域、完整证明/独立验证结果和full12实际RSS/耗时仍待授权运行。

## 分阶段预算（GiB，全部full12估算）

|阶段|优化前|优化后|
|---|---:|---:|
|graph|5.93|5.93|
|witness|21.52|21.52|
|commit|27.07|27.07|
|prove|149.09|109.09|
|verify|61.78|61.78|
|serialize|149.22|109.22|

## 实测与兼容性

最终Release65/65；改变的mapped_input/lookup_batch编译单元及真实测试定向UBSan3/3。全程序其余对象没有声称全部被本轮sanitizer覆盖。受限3block真实witness通过，跨block篡改被拒绝，RSS4.152GiB、wall47.32s；没有该3blockproof/承诺。新门禁23/23、50subcases通过。新门禁首次测试部署漏建test-runs父目录导致setuperrors，已补目录并全量重跑，原失败日志保留。

oneblock新的statement和proof与本轮优化前及R5Bnormal-v2独立ACCEPT原件逐字节相同，真实新独立Verifier从全新空cwd **ACCEPT**。Verifier RSS1.367GiB、wall320.32s。旧Verifier兼容性由完全相同的原已接受bytes支撑；没有多跑一次冗余旧Verifier。证明数学/FS兼容理由另见PROTOCOL_COMPATIBILITY.md，不能仅凭测试通过推导协议未变。

oneblockCPU user/system 472.64/13.00→476.40/14.09s；内核文件I/O输入30432→25416、输出18240→18320个512byte块（缓存条件不同，非算法I/O改进证据）。proof5,683,187bytes、statement3,174,795bytes均不变。无磁盘spill或以I/O换内存。

## 身份与停止条件

ProfileSHA/circuit/Wanchor/manifest及120参数全部保持原值，FULL12_FINAL_OPT_PLAN与旧plan每个布局/坐标/Range/Exp/GKR字段完全相同，仅资源预算及新增分解项变化。新源码树4c57b15419afc309c41545145f276c194a463601c44a07f61716b6fc02877810；P02ca7fef5bb9cd6aad845fc5ef1ce958916f10770e1db15f759e206c613e8e56，Vd2131c7319009f1027b1a657c6f88db7995c481adf3f84b368cfcecb58a25c69；门禁904717cd4be7dc9d8daf07c90b54b5235a6dfdd81974baaebcb30dc67a840867。新ID R6B-OPT-20261021-v1。旧门禁实测拒绝新字节，旧草案/SHA未继承。

未来需分别批准新ID、上述身份、固定X/路径、P128/V96预算、CPU4、P28800/V14400秒及外层+60秒、32GiB共享内存/64GiB磁盘保留、两份permit全文与独立SHA、最终外部批准record的SHA和一次P成功后V的串行尝试。当前仅UNAPPROVED scope草案，无有效permit和用户SHA批准。

运行前重新核对所有身份、有效可用内存及cgroup/RLIMIT/PID/磁盘/并发；完整gate_resource_check为REFUSED: unapproved visible competing large job。身份、输入、Range/转换、W、proof、RSS/AS、共享reserve、磁盘或时限失败即停，保留最早错误及进程组清理证据，不换输入/截断/减查询/改Profile/加预算/自动重试。最终状态是优化完成、可申请授权，非当前可直接启动full12。

原R5B/R6A/R6B受保护1697项SHA重新核对相同；未Git操作、删除或复制完整源码工程。所有新记录位于reports/R6B-OPT-20261021与logs同名子目录。归档/实际恢复校验收据在交付时单独补入。未宣称full12proof通过、论文等价或整体零知识审计完成。
