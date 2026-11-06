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
