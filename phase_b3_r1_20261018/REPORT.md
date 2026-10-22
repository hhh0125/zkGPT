# B3-R1 实施与证据报告

**完整 zkGPT 复现尚未达到验收条件。** 本轮在指定远端仓库实际新增了46个文件，实现并测试新的关系基础库及独立 Verifier；没有把合成电路或单独算子的通过结果称作真实 GPT-2 推理证明。未完成项见 [ACCEPTANCE_GAPS.md](ACCEPTANCE_GAPS.md)。

## 实际工作区与来源

远端工作区为 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT`，HEAD为 `7be84bdb5b41132c31ba556784580ddd2c66c4fc`，分支 `main`。初始完整 Git status、源码及B1/B2/B3 P0/重设计/审查报告在 [BASELINE.json](BASELINE.json) 中；最终状态在 [FINAL_EVIDENCE.json](FINAL_EVIDENCE.json) 中。已有 `g1_0402.npy` 删除及未跟踪用户工作被保留。

正式规范采用 [USENIX Security 2025 zkGPT](https://www.usenix.org/system/files/usenixsecurity25-qu-zkgpt.pdf)，PDF SHA256为 `2acc64d6b27eaed48cd4895aaec1b5cca8acc7473e7ff8015c041c98b9b35ff2`。官方上游固定到 commit `8fcde848e96da815ce3324fa5e25f605d290402c`、tree `20272606810ab2b3f067669f15dd2e40856f4b48`；34个blob均核对实际Git对象哈希，见 [UPSTREAM_VERIFIED.json](UPSTREAM_VERIFIED.json)。Lasso依据 [原论文](https://eprint.iacr.org/2023/1216)；本轮重新读取远端保存的59页PDF并核对哈希，未把403的在线PDF请求说成下载成功。正式20页zkGPT与缓存19页版本的完整Protocol1规范化文本一致，见 [PDF_IDENTITIES.json](PDF_IDENTITIES.json) 和 [PROTOCOL1_VERSION_CHECK.json](PROTOCOL1_VERSION_CHECK.json)。

## 新架构与关键裁决

新实现位于远端 `src/paper_r1/`；本地逐字节镜像位于 [delivery/src/paper_r1](delivery/src/paper_r1/README.md)。独立CMake构建，不调用 strict-v4、B1/B2 的协议实现，不接受旧格式或旧anchor。根生产入口尚未切换。

- PC使用明确shape/layout/role/domain的balanced行承诺和MLE IPA。旧Range实际commit/open为balanced column-major，P0的固定4096形状描述不能作为其密码学身份。新代码从实际维数计算并验证布局；没有仅改旧metadata来声称修复，也没有为保留mixed-C0建立桥梁。
- Range采用Lasso identity-table digits，末段位宽精确；signed bias乘active mask，补零通过承诺后随机加权的零尾证明认证。实例数由可信查询定义决定，不沿用81/87/88。
- Exp采用明确无clamp数学实例 `round(2^20 exp(-t/512))`、u32索引；8192项由正有理Taylor区间逐项认证。原始(t,E)、分支、压缩地址、合法native padding及indexed lookup均受PC绑定。这是独立数值实例，尚未证明完整GPT profile采用它。
- 通用GKR、专用矩阵sumcheck、四块输入一致性和终端W/aux/L opening已实现。Protocol1(7)的L系数使用可验证四块插值修正，并明确标为本项目代数修正，未冒充作者勘误。
- 公开与秘密正分母半开取整、真正最大值、正域整数sqrt已实现。秘密B*q与sqrt的y²通过eq加权degree3 sumcheck认证，终端打开原始源PC；没有用MLE乘积冒充逐坐标乘积。正域sqrt不自行定义论文印刷式未覆盖的零域。
- FS采用SHA256带标签/长度的framed hash chain、canonical Fr拒绝采样。独立Verifier仅链接 `R1_VERIFY_ONLY` 库；规范解码检查确切shape、位宽、轮数、canonical Fr/G1、资源上限及完整文件消费。

[PROTOCOL_MAPPING.md](PROTOCOL_MAPPING.md) 逐项列出数学定义、源码、Prover行为、Verifier接受条件、测试及限制；[SPEC.md](SPEC.md) 记录依据与裁决；[INVENTORY.csv](INVENTORY.csv) 对127个旧代码/构建/表文件给出唯一去留状态、替代模块和退休门槛。

去留状态为 KEEP 3、REWRITE 15、REPLACE 98、DELETE 2、BLOCKED 9。DELETE指旧LogUp的退休决策，**实际删除数为0**；新基础库尚未通过完整模型替代验收，故没有提前移除旧生产路径。B1/B2不默认复用，旧hiding原型也没有被用于宣称完整ZK。

## 构建、独立验证与拒绝证据

实际远端执行命令如下，完整stdout/stderr/退出码保存在 [final/logs](final/logs/LastTest.log)，摘要和哈希见 [FINAL_EVIDENCE.json](FINAL_EVIDENCE.json)。

```sh
cmake -S src/paper_r1 -B phase_b3_r1_20261018/build -DCMAKE_BUILD_TYPE=Release
cmake --build phase_b3_r1_20261018/build -j4
ctest --test-dir phase_b3_r1_20261018/build -V --output-on-failure
```

Release套件 **13/13通过**。包括PC/FS、memory、Range、input、GKR、矩阵、整数、Exp、秘密分母、正域sqrt、完整指数表证书及优化Python模式检查。基础库使用 `-Wall -Wextra -Wpedantic -Werror` 构建。

独立进程 **32个案例全部符合预期：5个接受、27个拒绝**。包含两份显式选择的合成模型、合成Bundle、独立Exp/除法/sqrt；篡改覆盖查询、read counter、memory root、GKR消息、终端L、公开X/Y、模型承诺/替换、表、transcript、正域binding、非规范Fr/G1、超长计数、截断和尾随。Verifier工作目录没有witness或真实模型文件；`nm -C`检查无Prover/open入口。实际statement/proof与每例退出码保存在 [final/artifacts](final/artifacts/results.json)，非仅统计数字。

ASan覆盖：Exp临时迭代器越界的RED→GREEN；最新秘密分母12项检查、正域sqrt11项检查，以及ASan版本的全部32个跨进程案例通过。没有声称所有单元测试均在ASan下运行。

完整失败与修复记录保留：W/aux尾部缺约束、错误oversized fixture、Exp临时Digest迭代器越界、Python `-O`关闭证书assert、MCL将直接构造的UINT64_MAX解释为−1所致sqrt测试数据错误。最后一项经独立最小probe定位，改canonical字节解码，未放宽sqrt关系。审查范围与结论见 [INDEPENDENT_REVIEW.md](INDEPENDENT_REVIEW.md)。

这些测试证明覆盖输入的实现行为，不能替代密码学安全性证明。

## 模型绑定、安全性及完整目标缺口

120个现存 `data/gpt2_int` 参数文件与3个旧anchor已重新哈希记录，未改变字节。full12旧anchor摘要仍为 `464e722650b16c5205d6f0576a1059571a6160e032bd8ad30b4d618a22b31de3`。其声明来源仍是 `unverified-local-quantized-gpt2`；本轮没有把它认证为原始官方checkpoint，也未生成新compact-W真实模型anchor。

**尚未交付：** 全部Linear/Attention/Softmax/LayerNorm/GELU/Residual/fused算子的论文一致整数profile及可信全局witness/query builder；Attention scale和量化参数的完整推导；真实full12的X/W/Y推理及新协议证明；可支持完整模型的调度/codec；hiding PC、全协议masking、模拟器、joint knowledge extractor和完整组合安全归约。

其中数学语义未确定的部分明确受阻；全模型builder、调度和集成则是尚未完成的实现工作，不伪称已由外部条件阻塞。当前成功证明全部是明确标注的合成电路或独立算子。

条件化soundness依据、Lasso计数链、sumcheck误差、field lifting、PC/FS假设及剩余义务见 [SECURITY_OBLIGATIONS.md](SECURITY_OBLIGATIONS.md)。确定性PC与未mask消息**不具完整零知识**，没有依据宣称完整knowledge soundness或具体128-bit组合安全。

## 实际变更与保护核对

[NEW_FILES.json](NEW_FILES.json) 记录46个新交付文件的实际SHA256；[CHANGELOG.json](CHANGELOG.json) 记录新代码、文档及已核对旧文件的保护范围。初始216文件、补充140项（含被初次扫描遗漏的旧`.inc`及历史PDF）、120参数文件、3个anchor在最终读取时均与各自baseline一致。初次扫描遗漏没有被改写成“起初已完整覆盖”。

没有执行禁止的Git操作，没有提交Git，没有删除旧生产代码，没有重新标识旧anchor。当前交付是可继续审查和开发的实际代码增量；完整目标保持未验收。
