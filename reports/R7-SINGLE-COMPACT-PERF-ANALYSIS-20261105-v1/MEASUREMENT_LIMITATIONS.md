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
