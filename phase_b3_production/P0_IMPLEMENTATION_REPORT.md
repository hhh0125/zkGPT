# B3 P0 可信布局与完整生产来源合约实现报告

状态：**P0 工程验收通过，仅限来源合约与元数据验收。** 2026-10-18 续跑；最终取证 UTC：`2026-10-18T02:38:11.178570+00:00`。
真实 full12 两个 CTest 均通过，最终退出码 0。本次止于 P0；没有进入 P1，没有构造私有模型 witness、执行完整推理、生成 proof 或提交 Git。**B3 仍未冻结，完整 zkGPT 论文协议尚未复现。**

## 基线、规格与保留检查

远程根：`/home/yttan/project/zkGPT/zkGPT-main/zkGPT`。HEAD 前后均为 `6164980bf55bf5b8d6d8fc8417a077b92cf7bf53`，分支 `main`。
初始已有 `D g1_0402.npy`、`M src/CMakeLists.txt`，及 B1/B2/B3 等未跟踪内容；均保留。起始 `src/route_b/`、`phase_b3_production/`、指定 build 目录均不存在。未执行 reset、clean、checkout、stash 或 commit。
收尾对基线 **129 个文件 SHA256 完全相等**，并逐字节验证原 `src/CMakeLists.txt` 是最终文件的完整前缀，差异恰为新增默认 OFF 开关的六行。八个本次源码文件的远端/本地 SHA256 相等；`git diff --check` 退出码 0。详见 `evidence/final-integrity.json`。此覆盖范围不冒称包括全部模型数据或所有历史 fixtures。

| 起始存在且最终保持不变的依据 | SHA256 |
|---|---|
| `PHASE_B3_DESIGN.md` | `7c3ea76950ebb75cce1e0f89c53d40e587b4a3756955630a880d0ea188cb73c7` |
| `phase_b3_protocol_freeze/PROTOCOL_FREEZE.md` | `8bc67fcc884102b3c6dbf752e043457a6b581936d712648275bde1365388ca22` |
| `phase_b3_direct_query_audit/DIRECT_QUERY_PLAN_AUDIT.md` | `94d2cdbe4eff1b40a38f2c878f95df312d6d6b558f2b7432f0d6b18ce3d1157c` |
| `B3_PRODUCTION_DIRECT_QUERY_MIGRATION.md` | `48fa892b1af45065aa2220e511767663c0790f50f620dee24e5f6fb0d3f25f85` |
| `task9_logs_20261014/full12.anchor.bin` | `464e722650b16c5205d6f0576a1059571a6160e032bd8ad30b4d618a22b31de3` |
| `task9_logs_20261014/full12.statement.bin` | `cef1dcfab8d64c4df4e0ea72926cb35a667cc11f46b068b15bc62d78903f54ac` |

迁移计划 §2 明确将旧 6733/589 来源合约增量扩为 6735/591，加入 constants/Y；不是放宽旧规格。§8.2–8.4 的完整 parent padding 和 indexed Exp 条件一并纳入。没有用历史 manifest、statement 内容或静态计数代替实际 full12 验收；statement 仅检查文件身份。
部署策略显式固定上述 publisher anchor、模型 ID `d3c62c91c331e828c1141440ac1822b3da3e26c3ba7d8c28cf4c42e50a127c65` 与实现 profile `3965ecaa02a1bd52181cf3d5e6ec40ffb5b785e9ac3b51b876d15dd03d779ca8`。沿用现有本地 publisher 信任，不证明参数来自官方 checkpoint；anchor 仍标记 `unverified-local-quantized-gpt2`。

## 变更与生成逻辑

仅对既有 `src/CMakeLists.txt` 追加 `ZKGPT_ENABLE_ROUTE_B_PRODUCTION`（默认 OFF）和条件子目录。新增：

- `src/route_b/production_layout.hpp/.cpp`：部署策略、scalar append 重建与实际电路/registry 核验。
- `src/route_b/source_plan.hpp/.cpp`：SourceId/SourceSpec/ObligationSpec、两种 ExpectedPlan 及逐字段拒绝接口。
- `src/route_b/CMakeLists.txt`：独立 `route_b_production` 库、两个固定 targets 与两个固定 CTest。
- `src/route_b/tests/source_plan_tests.cpp`、`source_plan_emit.cpp`、`source_plan_independent.py`：真实 full12 测试、公开元数据导出与独立核验。

新增对象只编入 `route_b_production`，该库复用已有 C++17、MCL BN254/Fr 和 `gpt_lib` 公开构建器。实际默认 OFF 配置及 legacy DependInfo 均确认新源码未混入 `gpt_lib`；没有修改 `neuralNetwork.cpp/models.cpp` 或旧 B1/B2。

