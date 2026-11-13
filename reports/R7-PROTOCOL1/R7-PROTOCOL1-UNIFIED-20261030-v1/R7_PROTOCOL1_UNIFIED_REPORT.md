# R7 Protocol 1 Unified Input 实测交付报告

日期：2026-10-30T13:53:58.241274+00:00。本轮真实 Oneblock 和 Full12 Prover/独立 Verifier 均退出0；Oneblock 5358/5358、Full12 8112/8112 动态必须关系全部 VERIFIED。16个 Oneblock 恶意输入、4个 Full12 选定恶意输入均由新独立 Verifier 明确拒绝（退出2）。工程验收通过；安全状态仍为 `FS_NIZK_CANDIDATE / BLOCKED_CRYPTOGRAPHIC_DESIGN / may_enter_stage2=false`。

本轮端到端实测：P wall 7023.895146s、V wall 1058.691154s、Proof 261504622 bytes。相对同配置最新OPT3-B，P wall增加 110.866%，V增加 3.949%，Proof减少 0.836%。Prover的公开K重建次数由45增至1485，计入所有时间与分配统计。

## 环境、源码和执行边界

Windows 权威源码 `C:\Users\tanyt\Desktop\program\zkGPT-R7-dev`，分支 `r7/remove-full-base-registration-20261029`，Git HEAD `8365fc13613a5269970f1fdef83a1cb883a45606`。基线为当前包含 No-FullBase 与 OPT3-A/B 的394文件未提交工作树，完整字节保存在 `before/`；本轮397源文件、13项增量修改，以 SHA 清单识别，不以 HEAD 代替源码身份。

