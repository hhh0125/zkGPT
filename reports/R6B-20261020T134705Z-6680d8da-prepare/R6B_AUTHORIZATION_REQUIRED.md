# 下一轮必须明确批准的具体项目

当前仅PREPARE-ONLY，以下均待用户独立确认，不表示已经授权。

- 唯一ID：`R6B-20261020T134705Z-6680d8da`；范围为保持冻结Profile/参数/Exp/全部约束的完整12层witness＋一次Prover，成功后一次独立Verifier；失败停止，不自动重试。
- 源码树：`68b444159c4fe756eef0fc4d92570150302e47316e16a1e681ad828820e81b8b`；Profile/circuit/anchor/manifest及全部120参数采用IDENTITY_CHECK中的现算且匹配的身份，保持operator-trusted声明。
- Prover二进制：`/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/build/release/src/paper_r2/r6_model_prove`，SHA `80f0da673f3f07065e41a1377ebad629beb1e1d579bd9df60ca877dda97bdd3d`。
- Verifier二进制：`/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/build/release/src/paper_r2/r6_model_verify`，SHA `3c257707a2eb2f1669accc5e410145ae3678466e015da59905ed83166b5e23fb`。
- 公共X：原默认seed1/幅度350，active32×768、padded32×1024、单位5/512；signed64le SHA `a1871e2f9731ed73e1b5db9655ee760d3e5435a06d793c09f1dc1eb5f6655edb`，与冻结R5B正常statement的全部X一致；禁止输入环境覆盖或样本替换，不减小维度，不绕过block3～11整数域检查。
- 预算：Prover192GiB/原生28800秒/外层28860秒；Verifier96GiB/原生14400秒/外层14460秒；每次共享可用内存预留32GiB、磁盘64GiB、cpu-limit4；旧GKR有32worker阶段；串行运行且无并发大型证明。快照不构成资源预留。
- 目录：准备记录 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6B-20261020T134705Z-6680d8da-prepare`（本轮仅此目录创建）；后续Prover输出 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6B-20261020T134705Z-6680d8da-full12`；许可目录 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6B-20261020T134705Z-6680d8da-permits`；独立验证空目录 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6B-20261020T134705Z-6680d8da-independent-empty`。后三个保持未创建。所有log/time/runJSON精确路径见草案，因现有helper而处于R6A下的新唯一名字，不得覆盖任何旧文件。
- 用户下一轮先批准上述ID/身份/预算/输入/目录/范围，再生成两个permit，并分别独立核准实际字节及各自SHA；当前两个SHA都为null。请勿仅授权“运行模板”而跳过独立pin批准。程序没有技术隔离能证明审批者独立，必须保留用户明确审批证据。
- 执行前落实外部门禁：CLI/permit/supervisor三方一致性；清除输入/库覆盖环境；所有路径无冲突；Prover成功且产物存在/非空/哈希/X/context匹配才放行Verifier；新空cwd；重算冻结身份和资源/PID/竞争进程；所有失败记录最早可定位错误、退出/信号/阶段，保留原始日志，不静默修复或重试。

这些是草案要求，不是已部署的新串联执行器。现有runner尚不能自动保证全部条件。任何必要执行编排应放在新R6B记录/流程中，不改旧R6A记录；若必须改冻结源码/二进制，则当前身份失效，重新核验并单独批准。两份真实permit及SHA须在后续授权后产生；本轮到此停止。