`LLM(12,12,64,768,3072).buildPublicCircuit()` 生成真实电路与 WitnessRegistry；独立 scalar append 规则逐项核对全部 registered regions、Exp 行、模型参数区域、常量 append 顺序及最后 residual advice 输出位置。普通层检查 ori_id 实际长度、每项边界、port 内唯一性和 gate 首次出现顺序；FCONN 对每项核对 row-major activation/weight 位置，分别保留 p0/p1 的 u/v。continuation 不登记为输入来源。
每个 ori_id 都进入无损的连续 span 描述；span 覆盖不以 hash 代替。当前电路 fingerprint 也与认可 anchor 比较，但它只是额外身份检查。
SourceSpec 包括 family/index/port/point、类型、m/p/d、shape、projection/bias、bits/signed、padding、alias 规则、完整 PC profile/引用和局部依赖。ObligationSpec 单独登记所有 GKR/zero、Range chunks/reconstruction、Exp branch/membership/openings、模型/公开输入及 constants/Y/tail 义务。

两种模式由 verifier-owned policy 固定，候选元数据不能改变模式。Lasso-v2 的 81+6 个 membership metadata 明确 query_length=p、无额外 native padding；Exp E-indexed query 精确引用 `exp/vector/1`（C_E），dim 精确引用 `exp/vector/3`（C_x），rows=256、columns=512、length=131072。table 为 8192 项 canonicalExpValue，K=6891 后补 1；Bridge-v1 仍登记 pair alpha/P/LogUp 条件。两种 source 的 Exp 依赖均覆盖全部六个 memberships。

## 真实统计及独立性

真实 shape `(32,12,12,64,768,3072)`，n0=195414944、N0=268435456、ell=28。

| 验收对象 | 实际重建并核验结果 |
|---|---:|
| Range regions / groups / chunks | 624 / 10 / 81 |
| Exp operators / rows | 12 / 76032 |
| GKR input endpoints | 576 |
| Used constant-one positions | 96 |
| Public Y offset | 195382176 |
| zero-tail support length | 73020512 |
| PRE-subset raw input accesses | 333748992 |
| Complete deduplicated mapped positions | 215829636 |
| Bridge-v1 sources / local obligations | 6735 / 6946 |
| Lasso-v2 sources / local obligations | 591 / 802 |

Bridge-v1 family 计数为 576/10/2/6144/1/1/1；Lasso-v2 为 576/10/2/0/1/1/1，依次对应 GKR、Range Z、Exp、GeLU direct、tail、constants、Y。
12 个 GeLU t 区域由每 block 的全部 512 个 256 项 tile 精确覆盖。96 个常量按 append 顺序重建并确认被 gate 使用；首 block 八位置符合任务要求，其后各增 2640120。Y 的实际公开维度为 32×768，逐行补至 32×1024；该 source 认证已存储的 32768 个位置及零通道，绝非最后检查层的零值 MLE。tail 在同一 C0 的完整域上使用 mask，support 为 `[n0,N0)` 的 73020512 项。

独立 Python 进程自行启动受控 emitter，复算部署/profile/Exp table、scalar append、region 分组和两个来源/局部义务目录；不调用 C++ buildExpectedPlan。emitter 仅在测试可执行文件用 linker wrapper，在真实 initSubset 改写 gate 前导出全部原始输入访问；Python 对完整访问流按首次出现去重，再与每个实际 ori_id span 逐项语义比较。FCONN 原始 row-major 位置另外按 scalar 规则逐项核验。所有字段递归严格比较类型，拒绝 bool/int/float 混用。
这份 gate 流是本地认可公开构建器的执行证据，不是 artifact 自报可信映射；两方共享认可的公开构建器/部署根，但**不共享来源生成函数或去重实现**。全部 624 regions、81 chunks、76032 Exp 行、576 endpoints、96 constants 和 Y/tail 都检查，未抽样或缩小范围。

## 实际命令、拒绝测试和资源

所有命令均在远程根执行；stdout/stderr、退出码 JSON 和 `/usr/bin/time -v` 原文分别保留在 `evidence/`。CTest 运行环境固定 `ZKGPT_THREADS=4`。
初始红灯命令 `g++ -std=c++17 -fsyntax-only -I src/route_b src/route_b/tests/source_plan_tests.cpp` 实际退出 1，按预期报告缺失 `source_plan.hpp`，原文见 `evidence/red.json`。审查回归的七项检查均失败，原始 expected/actual 见 `review-red.json`（逻辑 actual_exit=1，取证脚本自身 SSH 退出 0）；这些失败没有算作最终通过。

