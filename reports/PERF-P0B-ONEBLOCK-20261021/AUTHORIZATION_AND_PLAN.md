# 本轮授权及执行计划

用户明确授权2026-10-21一次P0B真实R5B oneblock验收，源码5c7053817202c418cd63947e8b75d83e7d04f4279abfe6b9f64cda708a6ae416。一次生产Prover；新V验新、P0A冻结旧V验新、新V验旧；真实产物必要负向；仅验证器P0A/P0B等价诊断，不再生成诊断证明。无full12/P1/permit/旧scope重绑。任一硬门禁/证明/验证/字节/负向失败立即停止，不重试。

1.核对完整源码、所有生产二进制/依赖、126参数/Profile/anchor/manifest、历史保护及冻结证明，原full12资源脚本不变；P24/V8门禁。
2.仅在reports新建等价诊断TU/库/二进制：原kernel函数体改名后包装一次chrono/clock计时，旧DP入口计数/新宏计数。生产源码和二进制保持不变。小fixture验证诊断链接正确。
3.默认gate-only；显式--execute-oneblock写入不可重复token，生产输出leaf必须不存在由Prover建立。生产一次，新旧独立验证三次，证明字节比较。外部operator-trust-v2.anchor SHA和engineering-model-manifest-v1.txt显式传参，验证器从空工作目录运行。
4.真实负向复用已审阅P0A六种篡改/七次拒绝；确认实际REJECT文本不是usage。资源限制所有子验证同一V8，遇错停止。
5.冻结证明在P0A和P0B诊断Verifier各验证一次，得到真实challenge kernel耗时/CPU/调用/命中/结果，严格与生产wall分开。生产只有一次Prover。
6.逐项汇总实际wall/user/sys/RSS/资源/频率，比较P0A完整生产基线310.965864/169.688897；诊断只对诊断。完整归档Windows恢复读取核验。

Ruling:复用已批准执行计划；本轮不修改生产协议或实现，测试编排和只读诊断属于明确授权。固定工作区、不建worktree、不提交/删除历史；本报告目录为唯一新增阶段。外部大任务WARN规则仅用本阶段复制的oneblock policy，不修改正式full12门禁。
