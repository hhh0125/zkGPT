# P0-B真实oneblock待授权验收

本文件是可审阅验收范围，不是permit，不会触发运行。本轮没有授权真实oneblock Prover或Verifier。

候选源码SHA 5c7053817202c418cd63947e8b75d83e7d04f4279abfe6b9f64cda708a6ae416；二进制精确SHA见NEW_IDENTITY_SHA.json。仅固定zkGPT-work。旧P0A独立Verifier已冻结于本阶段frozen/，SHA见BEFORE_BINARY_DEPENDENCY_SHA.json。原P0A完整proof/statement见reference/frozen-oneblock/。

收到单独一次明确授权后：先只读核对本候选源码/二进制/依赖/全部参数身份，再做最新P24/V8 cgroup/RLIMIT/PID/容量门禁；CPU0..3，保留32GiB有效可用内存和64GiB磁盘，原P1500/V1200超时。用户已批准oneblock-only其他用户大型任务WARN，不终止修改他人进程，不更改正式full12门禁。输出使用全新单次leaf且不覆盖原件，遇错即停，不重试、不等待自动启动。

1. 新生产Prover生成真实R5B oneblock文件，独立记录wall/user/system/峰值RSS。
2. 与冻结statement/proof逐字节比较并记录尺寸和SHA；任何不兼容停止。
3. 新独立Verifier接受新proof；冻结P0A独立Verifier接受新proof；新Verifier接受冻结旧proof。
4. 必要真实产物篡改应全部REJECT，拒绝必须来自验证而非usage错误。
5. 保持生产wall与诊断kernel计时分开；不把不同运行时间差/函数比值等同端到端加速。若需轻量诊断只允许kernel计时/计数，协议路径不变，输出必须与生产字节一致。
6. 完整证据、配置、身份、proof/statement、日志与资源/频率采样归档Windows并SHA恢复读取验证。无full12、P1、permit或旧scope转移。