```sh
cmake -S . -B phase_b3_production/build -DCMAKE_BUILD_TYPE=Release -DZKGPT_ENABLE_ROUTE_B_PRODUCTION=ON
cmake --build phase_b3_production/build --target route_b_source_plan_tests route_b_source_plan_emit -j4
ctest --test-dir phase_b3_production/build -R '^route_b_source_plan_' --output-on-failure
cmake -S . -B phase_b3_production/build-default-off -DCMAKE_BUILD_TYPE=Release
```

预期 configure/build/default-off 均退出 0、最终 CTest 为 2/2 通过。实际：

| 日志名 | 退出码 | wall 秒 | user / system CPU 秒 | 峰值 RSS KiB |
|---|---:|---:|---:|---:|
| `configure` | 0 | 1.14 | 0.84 / 0.25 | 26284 |
| `build` | 0 | 54.40 | 112.68 / 5.06 | 353224 |
| `ctest-01` | 8 | 884.26 | 935.38 / 99.53 | 19380580 |
| `build-02` | 0 | 15.73 | 15.03 / 0.68 | 336628 |
| `ctest-02` | 0 | 1007.29 | 1045.28 / 107.51 | 18527952 |
| `default-off` | 0 | 0.60 | 0.45 / 0.13 | 26420 |

最终独立 emitter 实际退出 0，wall 127.44s，user CPU 166.37s、system CPU 23.90s，峰值 RSS 18527952 KiB；独立 Python 峰值 RSS 18397648 KiB。完整 manifest 和 raw gate 流保留在最终 `build/source-plan-full12-*` 目录，另有 `.resource.json` 与 `.verified.json`；未因内存压缩验收范围。资源足够，无 OOM 或资源阻塞；这些测量不能代替 P10 的部署预算或证明性能。

24 个 C++ 负例逐项预期 reject、实际 reject，均在零个 proof challenges 时返回：漏 FCONN p1、continuation 冒充 input、offset/bits/signed/length、重复/漏 ID/换序、伪 model/profile、越界映射/alias、漏 tail、漏/移动常量、漏 Y/换 Y offset、计数正确语义错误、artifact 自报策略、PC 身份/局部依赖、模式降级和局部义务遗漏。独立进程还对每种模式的 offset、bits、signed、PC、dependency、constant、output 负例，以及嵌套 JSON 类型错配进行真实拒绝检查。每项 expected/actual 原文见最终 LastTest.log；没有改动验收预期。

第一次测试的 exit 8 保留在 `ctest-01.*` 和 `pre-fix-LastTest.log`：负例将 0 改回 0，属于独立测试缺陷，不是资源不足或通过证据。只读审查另发现 Exp 依赖遗漏、Lasso metadata 错用 Bridge pair 及嵌套 JSON 类型问题；`review-red.json` 在真实已导出的清单上复现。修正后完整重新构建、重跑两个 CTest，最终全部通过；既不将旧失败删去，也不将旧 Bridge 检查冒称 Lasso 已通过。

## 审查决定、边界与任务清单

一次 fresh-context 只读审查发现的四个 Important 问题均修正并有失败/通过证据。保留一个 Minor 内部 API 限制：writeLayout 对调用者手造且 ports 被截断的 PublicModelLayout 缺少结构性防护；当前 P0 入口只使用 verifier 从实际公开构建器重建的 layout，artifact 不能提供此对象。未来若将 layout 作为外部输入，须先加入边界检查。
决定：按用户要求在指定 dirty tree 中增量工作，不切 worktree/commit/stash；沿用已经授权的迁移设计；TrustedDeployment 必须是 verifier-owned 的部署认可，不能从 artifact 反序列化。若调用方越过此信任前提，来源认证结论无效。审查本身只看 fresh local snapshot，远端保留结论由本次独立字节/哈希/Git 收尾检查支持。

P0 建立的是已有 mixed-C0 布局和来源合约，属于工程迁移，不是论文字面四块输入布局或完整协议的实现。所有 local proof、模型/X、Y、常量及 padding **仍是必须在后续协议中真正证明的义务**，没有因登记 metadata 而被视为已满足。统一 Consistency Sumcheck、生产 Lasso、公开 I/O 新格式、transcript/FS 组合、PC knowledge/ZK、安全证明和完整推理验收均未完成，本次不实施。

- [x] 新鲜 HEAD/status、已有 CMake 修改、必需文档/anchor/statement 身份及目录占用检查。
- [x] 从实际 full12 public builder/registry 重建可信布局和全部映射。
- [x] 两种完整固定来源及局部义务目录逐字段验收。
- [x] 独立进程完整映射、全部生产条目和负例验证。
- [x] 保存首次失败、审查回归、最终成功日志与 CPU/RSS。
- [x] 收尾保留检查、报告交付；停止于 P0。
- [ ] P1–P11、B3 冻结及完整论文复现：本次不实施，也不声明完成。
