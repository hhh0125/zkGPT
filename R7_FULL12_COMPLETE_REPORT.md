# R7 Full12 unrestricted end-to-end

实际完成一次原生12 Block Full12 FS Proof，并由 Prover 退出后的独立 Verifier 完整接受。工程验收为 ACCEPT；密码学安全状态仍为 FS_NIZK_CANDIDATE / BLOCKED_CRYPTOGRAPHIC_DESIGN，may_enter_stage2=false。本轮不是密码学设计审计或生产准入。

- 真实执行：277层、48 FC、76032 Exp查询、120个原参数文件、Master199348640字段；完整保留240761504个 witness 字段的私有检查点，仅远端0700目录/0600文件，未归档。
- 真实独立 Verifier：退出码0，8066 / 8066 项必须关系全部 VERIFIED，45物理根、10模型分块、11个显式跨Block连接。关系数来自实际 Verifier 运行；完整逐项证据见 R7_FULL12_COVERAGE.json。
- Proof：6651349553 bytes，SHA256 `9c905ec2cc4a674bc1d9c186157b9dbf996302705c433542640184be86e0a136`。
- Anchor：两个独立新进程重做原R6 W预处理，并独立核对全部163749888个实际W字段，规范分为10个RowMajor分块；固定Pin `02ed086e3a5b596e193ded8ede2e5de5da38a1358ff628068cea1e483e1e90c1`。Verifier自行加载固定信任根。
- X/Y：新生成的真实12 Block witness 与原R6 Full12参考逐字段一致；统一公开Statement绑定Block12 Y。单一Full12 Statement、模型身份及FS Transcript，未拼接12个Oneblock证明。
- FS Session `7020bd8edf05f4f1342800d81a3ecb3a398bd7603fa26dfd93fc2be87836aff6`；P/V的Session、Challenge、Transcript、Proof SHA和bytes逐项一致。
- 源码提交 `f10371c6798b394825a1df48bd16f7ee67545343`；实际执行时全部386个源码/配置文件SHA固定并在运行后复核。Windows权威基于9f8869e，未从GitHub覆盖。

逐根运行时认证记录见 FULL12_RUNTIME_ROOT_AUTHENTICATION.json。原始 full-V-LAYOUT.json 的静态分块描述沿用历史规划字段 PENDING_INDEPENDENT_CHUNK_ANCHOR；该字段不是运行验证状态。实际固定Pin、独立Anchor重算及Verifier本地逐根VERIFIED记录均单独保留，10个W分块没有借用旧单根Anchor。

实际性能：Prover wall 9015.756s，CPU 9095.462s，peak RSS 52082248KiB，采样peak VmSize 52742572KiB；Verifier wall 5536.603s，CPU 5587.429s，peak RSS 6496724KiB，采样peak VmSize 6904676KiB。MSM calls/terms：P 1475737/757383584；V 476032/200647370。逐阶段wall/CPU、原始5秒资源记录、磁盘/限制/cgroup信息见性能JSON及原始日志。VmPeak为实际/proc采样最大值，RSS为wait4精确高水位，未用Oneblock乘12估算。

取消了本轮进程的8GiB地址空间人为上限，继承硬上限unlimited；未使用sudo或更改共享服务器配置。无任意运行分钟数限制。记录共享服务器其他用户的聚合RSS、物理内存、Swap、cgroup和磁盘；只在真实内存/磁盘压力时终止本任务进程组。

组件与回归检查包括分块映射/多根Helper/注册/zero-tail/A.3 Opening/认证屏障、Full12独立FS小规模进程、Full12/Oneblock套件隔离、OPT2 Kernel、原Oneblock模型/FS/plan/GKR/lookup。小规模合成检查没有作为真实Full12成功依据。错误X、错误模型身份、实际Proof row-response篡改均由独立Verifier拒绝，退出码2；单份Proof完成可逆篡改后恢复原SHA。Anchor替换、缺失、重排和混用也均拒绝。

只读工程复核未发现Critical/Important实现问题；记录一个非阻塞限制：继承的部分Oneblock P_ATTACK调试开关尚未适配Full12，不能用于Full12负例结论。本轮使用独立Statement修改和实际Proof消息修改，未使用这些开关。见 ENGINEERING_REVIEW.json。

复现：固定工作区 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`；恢复source及依赖/原120参数/固定Manifest/Anchor；按 runner/full_run_remote.py 的独立FD、环境、限制及顺序执行。该脚本使用排他输出名，复跑应选择新的Proof/日志输出路径并保留原证据；固定Anchor路径和Pin必须保持一致。完整命令和真实结果记录于各进程JSON。

Windows交付将只存一份大型Proof，其他证据含完整源码字节、补充patch、构建配置和依赖身份、公开Statement、Anchor/Pin、原始P/V和负例日志、Coverage、资源样本、命令和SHA清单。必须通过Windows容器/逐成员SHA与恢复读取验证后才认定归档完成。禁止归档私有witness、盲化、密钥和SSH配置。本任务未授权删除任何远端副本。