全部源代码编辑在 Windows 完成，使用既有 `4090_sub2api` SSH 配置逐文件 SCP 精确同步并进行前置冲突检查、同步后 SHA 检查。Linux 仅在固定 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work` 编译、测试及产生证明；未运行远程 AI 或源码生成脚本。隔离 Release 构建 `build/protocol1-unified-release`，既有 `build/release` 及所有历史证明/Anchor/日志保持保留。未提交、推送、合并、重置、清理或访问 GitHub。

实际运行四 CPU、`ZKGPT_THREADS=4`、`MALLOC_ARENA_MAX=2`。每5秒记录进程 VmRSS/VmPeak、共享内存、cgroup、负载和磁盘，低内存连续3次或磁盘不足时只停止本任务进程组；本轮未触发停止。无8GiB RLIMIT_AS、无全局配置修改。峰RSS来自 wait4，VmSize为5秒采样的VmPeak。负例并行进程共享同一组四CPU，总计不扩成16CPU。

真实 Full12 12 Blocks/277层/48FC/76032 Exp查询/120参数，45原物理根、10可信W分块、1502认证声明、11跨Block连接。模型/数值/原可信映射/原Anchor未改变。Full12 Statement SHA `ce1f1c7253802da5ceb90355c8c34b0547f4febe710fe6ef2691e14271fcb76b` 与比较基线完全一致。原Anchor pin `02ed086e3a5b596e193ded8ede2e5de5da38a1358ff628068cea1e483e1e90c1` 保留。每轮从真实R6计算生成 Witness；未复用旧Proof、旧进程私有状态或私有检查点。

## 实现与论文关系

### Protocol1 mathematical and code correspondence (candidate)

2026-10-30. Source: https://www.usenix.org/system/files/usenixsecurity25-qu-zkgpt.pdf Appendix A, Protocol1(6),(7), printed pages2061-2062. This is a minimal implementation correspondence, not a renewed full R7 audit. The printed final selector expression repeats a category coefficient; the code uses independently tested little-endian four-way multilinear selectors rather than copying that expression.

| Relation / responsibility | Actual candidate path |
|---|---|
| Native arithmetic/FC terminals (both ports and matrix points) | `oneblock_gkr.cpp::runHiddenGKR`, `classified_controller.inc` loop over `pending.terminals`, verifier-derived `ori_id_u/ori_id_v`; exact equality with required GKR_MASTER_TERMINAL IDs |
| Actual Range/Exp Lasso plus Memory/Product provenance | `oneblock_lookup.cpp::hiddenRange/hiddenExp`, `VectorOracle::open`, `Commitments::view/defer`; all sources and helper objects constrained by existing native plan |
| Native master coordinate to existing StageC physical root | `classified_layout.cpp::locate`, `partitioned_layout.cpp::baseCoordinate`; original trusted layout/digest preserved |
| Unified virtual X/W/aux/L layout | `unified_input.cpp::pack/validate`; `classified_commitments.cpp::unifiedInputIdentity`; explicit statement X, trusted root category/order, dimensions and aligned offsets; hash bound in FS instance and live authentication |
| Original mapped-claim identity and affine/constant terms | `Commitments::defer/mapDigest/prepareClaim`; all prepare passes before rho; original OPT3B RAM/file map budgets; frozen maps callback-free, exhausted rebuilds digest-checked |
| Preparation of the input sum target and public wiring polynomial | `Commitments::authenticateUnifiedInput`: A=sum_i rho_i(h_i-B_i), K(z)=sum_ij rho_i eq(q_i,j)1[z=offset(root)+index]; multiplicities accumulate; biases/virtual constants removed from target |
| Protocol1(6), actual single global hidden input Sumcheck | `unified_input.cpp::quadraticRound/reduce`; sum_{Boolean z}U(z)K(z)=A, degree2, one common r across complete4N virtual domain; coefficient commitments + recurrence Sigma before each challenge |
| Input Sumcheck final product | `reduce` verifies hidden U(r) times independently computed public K(r) equals the final hidden carry, using existing Sigma products |
| Protocol1(7), classified hidden PC openings | `authenticateUnifiedInput` opens every original physical root at r[:log2(root.padded)] via `rootOpening`, existing `openVector` / trusted `model::opening`, RowMajor Hyrax A.3 |
| Logical concatenation and model chunk stitching | Hidden relation U(r)=selector_X*X_MLE(r[:dX])+sum_k selector_k*root_k_MLE(r[:dk]); all roots included even with zero selector/kernel. No new physical master commitment. W root order remains trusted model chunk order. |
| Original model identity and base tails | Existing constructor `registerModel`/Full12 chunk trust; existing `constrainBaseTails`; no full base-row knowledge registration restored |
| Transcript/no downgrade/independent verification | Explicit OneblockUnified / Full12Unified FS proof types and new suites; `FsVerifierIO` reconstructs challenges from new disk Proof and separately loaded Statement; `finish()` validates footer and EOF |
| Verifier-local coverage | Candidate `oneblock_main.cpp` required inventory and `classified_controller.inc` completion only after all new checks; no peer coverage receipt accepted |

R6 reference `gkr_proof_builder.cpp::buildLassoProof` is input-layer wiring reduction, not the actual Range/Exp Lasso. Its plain input evaluation/opening path is never used as fallback by this candidate.

Storage semantics: zero virtual alignment holes and category tails are implicit and allocated nowhere. Physical root blocks use the complete committed padded vector, with original registered-base/model-tail constraints. Existing lookup helper logical padded values retain their original native constraints; unused padding inside an unregistered helper commitment is not newly asserted to be zero by the virtual layout. Every committed block opens with its original padded dimension. The existing public-X/Y/constants claim also binds native copies of public data, while X is an explicit public logical partition.

No 2^28 private input copy, root folded witness copy, or new mixed val[0] commitment is built. One public K physical-root buffer is used at a time. Streamed coefficients re-read sealed root values; repeated public kernel construction is included in timing. Split equality tables and a sparse suffix map have bounded size. The last scalar-block merging includes crossroot/crosspartition terms for BOTH factors, including public X even where Boolean K_X=0.

Security status: FS_NIZK_CANDIDATE / BLOCKED_CRYPTOGRAPHIC_DESIGN / may_enter_stage2=false. Exact algebra/real P-V testing is engineering evidence; there is no new formal proof of joint knowledge soundness, zero-knowledge simulation or Fiat-Shamir/Sigma/Hyrax composition.


### 数学、映射、挑战顺序与内存

#### R7 Protocol 1 hidden unified input design

2026-10-30. Authorized spec: user's attachment 75f39a11-a940-411c-8ecf-974e91caabda. Implement inline, Windows edits and exact-byte SCP only; remote builds/tests only. No commits, deletion, model/numerical changes or remote AI. Frozen current OPT3-B baseline394 source files is saved under Windows zkGPT-backups/r7-protocol1-unified-20261030-v1/before. Local HEAD8365fc13613a5269970f1fdef83a1cb883a45606. Remote Git is an ancestor repository, not this source identity. Migration success and all394 local/remote SHA match.

##### Intended relation

Implement Appendix A Protocol1(6),(7), not a rename of existing root-wise reductions. Every existing deferred hidden GKR (including both FC ports/matrix points), true Range/Exp Lasso and helper/Memory/Product/source claim remains required. Stage C trusted coordinate callbacks and digests remain the source of truth. Public X/Y/constants binding remains mandatory. Existing original root and model Anchor identities and zero-tail proofs remain unchanged.

Define U=X||W||aux||L with four equally padded power-of-two partitions N. X is the canonical public input embedding from the Statement. W consists of model roots in existing logical model chunk order. aux consists of C_aux roots. L consists of C_lookup/queries and C_lookup/helpers roots, including all existing Lasso auxiliary values needed by the actual implementation. Each physical root occupies one aligned power-of-two subcube in its category. Alignment holes and partition tails are zero. Public Y/constants and copies of X within native aux/query roots remain constrained by the existing trusted public-entries claim; including public data among aux does not make it secret. No physical copy of U is created.

For deferred claim i with hidden value h_i, local point q_i, trusted map m_i(j) and affine bias b_i(j), compute B_i=sum_j eq(q_i,j)b_i(j). For constants, the whole value is bias; mapped nonconstant coordinates contribute one root value plus bias. After fixing all declarations and validated maps, sample rho_i. Define A=sum_i rho_i(h_i-B_i) as a hidden linear combination; define K(z)=sum_i,j rho_i eq(q_i,j)[z=offset(root_i(j))+coordinate_i(j)]. K is public; multiplicities, zero/negative strides, virtual padding and crossroot mappings are preserved. The target is A=sum_{z in {0,1}^{log(4N)}} U(z)K(z).

Run ONE degree2 hidden input Sumcheck over the entire4N domain, drawing one common coordinate per round AFTER binding all hidden coefficient commitments and their recurrence Sigma proof. This ends at hidden U(r), public K(r). Verify terminal multiplication using existing hidden arithmetic Sigma. No root partial-sum commitments or separate root-level expressionSumcheck in the new path. Random aggregation is only preparation for the actual global Sumcheck.

##### Streaming prover and classified opening

Represent U and K as disjoint aligned root subcubes plus one public-X subcube. For round t, compute each root's partial low-bit folds from original sealed root values and its public K buffer. Split equality tables need O(sqrt(root padded)) fields. Traverse values directly, without private full-domain or per-root value copies. When t is less than a root dimension, contribute its local paired quadratic coefficients. Once t reaches that dimension, accumulate its folded scalar pair in a bounded sparse map indexed by the remaining global Boolean suffix, multiplying the fixed-offset low-bit selector. Merge scalar contributions BEFORE multiplying, so crossroot and crosspartition cross terms are included. Domain holes contribute0. Only one public root K buffer (existing max2^24) is live; no unified2^28 buffer, no private checkpoint/fallback. This intentionally trades repeated scans for bounded memory; report actual cost, do not predict a gain.

At the final common point r, form a hidden scalar for every physical root evaluation at r[0:log(root.padded)]. Verify its existing hidden Hyrax A.3 opening, with trusted model rerandomization/Anchor binding for W. Let s_k be the fixed-offset selector on the remaining high bits. Verify the hidden linear relation U(r)=public_X_MLE(r)+sum_k s_k root_k_MLE(r). This includes each classified root exactly once and preserves every zero-tail and fixed padding constraint. Verifier computes K(r) independently from the validated maps and the same rho values, using only one root K buffer at a time. No Prover-chosen locations or values are trusted. No hidden evaluation becomes plaintext.

##### Rollout and acceptance

Builder check completed: the canonical U serialization must bind N, category/order/root identity, every aligned offset/padded/actual dimension, public X bytes and all zero gaps before rho; put its digest in the new public FS instance. All mapping preparation runs before any rho (separate passes). For a root of size2^d at offset o, when t>=d the scalar contribution is localMLE(r[:d])*product_{b=d}^{t-1}eq(r_b,bit_b(o)); apply this separately to U and K before merging suffixes o>>t. Public X participates even when its K=0. Every root is opened using its committed padded dimension, regardless of zero selectors/kernel. Never use expressionSumcheck's materializing driver or merely raise its2^24 bound. Remaining security composition is explicitly unproved. No user clarification is required by the builder findings.

Keep OPT3-B old authenticate as an explicit reference path during testing; introduce separate opt-in Unified entrypoints and suites until real Oneblock positive/negative acceptance. Only then route current production entrypoints to the new mechanism and mark old root barrier nonproduction/reference. Distinct FS suite/instance binding rejects old proofs, while keeping hash challenge sampling rules and actual Stage C layout identity. Never mark authenticated until global Sumcheck, all openings and stitching pass. All coverage transitions remain verifier-local and depend on successful new path.

Before proof runs: red/green native algebra tests against independent dense U,K on tiny fixtures; test every round coefficient with uneven roots, zero gaps, all4 categories and late crossroot/crosspartition merging. Hidden protocol tape replay must reject wrong claim/round/endpoint/Opening; full production tests cover actual GKR/Lasso source omissions and root/model/Statement swaps. Run affected plus complete CTest in a new project-local Release build directory using existing toolchain/dependencies; do not overwrite existing successful production build. Real Oneblock P must exit before fresh disk-reading V; all specified malicious cases independently rejected. Only after these gates run real Full12 P then independentV with fourCPU resource monitor and unique paths. Report actual message counts/EOF/identities, dynamic coverage, opening and round counts, RSS and times versus archived OPT3-B. No claim of formal knowledge soundness/ZK/FS composition; security remains candidate/blocked/stage2false.


实际全局 Sumcheck 轮数 P/V 均为 32，虚拟域 4294967296，每分区 1073741824；两个私有完整输入副本计数均为0。原认证入口仅在真实 Oneblock 正反例、归档恢复检查通过之后切换；6个生产完整入口均选择新协议。旧 authenticate 保留在明确的reference/probe/unit路径，没有承担当前完整流程认证。切换前生产 Oneblock ELF 与当时实际测试候选逐字节一致；虚拟域边界修正后，重新用当前实际生产P/V生成全新Oneblock-v3 Proof并完成独立验证及16项负例，之后才运行Full12-v2。新交互 READY、新FS suite/type 不允许自动降级；旧Proof在新入口不兼容。

注册统计 `registered_rows=0` 与 `registration_response_bytes=0` 专指已删除的完整基础行注册；zero-tail独立统计。本轮54个tail行、15040响应字节，10个可信模型分块绑定仍实际执行。Coverage文件的历史 `direct_full_registration_response_bytes` 是潜在完整注册的理论大小字段，不能当作实际通信量。除去旧 `CLASSIFIED_AUTHENTICATION` 1项，原所有必须关系逐项保持一致；新增1个全局Sumcheck、1个统一Opening、45个物理根Opening，实际动态计数 8112，未硬编码8066。

Coverage中新全局关系条目的通用 `domain` 字段沿用native master实际坐标数、`sumcheck_rounds=0` 为聚合关系的发现元数据；它不是实际虚拟Sumcheck域/轮数计数。真实执行的虚拟域和轮数来自FS绑定及 `UNIFIED_INPUT` / `UNIFIED_SUMCHECK` 日志，以上已分别记录，未混用这些字段。

## 实测性能

| 指标 | OPT3-B 实测基线 | 本轮实测 | 绝对变化 | 变化百分比 | 旧/新 |
| --- | ---: | ---: | ---: | ---: | ---: |
| prover_wall_seconds | 3330.973577 | 7023.895146 | +3692.921569 | +110.866% | 0.4742x |
| prover_cpu_seconds | 3413.454836 | 7105.085644 | +3691.630808 | +108.149% | 0.4804x |
| prover_peak_rss_kib | 52077412.000000 | 52071256.000000 | -6156.000000 | -0.012% | 1.0001x |
| prover_peak_vmsize_kib | 52742392.000000 | 52741880.000000 | -512.000000 | -0.001% | 1.0000x |
| verifier_wall_seconds | 1018.475648 | 1058.691154 | +40.215506 | +3.949% | 0.9620x |
| verifier_cpu_seconds | 1074.318657 | 1107.817749 | +33.499092 | +3.118% | 0.9698x |
| verifier_peak_rss_kib | 6312208.000000 | 6308632.000000 | -3576.000000 | -0.057% | 1.0006x |
| verifier_peak_vmsize_kib | 6643596.000000 | 6641032.000000 | -2564.000000 | -0.039% | 1.0004x |
| proof_bytes | 263710426.000000 | 261504622.000000 | -2205804.000000 | -0.836% | 1.0084x |
| authentication_sumcheck_instances | 45.000000 | 1.000000 | -44.000000 | -97.778% | 45.0000x |
| authentication_sumcheck_rounds | 1064.000000 | 32.000000 | -1032.000000 | -96.992% | 33.2500x |
| classified_Hyrax_root_openings | 45.000000 | 45.000000 | +0.000000 | +0.000% | 1.0000x |
| prover_kernel_calls | 45.000000 | 1485.000000 | +1440.000000 | +3200.000% | 0.0303x |
| prover_kernel_coordinates | 2461708448.000000 | 81236378784.000000 | +78774670336.000000 | +3200.000% | 0.0303x |
| prover_kernel_cpu_seconds | 68.761300 | 2254.030000 | +2185.268700 | +3178.050% | 0.0305x |
| prover_map_cpu_seconds | 419.344000 | 412.638000 | -6.706000 | -1.599% | 1.0163x |
| prover_kernel_peak_bytes | 536870912.000000 | 536870912.000000 | +0.000000 | +0.000% | 1.0000x |
| prover_MSM_calls | 1281061.000000 | 1268601.000000 | -12460.000000 | -0.973% | 1.0098x |
| prover_MSM_terms | 558035360.000000 | 558010440.000000 | -24920.000000 | -0.004% | 1.0000x |
| prover_authentication_wall_seconds | 846.528000 | 4593.160000 | +3746.632000 | +442.588% | 0.1843x |
| verifier_kernel_calls | 45.000000 | 45.000000 | +0.000000 | +0.000% | 1.0000x |
| verifier_kernel_coordinates | 2461708448.000000 | 2461708448.000000 | +0.000000 | +0.000% | 1.0000x |
| verifier_kernel_cpu_seconds | 70.457100 | 69.053000 | -1.404100 | -1.993% | 1.0203x |
| verifier_map_cpu_seconds | 420.706000 | 414.484000 | -6.222000 | -1.479% | 1.0150x |
| verifier_kernel_peak_bytes | 536870912.000000 | 536870912.000000 | +0.000000 | +0.000% | 1.0000x |
| verifier_MSM_calls | 281356.000000 | 276697.000000 | -4659.000000 | -1.656% | 1.0168x |
| verifier_MSM_terms | 1299146.000000 | 1289828.000000 | -9318.000000 | -0.717% | 1.0072x |
| verifier_authentication_wall_seconds | 387.746000 | 436.150000 | +48.404000 | +12.483% | 0.8890x |

基线为本次此前实际完成的最新 OPT3-B Full12，Statement、120参数SHA、原Anchor和native必需关系完全相同，均为Release/四CPU。共享服务器负载已采样；每配置一次完整运行，不能视为独立重复受控统计实验。旧/新小于1表示变慢。通信及时间均为实际文件/进程/阶段测量，没有把理论节省当实测。新Prover为限制内存，每轮重建公开K并重读密封根；这些成本全部计入全局Sumcheck及端到端时间。

Oneblock P wall 797.677796s / V 215.768408s，峰RSS 4381360 / 1211780 KiB，Proof 142894014 bytes。

| 角色 | 阶段 | wall 秒 | CPU 秒 | P wire bytes | MSM calls | 阶段末 RSS KiB | 累计峰 RSS KiB |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| prover | frozen-witness-and-prechallenge-lookup | 298.299 | 314.385 | 0 | 0 | 34560716 | 34560716 |
| prover | all-classified-roots-prechallenge-seal | 740.102 | 739.808 | 24274886 | 542722 | 36473632 | 52071256 |
| prover | classified-physical-zero-tails | 0.0278009 | 0.0278349 | 19442 | 1 | 36473632 | 52071256 |
| prover | classified-public-X-Y-constants | 0.0290886 | 0.029121 | 2680200 | 1 | 36473632 | 52071256 |
| prover | all-native-GKR | 187.868 | 223.797 | 24520709 | 94083 | 36474072 | 52071256 |
| prover | native-Range-Exp-Lasso-Memory-Product | 1065.25 | 1064.79 | 182248997 | 629790 | 30080572 | 52071256 |
| prover | all-source-original-handles | 58.3801 | 58.3535 | 20225 | 16 | 30080572 | 52071256 |
| prover | protocol1-unified-input-sumcheck-and-opening | 4593.16 | 4590.98 | 25036952 | 1988 | 30080692 | 52071256 |
| verifier | frozen-witness-and-prechallenge-lookup | 5.4807 | 5.47844 | 0 | 0 | 5560440 | 5560440 |
| verifier | all-classified-roots-prechallenge-seal | 93.3217 | 93.2901 | 24274886 | 0 | 5588420 | 5588420 |
| verifier | classified-physical-zero-tails | 0.0217977 | 0.0217817 | 19442 | 1 | 5588420 | 5588420 |
| verifier | classified-public-X-Y-constants | 0.0331675 | 0.0332004 | 2680200 | 1 | 5602676 | 5602676 |
| verifier | all-native-GKR | 52.7453 | 69.7502 | 24520709 | 35403 | 5621684 | 5752660 |
| verifier | native-Range-Exp-Lasso-Memory-Product | 358.675 | 358.528 | 182248997 | 240477 | 5706816 | 6183968 |
| verifier | all-source-original-handles | 25.487 | 25.4788 | 20225 | 0 | 5707080 | 6183968 |
| verifier | protocol1-unified-input-sumcheck-and-opening | 436.15 | 435.943 | 25036952 | 815 | 5786660 | 6308632 |

### Sumcheck、Opening、Kernel、MSM 原始测量

公开K构造包含在全局Sumcheck中；Opening包含根MLE和隐藏拼接。两者与认证phase存在包含关系，不可重复相加。临时分配累计字节不是峰驻留内存；阶段末RSS和累计全程峰值有明确区分，未将累计全程峰值当作认证阶段峰值。具体计数scope保留在原日志。

```json
{
  "P": {
    "UNIFIED_SUMCHECK": {
      "domain": 4294967296,
      "rounds": 32,
      "wall_seconds": 4066,
      "cpu_seconds": 4064.01,
      "includes_public_kernel_endpoint": true
    },
    "UNIFIED_OPENING": {
      "roots": 45,
      "wall_seconds": 256.656,
      "cpu_seconds": 256.563,
      "includes_root_MLE_and_hidden_stitch": true
    },
    "UNIFIED_INPUT": {
      "claims": 1502,
      "roots_opened": 45,
      "partition_size": 1073741824,
      "domain": 4294967296,
      "sumcheck_rounds": 32,
      "full_input_copy_bytes": 0,
      "private_folded_input_copy_bytes": 0,
      "old_root_sumchecks": 0,
      "production_gate": "BLOCKED_CRYPTOGRAPHIC_DESIGN",
      "may_enter_stage2": false
    },
    "OPT3A_AUTH": {
      "builtin_RowMajor_base_coordinate_lookups": 1319730272,
      "kernel_matched_coordinates": 81236378784,
      "partial_visited_coordinates": 0,
      "frozen_kernel_coordinates": 81236378784,
      "fallback_map_queries": 0,
      "kernel_and_partial_Fr_multiplications": 81930065151,
      "kernel_and_partial_Fr_additions": 81236378784,
      "kernel_wall_seconds": 2255.12,
      "partial_wall_seconds": 0,
      "prepare_wall_seconds": 210.999,
      "kernel_and_partial_cpp_allocations": 1485,
      "kernel_and_partial_cpp_allocated_bytes": 764036579328
    },
    "OPT2_AUTH": {
      "map_calls": 3004,
      "map_coordinates": 5254320864,
      "map_cpu_seconds": 412.638,
      "kernel_calls": 1485,
      "kernel_coordinates": 81236378784,
      "kernel_cpu_seconds": 2254.03,
      "weights_precomputations": 1502,
      "weights_terms": 2488360,
      "weights_cpu_seconds": 0.132997,
      "weight_products": 81274405536,
      "partial_cpu_seconds": 0,
      "kernel_peak_bytes": 536870912
    },
    "OPT2_MSM": {
      "cpu_seconds": 588.875,
      "allocation_scope": "ordinary C++ new on tracing thread inside wrappers; excludes caller construction, C malloc, aligned and worker allocation",
      "bins": [
        {
          "length": "1",
          "calls": 90,
          "terms": 90,
          "cpu_seconds": 0.00361019,
          "cpp_allocations": 0
        },
        {
          "length": "2",
          "calls": 724443,
          "terms": 1448886,
          "cpu_seconds": 40.7859,
          "cpp_allocations": 0
        },
        {
          "length": "3-8",
          "calls": 180,
          "terms": 1080,
          "cpu_seconds": 0.0229882,
          "cpp_allocations": 0
        },
        {
          "length": "9-64",
          "calls": 270,
          "terms": 10080,
          "cpu_seconds": 0.182577,
          "cpp_allocations": 0
        },
        {
          "length": "65-1024",
          "calls": 543578,
          "terms": 556427424,
          "cpu_seconds": 546.859,
          "cpp_allocations": 0
        },
        {
          "length": ">1024",
          "calls": 40,
          "terms": 122880,
          "cpu_seconds": 1.02131,
          "cpp_allocations": 0
        }
      ]
    }
  },
  "V": {
    "UNIFIED_SUMCHECK": {
      "domain": 4294967296,
      "rounds": 32,
      "wall_seconds": 91.5145,
      "cpu_seconds": 91.4514,
      "includes_public_kernel_endpoint": true
    },
    "UNIFIED_OPENING": {
      "roots": 45,
      "wall_seconds": 70.6809,
      "cpu_seconds": 70.6464,
      "includes_root_MLE_and_hidden_stitch": true
    },
    "UNIFIED_INPUT": {
      "claims": 1502,
      "roots_opened": 45,
      "partition_size": 1073741824,
      "domain": 4294967296,
      "sumcheck_rounds": 32,
      "full_input_copy_bytes": 0,
      "private_folded_input_copy_bytes": 0,
      "old_root_sumchecks": 0,
      "production_gate": "BLOCKED_CRYPTOGRAPHIC_DESIGN",
      "may_enter_stage2": false
    },
    "OPT3A_AUTH": {
      "builtin_RowMajor_base_coordinate_lookups": 1319730272,
      "kernel_matched_coordinates": 2461708448,
      "partial_visited_coordinates": 0,
      "frozen_kernel_coordinates": 2461708448,
      "fallback_map_queries": 0,
      "kernel_and_partial_Fr_multiplications": 2482729247,
      "kernel_and_partial_Fr_additions": 2461708448,
      "kernel_wall_seconds": 69.0979,
      "partial_wall_seconds": 0,
      "prepare_wall_seconds": 212.603,
      "kernel_and_partial_cpp_allocations": 45,
      "kernel_and_partial_cpp_allocated_bytes": 23152623616
    },
    "OPT2_AUTH": {
      "map_calls": 3004,
      "map_coordinates": 5254320864,
      "map_cpu_seconds": 414.484,
      "kernel_calls": 45,
      "kernel_coordinates": 2461708448,
      "kernel_cpu_seconds": 69.053,
      "weights_precomputations": 1502,
      "weights_terms": 2488360,
      "weights_cpu_seconds": 0.143305,
      "weight_products": 2499735200,
      "partial_cpu_seconds": 0,
      "kernel_peak_bytes": 536870912
    },
    "OPT2_MSM": {
      "cpu_seconds": 22.0913,
      "allocation_scope": "ordinary C++ new on tracing thread inside wrappers; excludes caller construction, C malloc, aligned and worker allocation",
      "bins": [
        {
          "length": "1",
          "calls": 0,
          "terms": 0,
          "cpu_seconds": 0,
          "cpp_allocations": 0
        },
        {
          "length": "2",
          "calls": 276066,
          "terms": 552132,
          "cpu_seconds": 15.12,
          "cpp_allocations": 0
        },
        {
          "length": "3-8",
          "calls": 0,
          "terms": 0,
          "cpu_seconds": 0,
          "cpp_allocations": 0
        },
        {
          "length": "9-64",
          "calls": 0,
          "terms": 0,
          "cpu_seconds": 0,
          "cpp_allocations": 0
        },
        {
          "length": "65-1024",
          "calls": 601,
          "terms": 614816,
          "cpu_seconds": 5.98207,
          "cpp_allocations": 0
        },
        {
          "length": ">1024",
          "calls": 30,
          "terms": 122880,
          "cpu_seconds": 0.989196,
          "cpp_allocations": 0
        }
      ]
    }
  }
}
```

## Proof、Statement、Transcript及消息审计

新 Proof `261504622` bytes，SHA256 `4c5c63501e67a3c1f558f5ca0d43ff3d5b9f0e84eadb7a8f20cf8d577b53d510`。
FS suite `R7-FULL12-PROTOCOL1-UNIFIED-HIDDEN-FS-CANDIDATE-v1`；session `b18afdb8a56aa05df2d5989de9e5d182de18e194e2a09fe5aba183d016a0bd3b`；transcript `e6e02cd90a2c1211c3f2e44e375929889777d5f279d0060fcbc97284cc43ca7d`；challenge `65d0cc6d4b998a5fb54496e1628a7071f2831c4c402cf22eebda795127dcacb2`。P和V的身份、事件/消息/挑战计数完全一致，Verifier读取新磁盘文件并通过footer/EOF。

`R7_PROTOCOL1_UNIFIED_MESSAGE_AUDIT.json` 给出完整消息类别及去数字后的类别计数/实际payload/wire字节。旧完整行注册四类消息全0；旧lambda/aggregate/partial/root-sumcheck全部0。新全局Sumcheck、45物理根Opening、zero-tail完成标记与10个可信模型绑定均存在。Windows下载后的真实Proof重新解析，审计结果与远程完全一致。

## 正反例和命令证据

全量CTest92/92、独立密集数学参考每轮系数/终点测试、隐藏驱动和真实小PCS、跨根/重复/负或零stride/常量/affine bias/非幂长度/padding/错误root/map/可变回调/普通缓存耗尽均通过。缓存耗尽保留逐坐标digest重建检查，没有提高缓存上限或跳过验证。GKR、真实Range/Exp Lasso、Memory/Product、模型绑定独立测试通过。最初旧root认证代替全局Sumcheck的契约测试RED、旧transport新READY不兼容RED及修复后GREEN均保留。

| 配置 | 案例 | 退出码 | 原始拒绝原因 |
| --- | --- | ---: | --- |
| Oneblock | wrong-lasso-query | 2 | REJECT_OR_INCOMPLETE R7 public statement/parent binding mismatch |
| Oneblock | wrong-source-mapping | 2 | REJECT_OR_INCOMPLETE R7 public statement/parent binding mismatch |
| Oneblock | replaced-model-commitment | 2 | REJECT_OR_INCOMPLETE R7 public statement/parent binding mismatch |
| Oneblock | replaced-classified-root | 2 | REJECT_OR_INCOMPLETE R7 public statement/parent binding mismatch |
| Oneblock | wrong-gkr-terminal | 2 | REJECT_OR_INCOMPLETE R7 public statement/parent binding mismatch |
| Oneblock | different-gkr-lasso-witness-location | 2 | REJECT_OR_INCOMPLETE R7 public statement/parent binding mismatch |
| Oneblock | wrong-physical-zero-tail | 2 | REJECT_OR_INCOMPLETE classified zero-tail equation failed |
| Oneblock | wrong-public-constant | 2 | REJECT_OR_INCOMPLETE R7 public statement/parent binding mismatch |
| Oneblock | wrong-logical-padding | 2 | REJECT_OR_INCOMPLETE R7 Sigma compressed linear relation failed |
| Oneblock | wrong-sumcheck-round | 2 | REJECT_OR_INCOMPLETE R7 public statement/parent binding mismatch |
| Oneblock | wrong-hyrax-opening | 2 | REJECT_OR_INCOMPLETE A3 scalar knowledge equation failed |
| Oneblock | wrong-public-X | 2 | REJECT_OR_INCOMPLETE FS external statement/context/parameter identity mismatch |
| Oneblock | wrong-public-Y | 2 | REJECT_OR_INCOMPLETE FS external statement/context/parameter identity mismatch |
| Oneblock | wrong-fiat-shamir-transcript | 2 | REJECT_OR_INCOMPLETE FS completion/challenge/transcript digest mismatch |
| Oneblock | trailing-proof-bytes | 2 | REJECT_OR_INCOMPLETE FS appended buffered data rejected |
| Oneblock | truncated-proof | 2 | REJECT_OR_INCOMPLETE FS truncated proof or file IO failure |
| Full12 | replaced-model-commitment | 2 | REJECT_OR_INCOMPLETE R7 public statement/parent binding mismatch |
| Full12 | wrong-source-mapping | 2 | REJECT_OR_INCOMPLETE R7 public statement/parent binding mismatch |
| Full12 | wrong-sumcheck-round | 2 | REJECT_OR_INCOMPLETE R7 public statement/parent binding mismatch |
| Full12 | wrong-hyrax-opening | 2 | REJECT_OR_INCOMPLETE A3 scalar knowledge equation failed |

Full12恶意测试为四个明确选定案例，未声称重跑全部16个Oneblock案例。每次Verifier为独立新进程，从磁盘读mutantProof/Statement；无Prover进程或私有状态。每个负例保存实际Proof字节身份、精确差分、原始错误、资源记录和退出码。Windows分别重建16/4个恶意Proof并逐个核对SHA。

原始命令清单还见 `R7_PROTOCOL1_UNIFIED_COMMANDS.json` 和归档内每个测试/P/V/负例结果；可复核当前runner精确快照 `EXECUTED_RUN_ONEBLOCK_RETRY2.py`、`EXECUTED_RUN_ONEBLOCK_NEGATIVES_RETRY2.py`、`EXECUTED_RUN_FULL12_RETRY2.py`、`EXECUTED_FULL12_NEGATIVES_RETRY2.py`。先前Oneblock-v2/Full12-v1尝试和对应执行快照分别保留。

- `cmake --build build/protocol1-unified-release -j4` → 0 (GREEN_STATUS.json)
- `build/protocol1-unified-release/src/paper_r7/r7_fs_io_tests` → 0 (GREEN_STATUS.json)
- `ctest --test-dir build/protocol1-unified-release --output-on-failure -j1` → 0 (GREEN_STATUS.json)
- `cmake --build build/protocol1-unified-release -j4 --target r7_protocol1_oneblock_prover r7_protocol1_oneblock_verifier r7_protocol1_full12_prover r7_protocol1_full12_verifier r7_unified_input_tests r7_full12_runtime_tests r7_fs_io_tests r7_oneblock_gkr_tests r7_oneblock_lookup_tests r7_oneblock_model_tests` → 0 (CANDIDATE_CHECKS.json)
- `build/protocol1-unified-release/src/paper_r7/r7_unified_input_tests` → 0 (CANDIDATE_CHECKS.json)
- `build/protocol1-unified-release/src/paper_r7/r7_unified_input_tests exhaustion` → 0 (CANDIDATE_CHECKS.json)
- `build/protocol1-unified-release/src/paper_r7/r7_full12_runtime_tests` → 0 (CANDIDATE_CHECKS.json)
- `build/protocol1-unified-release/src/paper_r7/r7_fs_io_tests` → 0 (CANDIDATE_CHECKS.json)
- `build/protocol1-unified-release/src/paper_r7/r7_oneblock_gkr_tests` → 0 (CANDIDATE_CHECKS.json)
- `build/protocol1-unified-release/src/paper_r7/r7_oneblock_lookup_tests` → 0 (CANDIDATE_CHECKS.json)
- `build/protocol1-unified-release/src/paper_r7/r7_oneblock_model_tests` → 0 (CANDIDATE_CHECKS.json)
- `cmake --build build/protocol1-unified-release -j4` → 0 (PRODUCTION_ROUTE_VERIFIED.json)
- `build/protocol1-unified-release/src/paper_r7/r7_unified_input_tests interactive` → 0 (PRODUCTION_ROUTE_VERIFIED.json)
- `build/protocol1-unified-release/src/paper_r7/r7_fs_io_tests` → 0 (PRODUCTION_ROUTE_VERIFIED.json)
- `ctest --test-dir build/protocol1-unified-release --output-on-failure -j1` → 0 (PRODUCTION_ROUTE_VERIFIED.json)
- `cmake --build build/protocol1-unified-release -j4` → 0 (LARGE_DOMAIN_GREEN.json)
- `build/protocol1-unified-release/src/paper_r7/r7_unified_input_tests large-domain` → 0 (LARGE_DOMAIN_GREEN.json)
- `ctest --test-dir build/protocol1-unified-release --output-on-failure -j1` → 0 (LARGE_DOMAIN_GREEN.json)
- `build/protocol1-unified-release/src/paper_r7/r7_fs_oneblock_prover prove reports/R7-PROTOCOL1-UNIFIED-20261030-v1/oneblock-v3/protocol1-new.fsproof zkGPT/R7/Protocol1/Unified/oneblock/20261030/retry2` → 0 (Oneblock)
- `build/protocol1-unified-release/src/paper_r7/r7_fs_oneblock_verifier verify reports/R7-PROTOCOL1-UNIFIED-20261030-v1/oneblock-v3/protocol1-new.fsproof zkGPT/R7/Protocol1/Unified/oneblock/20261030/retry2` → 0 (Oneblock)
- `build/protocol1-unified-release/src/paper_r7/r7_fs_full12_prover prove reports/R7-PROTOCOL1-UNIFIED-20261030-v1/full12-v2/protocol1-new.fsproof zkGPT/R7/Protocol1/Unified/full12/20261030/retry2` → 0 (Full12)
- `build/protocol1-unified-release/src/paper_r7/r7_fs_full12_verifier verify reports/R7-PROTOCOL1-UNIFIED-20261030-v1/full12-v2/protocol1-new.fsproof zkGPT/R7/Protocol1/Unified/full12/20261030/retry2` → 0 (Full12)
- `build/protocol1-unified-release/src/paper_r7/r7_fs_oneblock_verifier verify reports/R7-PROTOCOL1-UNIFIED-20261030-v1/oneblock-v3/negatives-v1/wrong-lasso-query.fsproof zkGPT/R7/Protocol1/Unified/oneblock/20261030/retry2` → 2 (Oneblock negative wrong-lasso-query)
- `build/protocol1-unified-release/src/paper_r7/r7_fs_oneblock_verifier verify reports/R7-PROTOCOL1-UNIFIED-20261030-v1/oneblock-v3/negatives-v1/wrong-source-mapping.fsproof zkGPT/R7/Protocol1/Unified/oneblock/20261030/retry2` → 2 (Oneblock negative wrong-source-mapping)
- `build/protocol1-unified-release/src/paper_r7/r7_fs_oneblock_verifier verify reports/R7-PROTOCOL1-UNIFIED-20261030-v1/oneblock-v3/negatives-v1/replaced-model-commitment.fsproof zkGPT/R7/Protocol1/Unified/oneblock/20261030/retry2` → 2 (Oneblock negative replaced-model-commitment)
- `build/protocol1-unified-release/src/paper_r7/r7_fs_oneblock_verifier verify reports/R7-PROTOCOL1-UNIFIED-20261030-v1/oneblock-v3/negatives-v1/replaced-classified-root.fsproof zkGPT/R7/Protocol1/Unified/oneblock/20261030/retry2` → 2 (Oneblock negative replaced-classified-root)
- `build/protocol1-unified-release/src/paper_r7/r7_fs_oneblock_verifier verify reports/R7-PROTOCOL1-UNIFIED-20261030-v1/oneblock-v3/negatives-v1/wrong-gkr-terminal.fsproof zkGPT/R7/Protocol1/Unified/oneblock/20261030/retry2` → 2 (Oneblock negative wrong-gkr-terminal)
- `build/protocol1-unified-release/src/paper_r7/r7_fs_oneblock_verifier verify reports/R7-PROTOCOL1-UNIFIED-20261030-v1/oneblock-v3/negatives-v1/different-gkr-lasso-witness-location.fsproof zkGPT/R7/Protocol1/Unified/oneblock/20261030/retry2` → 2 (Oneblock negative different-gkr-lasso-witness-location)
- `build/protocol1-unified-release/src/paper_r7/r7_fs_oneblock_verifier verify reports/R7-PROTOCOL1-UNIFIED-20261030-v1/oneblock-v3/negatives-v1/wrong-physical-zero-tail.fsproof zkGPT/R7/Protocol1/Unified/oneblock/20261030/retry2` → 2 (Oneblock negative wrong-physical-zero-tail)
- `build/protocol1-unified-release/src/paper_r7/r7_fs_oneblock_verifier verify reports/R7-PROTOCOL1-UNIFIED-20261030-v1/oneblock-v3/negatives-v1/wrong-public-constant.fsproof zkGPT/R7/Protocol1/Unified/oneblock/20261030/retry2` → 2 (Oneblock negative wrong-public-constant)
- `build/protocol1-unified-release/src/paper_r7/r7_fs_oneblock_verifier verify reports/R7-PROTOCOL1-UNIFIED-20261030-v1/oneblock-v3/negatives-v1/wrong-logical-padding.fsproof zkGPT/R7/Protocol1/Unified/oneblock/20261030/retry2` → 2 (Oneblock negative wrong-logical-padding)
- `build/protocol1-unified-release/src/paper_r7/r7_fs_oneblock_verifier verify reports/R7-PROTOCOL1-UNIFIED-20261030-v1/oneblock-v3/negatives-v1/wrong-sumcheck-round.fsproof zkGPT/R7/Protocol1/Unified/oneblock/20261030/retry2` → 2 (Oneblock negative wrong-sumcheck-round)
- `build/protocol1-unified-release/src/paper_r7/r7_fs_oneblock_verifier verify reports/R7-PROTOCOL1-UNIFIED-20261030-v1/oneblock-v3/negatives-v1/wrong-hyrax-opening.fsproof zkGPT/R7/Protocol1/Unified/oneblock/20261030/retry2` → 2 (Oneblock negative wrong-hyrax-opening)
- `build/protocol1-unified-release/src/paper_r7/r7_fs_oneblock_verifier verify reports/R7-PROTOCOL1-UNIFIED-20261030-v1/oneblock-v3/negatives-v1/wrong-public-X.fsproof zkGPT/R7/Protocol1/Unified/oneblock/20261030/retry2` → 2 (Oneblock negative wrong-public-X)
- `build/protocol1-unified-release/src/paper_r7/r7_fs_oneblock_verifier verify reports/R7-PROTOCOL1-UNIFIED-20261030-v1/oneblock-v3/negatives-v1/wrong-public-Y.fsproof zkGPT/R7/Protocol1/Unified/oneblock/20261030/retry2` → 2 (Oneblock negative wrong-public-Y)
- `build/protocol1-unified-release/src/paper_r7/r7_fs_oneblock_verifier verify reports/R7-PROTOCOL1-UNIFIED-20261030-v1/oneblock-v3/negatives-v1/wrong-fiat-shamir-transcript.fsproof zkGPT/R7/Protocol1/Unified/oneblock/20261030/retry2` → 2 (Oneblock negative wrong-fiat-shamir-transcript)
- `build/protocol1-unified-release/src/paper_r7/r7_fs_oneblock_verifier verify reports/R7-PROTOCOL1-UNIFIED-20261030-v1/oneblock-v3/negatives-v1/trailing-proof-bytes.fsproof zkGPT/R7/Protocol1/Unified/oneblock/20261030/retry2` → 2 (Oneblock negative trailing-proof-bytes)
- `build/protocol1-unified-release/src/paper_r7/r7_fs_oneblock_verifier verify reports/R7-PROTOCOL1-UNIFIED-20261030-v1/oneblock-v3/negatives-v1/truncated-proof.fsproof zkGPT/R7/Protocol1/Unified/oneblock/20261030/retry2` → 2 (Oneblock negative truncated-proof)
- `build/protocol1-unified-release/src/paper_r7/r7_fs_full12_verifier verify reports/R7-PROTOCOL1-UNIFIED-20261030-v1/full12-v2/negatives-v1/replaced-model-commitment.fsproof zkGPT/R7/Protocol1/Unified/full12/20261030/retry2` → 2 (Full12 negative replaced-model-commitment)
- `build/protocol1-unified-release/src/paper_r7/r7_fs_full12_verifier verify reports/R7-PROTOCOL1-UNIFIED-20261030-v1/full12-v2/negatives-v1/wrong-source-mapping.fsproof zkGPT/R7/Protocol1/Unified/full12/20261030/retry2` → 2 (Full12 negative wrong-source-mapping)
- `build/protocol1-unified-release/src/paper_r7/r7_fs_full12_verifier verify reports/R7-PROTOCOL1-UNIFIED-20261030-v1/full12-v2/negatives-v1/wrong-sumcheck-round.fsproof zkGPT/R7/Protocol1/Unified/full12/20261030/retry2` → 2 (Full12 negative wrong-sumcheck-round)
- `build/protocol1-unified-release/src/paper_r7/r7_fs_full12_verifier verify reports/R7-PROTOCOL1-UNIFIED-20261030-v1/full12-v2/negatives-v1/wrong-hyrax-opening.fsproof zkGPT/R7/Protocol1/Unified/full12/20261030/retry2` → 2 (Full12 negative wrong-hyrax-opening)

### 保留的失败和修正

新构建初始未打开既有R7 CMake开关而缺target，修正配置后全量构建；原日志保留。隐藏负例测试最初在Prover侧制造非法初值被既有自检拒绝，改为诚实Proof后独立Verifier变造测试，生产逻辑未绕过。首次Oneblock-v1 runner误用probe做setup，在证明开始前失败，无Proof；改用完整setup，在全新oneblock-v2重试。负例后处理最初错误期望退出1，既有CLI实际明确拒绝返回2；只修正审计helper并保留失败helper/修正记录，不改变Verifier。

首次真实Full12-v1在2764.848109秒退出2，原始错误 `REJECT_OR_INCOMPLETE vector dimension bound`；1502映射准备完成，但Kernel尚未调用，Verifier没有启动。失败不是共享服务器压力停止。实际部分Proof布局独立解析确认每分区2^30、全局2^32，原新驱动误用带实体向量2^28限制的 `r1::log2exact`。修正仅替换两个全局虚拟域的轮数计算，使用既有2^32虚拟上限的metadata计数，所有实体根维度/内存上限不变。三标量稀疏回归先复现RED，再通过2^29至2^32每轮独立参考、隐藏P/V和EOF；2^33仍拒绝；完整构建与CTest92/92重过。测试参考最初未显式初始化mcl Fr pair，单独改为Fr0；保留首次失败日志及精确源字节。之后重新运行当前生产Oneblock-v3及16个恶意验证，才重试全新Full12-v2。本报告性能表仅使用最终成功Full12-v2，不混入失败尝试或历史成功代替本轮。

失败部分Proof237636833 bytes，SHA `eca5ba0c067d3aa0344c86779b5b0da49d65943b2bce8061570fd64f1dbd60f2`，明确以 `FULL12_V1_FAILED_INCOMPLETE_PROOF.bin` 保存，不作为完整Proof发布；失败源码397/ELF/原始日志和未完成关系单独归档，Windows418成员/gzip/SHA/恢复均校验。失败P账本文件为空，因此 `FULL12_V1_FAILURE.json` 明确将全部动态关系保守记为无独立验证，关系清单按未改变的native/layout加新声明重建，没有伪称观察到部分VERIFIED。这些失败均未被计为成功，也未覆盖原日志。

## 源码变更与Windows归档

- `src/paper_r7/CMakeLists.txt`: +25 / -0
- `src/paper_r7/classified_commitments.cpp`: +31 / -0
- `src/paper_r7/classified_commitments.hpp`: +5 / -0
- `src/paper_r7/classified_controller.inc`: +4 / -0
- `src/paper_r7/fs_io.cpp`: +7 / -5
- `src/paper_r7/fs_io.hpp`: +2 / -0
- `src/paper_r7/fs_tests.cpp`: +9 / -1
- `src/paper_r7/full12_runtime_tests.cpp`: +2 / -2
- `src/paper_r7/interactive.cpp`: +1 / -1
- `src/paper_r7/oneblock_main.cpp`: +37 / -4
- `src/paper_r7/unified_input.cpp`: +73 / -0
- `src/paper_r7/unified_input.hpp`: +23 / -0
- `src/paper_r7/unified_input_tests.cpp`: +131 / -0

增量补丁 `R7_PROTOCOL1_UNIFIED_DIFF_v2.patch` SHA `fc8193f34aeb97b29d2a43db073e3eb347107598951976f04295b9ff31abfd73`，基于冻结工作树而非HEAD；反向 `git apply --check` 退出0。边界修正前的v1补丁和源码仍保留。源码397文件最终SHA、所有参数SHA、配置/编译器/运行依赖和二进制均入档。

Windows证据根：`C:/Users/tanyt/Desktop/program/zkGPT-backups/r7-protocol1-unified-20261030-v1`。Linux原始证据：`/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R7-PROTOCOL1-UNIFIED-20261030-v1`。

Windows档案：保留初版单位/Oneblock正例、16负例归档；当前版本新增 `protocol1-oneblock-retry2-source-and-evidence.tar.gz`、`protocol1-full12-source-and-evidence-v2.tar.gz`，并保留失败归档 `protocol1-full12-failed-v1-evidence.tar.gz`。当前新公开Proof分开保存在 `oneblock-protocol1-retry2.fsproof` / `full12-protocol1.fsproof`，初版Oneblock Proof另外保留。对应Windows验收收据均通过；验证容器SHA、全部成员SHA、gzip完整性、解包恢复读取、397源码、120参数CAS和当前16/4个恶意Proof重建。原完整证明、Statement、原Anchor及Pin、历史失败/成功数据均保留；未归档SSH凭据、私有Witness或blinds，未把任何远程副本视为可删除。

## 未解决的安全边界

本报告证明的是实际代码的全局输入等式、独立验证和工程覆盖。尚无对改造后的Sigma隐藏Sumcheck/Fiat–Shamir/分类Hyrax联合组合的正式知识可靠性、零知识模拟或可提取性证明。No-FullBase状态下也未凭工程测试重新宣称完整知识注册的安全性质。原论文的关系对应、单元参考和恶意拒绝测试不能代替这些证明；模型/数值/Hyrax A.3未修改，但这不使新组合自动获得正式安全性。当前仍严格保持 `FS_NIZK_CANDIDATE / BLOCKED_CRYPTOGRAPHIC_DESIGN / may_enter_stage2=false`。
