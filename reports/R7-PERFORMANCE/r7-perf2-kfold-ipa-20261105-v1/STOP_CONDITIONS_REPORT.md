# 停止条件记录

STOP_PERFORMANCE 触发：K 7组配对比中位数1.04977，低于预先固定的1.05，尽管范围稳定。立即退出验收脚本、保留全部8组（含预热）和P/V原始记录，默认关闭候选；不再运行下一夹具、profile 或 IPA。只允许交付默认关闭版本的构建、回归、身份审计和归档工作，已完成。

未触发 STOP_READONLY/SYNC/BUILD/CORRECTNESS/SECURITY/RESOURCE。TDD red是预期断言失败，非候选数学失败。一次验收脚本启动早于最终身份完成，K-ready-SUCCESS前置文件尚不存在而 fail closed；没有启动编译、证明或性能子进程。保留 accept-K-FAILURE.json；随后等待构建/CTest及身份校验完成才执行 accept_k_v2.py，不删除或覆盖早先失败文件。

独立接受过程在性能门禁处退出，因此没有 after-identity 文件；测量前完整身份和部署SHA已冻结，随后默认关闭变更只涉及已列两文件，并经整个远端输入集SHA前置校验。最终417输入、272依赖、120参数在最终回归/独立验证前后保持一致。不得把缺失的accept-success当作成功。

STOP_AUTHORIZATION 防线：真实Oneblock/Full12未启动，状态 NOT_RUN_AUTHORIZATION_REQUIRED。预算未提高；完整CTest使用已授权12GiB AS/4CPU/180秒每例/600秒整套，其余有界proof4GiB/4CPU/60秒/120CPU秒。没有Git提交/重置/清理/分支切换，没有历史目录删除，没有兄弟源码复制。
