# 任务七 D：zkGPT v2 Hyrax Input Opening 零拷贝优化

状态：只读审计、最小零拷贝实现、Release/Debug/Mutation/ASan/UBSan 前置回归、真实 12-block GKR-only、完整 Range + GKR + Artifact、同进程及新进程独立生产验证全部完成。

## 1. 修改前只读审计

`GKRProofBuilder::buildInputOpeningProof()` 从 `comm.w` 构造 `vector<Fr> witness(comm.w,comm.w+value_count)`，仅因为 `hyraxMleOpenProveFrRowMajor()` 与内部 reduction 的参数类型为 `const vector<Fr>&`。旧实现不依赖该输入 vector 的独占所有权；完整 copy 不参与 Transcript，也不在历史 Hyrax arithmetic timer 内。

### 1.1 读写路径和生命周期

```text
prover（唯一 owner）
 └ committed_witness_ unique_ptr<Fr[]>：padded cc.w
    └ cc / GKR builder 的 comm：raw borrowed view
       └ buildInputOpeningProof
          ├ copy cc.w[0,N) → owning opening_witness vector[0,N)
          ├ Hyrax row-major reduction（values 为 const vector&）
          │  ├ column_weights = eq(point[0,column_bits))
          │  ├ row_weights = eq(point[column_bits,n))
          │  ├ collapsed[column_count] 自有 vector，初始化 0
          │  ├ joined workers：只读 values[row*columns+column]
          │  │                 只写自己 column 区间的 collapsed[column]
          │  └ row commitment 线性组合 → MleReduction 值
          ├ innerProduct(collapsed,column_weights) 检查 evaluation
          ├ IPA：复制 reduction 的小向量，fold 自有临时 vectors
          │      L/R commitment → challenge → folding → final_witness
          ├ 返回完全 value-owned MleOpeningProof；销毁 opening_witness
          └ 复制 opening proof 到 GKRProof，无输入指针保留
GKR build 返回 → C 阶段原 lifecycle scope 最后销毁 prover owner
```

输入只被 `values.size()` 和 `values[index]` 读取；row-major reduction 没有 capacity、resize、push_back、迭代器持有或任何输入写入。IPA 的 resize/push_back/分割操作针对 `reduction.witness` 的本地副本及本地 coefficients/generators。不是把全 N witness 传给 IPA；传入的是 column_count 个 collapsed 元素。

`parallelRange` lambda 在 synchronous 调用中借用输入，线程对象储存在局部 vector，ThreadJoiner 在返回或部分线程构造异常时 join 已创建线程。线程不被 detach，proof 中没有 borrowed address。row/column weights、collapsed、point/generators 在 join 期间存活。每个 worker 的 `[begin,end)` column 区间互不重叠，所有 worker 只读输入，不能形成 witness 写冲突。

G1::mulVec 已有的 const_cast 仅用于兼容 G1 generator/commitment API，本轮没有新增 const_cast，也没有对 witness const_cast。Fr 输入路径保持 const。

### 1.2 几何与索引证明

设 n=point.size，row_bits=floor(n/2)，column_bits=n-row_bits，rows=2^row_bits，columns=2^column_bits，N=rows*columns=2^n。

- 全规模 n=28，rows=columns=16,384，N=268,435,456，sizeof(Fr)=32，完整 copy **8,589,934,592 bytes = 8 GiB**。
- 各 worker 读取 `values[row*columns+column]`；row ∈ [0,rows)，column ∈ [begin,end) ⊆ [0,columns)，最小索引 0，最大 `N-1=268,435,455`。只写 collapsed[column]，范围 [0,columns)。
- 每个 collapsed[column] 仍按 row=0…rows−1 的原次序累计；并行只切 column，改变接口不改变任何一列的运算次序。
- n=0 最小合法 witness 为 1，rows=columns=1，point 为空，IPA 0 轮，直接 final witness。
- 奇数 n 的 columns 比 rows 大 2 倍，低 column_bits 挑战对应列；本轮不交换 point 分段，不改变 input mapping。
- Builder 当前要求 comm.l∈[0,30]，r_u[0].size()==comm.l；public val0_log_size 在构造时取 comm.l。view 精确设为 N，底层 owner 来自 commitInput 同一 2^comm.l array，padding 保持不变。

### 1.3 全部相关调用者与 verifier

生产 row-major prover 调用者只有 GKR input opening；原 `main_range_tests::testRowMajorMleOpening` 使用 vector。其它 `hyraxMleOpenProveFr` 调用者位于 Range LogUp 和 reconstruction，是不同的 column-major reduction，保留其原 vector 接口和实现。uint16 MLE、通用 IPA 的接口不变。

`hyraxMleOpenVerifyRowMajor` 只使用公开 commitments、point、generators 和 opening proof，重新组合 row commitment、column weights，再调用原 IPA verifier；它从不需要私有 witness，本轮没有修改该 verifier 或其接受条件。

## 2. A/B/C 比较与选择

本项目实际 `CMAKE_CXX_STANDARD=17`，直接使用标准 std::span 会要求 C++20 或引入额外依赖。选择 C++17 的小型 bounded read-only view（pointer + length 的 span 等价语义），**按方案 B 统一内部 row-major 只读核心，vector 接口委托**。没有更改项目语言标准。

| 方案 | 范围 / 只读保证 | 边界与生命周期 | 其它调用者 / Proof | RSS 与复制收益 / 风险 |
|---|---|---|---|---|
| A 新增 span 重载，保留原独立 vector 实现 | 新入口但两份 arithmetic 路径；std::span 原生需 C++20 | 需两份 geometry 检查；同步 borrowed lifetime 可安全 | vector 不受影响，但两份计算代码可能漂移 | 可消除 full copy；重复生产 arithmetic 风险较高 |
| B 统一只读核心，vector 委托 | **采用**；HyraxFrWitnessView 暴露 const Fr&，仅 row-major 两个函数适配 | geometry 在读取前统一检查；view 只在同步函数内借用，worker join | 原 vector 源码调用仍兼容；其它 Hyrax/IPA 路径不动；逐字节参考对比 | 同样消除 8 GiB full allocation/copy，修改最小且只有一个 arithmetic 核心 |
| C 当前 GKR 局部 pointer wrapper | 可只改局部签名；若仍调用旧 vector 会再次复制，若单独实现又重复 arithmetic | checked length 可安全，但容易形成特例与检查漂移 | 其它调用者不动，需另维护特殊路径 | 收益相同，长期接口/测试一致性更差 |

A/B/C 消除的是 allocation 和 memcpy，不保证 complete Peak RSS 降低。任务七 C Lasso 峰值 47,190,148 KiB 已高于旧 opening 的直接采样 RSS，完整峰值可能继续受 Lasso 限制；最终必须以实测回答。

## 3. 修改后数据流与实际实现

```text
prover unique_ptr<Fr[]> cc.w（唯一 owner，未修改）
 └ GKR builder comm（原 borrowed view，未修改 owner）
    └ buildInputOpeningProof
       ├ HyraxFrWitnessView{comm.w,N}：const pointer + exact length，不分配/copy
       ├ 原 row-major reduction：只读 view[0,N)，自有 collapsed[columns]
       │  └ joined workers，各列保持原 row accumulation 次序
       ├ 原 evaluation 检查 / 原 IPA 自有 folding
       └ owning MleOpeningProof → GKRProof 值
opening 返回后不保留 view；所有 worker 已 join
GKR 完成后按任务七 C 的原作用域释放私有 owner
原完整同进程 verifyZkGPT → 排他写 Artifact → 正常退出
新启动生产 file verifier → 第二次完整验证
```

### 3.1 修改文件和函数

| 文件 | 修改 |
|---|---|
| `src/hyrax_opening.hpp` | 增加 C++17 HyraxFrWitnessView 与 row-major overload；保留原 const vector& 接口 |
| `src/hyrax_opening.cpp::reduceMleProverFrRowMajor` | 参数改只读 view；在任何数据读取和 eqWeights shift 前检查 log、长度、byte/multiplication overflow、row commitments 和 generator geometry；循环、索引、加法次序原样保留 |
| `src/hyrax_opening.cpp::hyraxMleOpenProveFrRowMajor` | 唯一 view 核心，原 vector 入口仅构造 data/size view 委托；IPA 调用与 arithmetic 不变 |
| `src/gkr_proof_builder.cpp::buildInputOpeningProof` | 删除整个 owning witness vector；直接借用 comm.w 的精确 N view；增加 null storage 拒绝与针对性 diagnostics |
| `src/main_hyrax_opening_tests.cpp` | 70 cases，vector/view/旧参考 canonical bytes 与完整 Challenge Trace 对比，输入 guard/未修改、owner 销毁后验证、invalid geometry |
| `src/tests/hyrax_row_major_reference.hpp` | 冻结任务七 C 的 vector row-major reduction/prove，**仅测试 target 包含，未进入 gpt_lib 或生产证明** |
| `src/CMakeLists.txt` | 增加 hyrax_opening_tests target |

没有修改 prover/comm ownership、Range、Lasso、Circuit Builder、IPA folding、Transcript、Verifier 或任务七 C 生命周期。新增 diagnostics 只在 ZKGPT_MEMORY_DIAGNOSTICS 开启时记录，不进入证明或挑战。

### 3.2 边界、所有权与线程安全

view constructor 拒绝 null+nonzero length；view 不拥有、不释放、不延长 owner。operator[] 返回 const Fr&，与 std::span 一样不逐元素重复 bounds check；生产 reduction 在读前验证整体 geometry，并由上文索引证明确保不越界。必须由调用方提供真实存活的 storage，长度不能凭空证明 allocation 有效；生产 builder 的 unique owner/comm geometry 已审计。

core 检查 n<=30、n<size_t bit width、N*sizeof(Fr) 不溢出、rows*columns 不溢出且等于 N、view.size==N、commitments.size==rows、generators.size>=columns。非法 geometry 在读取 witness、修改 Transcript、分配 collapse 工作表之前拒绝。32-bit 平台的 byte-size 不可表达时也会拒绝；实际验收平台是 64-bit。

所有 view/weights/collapsed 在 worker join 前存活。线程创建失败时原 ThreadJoiner 同步回收；输入本身不会被释放或 resize。返回的 reduction 拥有 collapsed 和 coefficients，返回的 proof 仅包含 Fr/G1/vector 值。owner 销毁后 proof 可被生产 verifier 验证，不再访问原 view。


## 4. 前置回归与专项正确性结果

| 测试 | Release | Debug | ASan | UBSan |
|---|---|---|---|---|
| range_protocol_tests | 通过，exit 0 | 通过，exit 0 | 通过，exit 0 | 通过，exit 0 |
| gkr_mutation_tests | 通过，exit 0 | 通过，exit 0 | 通过，exit 0 | 通过，exit 0 |
| zkgpt_artifact_mutation_tests | 通过，exit 0 | 通过，exit 0 | 未在该模式执行 | 未在该模式执行 |
| zkgpt_decoder_resource_tests | 通过，exit 0 | 通过，exit 0 | 未在该模式执行 | 未在该模式执行 |
| lasso_folding_tests | 通过，exit 0 | 通过，exit 0 | 通过，exit 0 | 通过，exit 0 |
| prover_lifecycle_tests | 通过，exit 0 | 通过，exit 0 | 通过，exit 0 | 通过，exit 0 |
| hyrax_opening_tests | 通过，exit 0 | 通过，exit 0 | 通过，exit 0 | 通过，exit 0 |

Release/Debug Artifact Mutation 各 36/36 semantic、11/11 encoding；GKR Mutation 各 19/19；Lasso Folding 632 cases 保持通过。新增 Hyrax 70 组 cases 在四种 build 下全部通过。

Hyrax 专项用冻结的任务七 C vector reduction/prove 作独立 test-only reference；n=0…13，每个 n 覆盖零值、确定性随机、稀疏、logical 非二次幂经零 padding、首末行列/四角。n>=12 的 columns>=64 会真实触发 parallelRange，多线程路径不只是设置环境变量。

每例检查旧 reference/new view/vector wrapper 的 canonical opening bytes（经原 GKR serializer 编码的 opening 字段）完全一致、全部 ChallengeTrace entries 相同、Transcript digest 相同；独立 owner 的首尾 guard 和每个 witness 元素不变；owner 析构后生产 row-major verifier 仍接受 Proof。比较 1/8 线程下全部 70 条 OPENING_VECTOR digest 行，完全一致；各自都与旧参考 canonical bytes/挑战匹配。

非法输入测试覆盖 null+nonzero view constructor、空 view、N−1/N+1/SIZE_MAX 长度、缺少 commitments/generators、n=31 和 size_t bits 个挑战；均在读取前抛 invalid_argument，Transcript 未改变。编译期 static_assert 确认 view 元素返回 const Fr&。这些测试验证受控存活 allocation 的生命周期，不故意解引用已释放指针；borrowed view 接口要求 caller 持续保证 backing storage 存活。

ASan 使用 detect_leaks=0:halt_on_error=1；UBSan 使用 halt_on_error=1；均无 sanitizer 错误。输入 guard/值一致与只读并发代码检查支持无 witness 修改/列写冲突，但没有执行 ThreadSanitizer，不能把 ASan/UBSan 当成一般竞态检测。

固定 GKR Challenge Trace：Release ZKGPT_THREADS=1 和 8 的全部 TRACE_VECTOR 行与任务七 C 完全一致，75 entries，final digest `3b08a8baaf3c80c4089a97af7ddeb83c2e48e633007e7dea545db428856aefd2`。

小型完整 Artifact：134,609-byte Statement、1,968,644-byte Proof；原 lifecycle fixture 在 private scope 析构后完成同进程 production verification，再启动生产文件 verifier，exit 0、wall 8.75 s、Peak RSS 2,040,652 KiB。

LeakSanitizer 另以 `detect_leaks=1:halt_on_error=1` 实际执行 Hyrax 专项：70 cases 的测试体通过，但 LSan fatal，报告 `LeakSanitizer does not work under ptrace`，该运行 exit 1（`lsan_hyrax.log`）。**LSan 未验证，不计为通过，不宣称无泄漏。**

## 5. 真实 12-block GKR-only

执行 `/usr/bin/time -v timeout 3600s env ZKGPT_THREADS=8 ZKGPT_MEMORY_DIAGNOSTICS=1 ./cmake-build-release/src/demo_llm_run --stage-a-test=gkr-only --gkr-challenge-trace`，exit 0。

| 指标 | 任务七 C | 本轮 D | 变化 |
|---|---:|---:|---:|
| GKR Prover | 100.219 s | 100.825 s | +0.606 s，+0.60% |
| GKR-only wall | 369.38 s | 367.03 s | −2.35 s，−0.64% |
| GKR-only Peak RSS | 45,889,604 KiB | 45,890,004 KiB | +400 KiB，基本不变 |
| GKR Proof bytes | 827,780 | 827,780 | 一致 |
| full Challenge Trace entries | 9,334 | 9,334 | 一致 |

最终 digest `92fbe9e46a08734483df6147abd57734a8eebe5b7ae7b308a3a88d8520157cd3` 与 C 一致，生产 GKR verifier 和原负例检查通过。

GKR-only opening 旧采样最高 RSS 45,050,680 KiB，新采样最高 36,662,336 KiB，下降 8,388,344 KiB ≈ 8.00 GiB；旧 opening 前 36,655,216 KiB，新 opening 前 36,655,480 KiB，基准阶段仅差 264 KiB。新 view 创建前后 RSS 完全相同；IPA 后约增加 6,856 KiB 的小型工作表/allocator 驻留，未创建 N-element witness vector。旧峰值采样与新峰值采样是阶段观测点，不能称为独立重置的 ru_maxrss；累计 HWM 在进入 opening 前已受 Lasso 峰值限制。

线程与基线一致：ZKGPT_THREADS=8；row-major columns 并行采用 8 workers，完整 Range 使用 --range-threads 8；既有 Lasso/legacy 32-worker 循环未修改。

## 6. 内存测量口径和可确认的分配消除

被删除的是旧 `vector<Fr> witness(comm.w,comm.w+N)` 整个构造与析构路径。N=2^28，Fr=32 bytes：减少确定的 owning allocation **8,589,934,592 bytes**，减少完整源数据复制 **8,589,934,592 bytes**（若计算数据传输读+写合计则为 16 GiB，但本报告的“复制字节”按 payload 8 GiB 计）。源 cc.w 的 8 GiB owner allocation 未改变，padding 与任意 witness 元素未改变。

新 view 在 64-bit 平台仅为 const pointer + size_t，不拥有 capacity，不分配大数组；diagnostics 的 full_witness_copy_allocation_bytes/full_witness_copy_bytes 都为 0。它们由源码和实际小型参考/全规模测量共同支持，不能仅靠 scalar 日志宣称收益。collapsed 自有 vector 的 count/capacity=16,384，524,288 bytes；row/column weights、opening_generators 和 IPA 自有 scratch 维持原实现。

VmSize 是虚拟地址空间，vector/array capacity 是 logical allocation，已访问页面/RSS 是物理驻留口径；本轮读取已有 cc.w 不新建 full copy，其读取本身仍会触碰 witness 页面。没有逐页采样或声称特定页面全部退出物理内存，采用实际 VmRSS 和 time -v Peak 验证。

opening 局部值取新增阶段采样的最高 VmRSS，并与 C 的 copy-ready/before-release 采样比较。进程 VmHWM/ru_maxrss 不能在 opening 开始时重置，opening 期间的累计 HWM 可能已由先前 Lasso 决定；因此严格称为“opening 采样最高 RSS”，不伪称连续监测的局部独立峰值。完整进程 Peak 以生成程序正常退出的 /usr/bin/time -v 记录为准。

GKR/Hyrax 内部 timer 仍从实际 opening arithmetic 前开始，不把旧 copy 的成本突然纳入或移出新 GKR 计时口径；copy 移除的整体收益主要应看完整 wall，单次墙钟差还受机器波动影响，不能推算独立精确 memcpy 时间。

## 7. 完整 12-block Range + GKR + Artifact 与独立验证

资源检查：MemAvailable 507,378,252 KiB，/tmp 空闲 75,983,761,408 bytes。所有前置 tests/trace 通过后，GKR-only、完整生成、独立 verifier 串行执行，无大型验收并发。生产仍使用 ZKGPT_THREADS=8 和 --range-threads 8。

新排他前缀 `/tmp/zkgpt-v2-task7-hyrax-opt` 未占用，未追加 suffix。原 main 的 O_EXCL writer 保持，历史 Artifact 未覆盖。

```bash
/usr/bin/time -v timeout 7200s env ZKGPT_THREADS=8 ZKGPT_MEMORY_DIAGNOSTICS=1 \
  ./cmake-build-release/src/demo_llm_run --range-threads 8 \
  --artifact-prefix /tmp/zkgpt-v2-task7-hyrax-opt

# 仅在生成进程正常 exit 0 后，新启动独立 production verifier
/usr/bin/time -v timeout 1800s env ZKGPT_THREADS=8 ZKGPT_MEMORY_DIAGNOSTICS=1 \
  ./cmake-build-release/src/zkgpt_artifact_verify \
  /tmp/zkgpt-v2-task7-hyrax-opt.statement.bin \
  /tmp/zkgpt-v2-task7-hyrax-opt.proof.bin
```

完整生成正常 exit 0；原同进程 verifyZkGPT 全部验证通过；新进程 file verifier 正常 exit 0。两个入口均执行原 production binding/Range/public C/GKR/FCONN U-V/Lasso/Hyrax-IPA 路径，接受条件没有修改。

| 指标 | 任务七 C | 本轮 D | D − C |
|---|---:|---:|---:|
| 完整生成进程 Peak RSS（包含同进程完整 verify） | 47,190,148 KiB | **47,198,300 KiB** | **+8,152 KiB，+0.0173%，基本不变** |
| 完整 wall | 1,060.86 s | 1,061.66 s | +0.80 s，+0.075% |
| Range Prover | 628.478 s | 629.165 s | +0.687 s，+0.109% |
| GKR Prover | 101.639 s | 101.511 s | −0.128 s，−0.126% |
| Hyrax opening arithmetic timer | 3.014270 s | 2.783288 s | −0.230982 s，−7.66% |
| 同进程完整 verify timer | 60.2424 s | 61.0299 s | +0.7875 s，+1.31% |
| 独立 verifier wall | 85.78 s | 83.71 s | -2.07 s |
| 独立 verifier Peak RSS | 23,426,108 KiB | 23,397,220 KiB | -28,888 KiB |
| Statement bytes | 562,578 | 562,578 | 一致 |
| Proof bytes | 16,683,344 | 16,683,344 | 一致 |

完整 wall 的 1,061.66 s 包含模型、电路、witness/commitment、Range/GKR、serialization/round-trip、同进程完整 production verification、排他写文件和析构；driver wall=1,061.667425 s，口径另列。不是仅截取 arithmetic 阶段。

同进程 verification 分段：Range 21.2946 s、公开 rebuild 20.0848 s、GKR 18.9105 s，完整 timer 61.0299 s。完整 Range 分段：membership 382.763 s、reconstruction/opening 246.402 s，总 629.165 s。独立 verifier 分段：Range 23.6295 s、公开 rebuild 21.402 s、GKR 18.7876 s，wall 83.71 s 另包含启动、读文件、解码与回收；driver wall 83.716796 s。

Artifact canonical bytes 与 SHA-256：

| 文件 | bytes | SHA-256 |
|---|---:|---|
| `/tmp/zkgpt-v2-task7-hyrax-opt.statement.bin` | 562,578 | `c7ed3033990582f6ba27a6048f8b3eb8081a48f18ab677a0417165b936876662` |
| `/tmp/zkgpt-v2-task7-hyrax-opt.proof.bin` | 16,683,344 | `376ba9bc14b923adf4dfc2693a120c17fad7d23bc5ba88fbaed069a5bd72a508` |

与任务七 C `/tmp/zkgpt-v2-task7-lifecycle-opt.{statement,proof}.bin` 两次 `cmp -s` 均 exit 0；额外与任务七 A witness-opt、任务七 B lasso-opt 各两次 cmp 均 exit 0。历史文件前后 size/SHA metadata 相同，证据 `historical_artifacts_before.json`、`artifact_results.json`。

结合小型旧实现/新 view canonical bytes 和完整 Challenge Trace 相同，确认保持 protocol v2、GKR encoding v3、ZKGS/ZKPF v2、Transcript domains/Fiat–Shamir 顺序、fingerprint、val0 commitment、Range/GKR/FCONN/Lasso/row-major MLE/IPA/binding/公开输出语义。

## 8. 完整 opening 阶段实测 RSS

单位 KiB，全部为同一个完整生成进程。

| 阶段 | VmSize | VmRSS | VmHWM | ru_maxrss |
|---|---:|---:|---:|---:|
| input_opening_begin | 44,502,340 | 37,963,772 | 47,198,300 | 47,198,300 |
| input_opening_view_ready | 44,502,340 | 37,963,772 | 47,198,300 | 47,198,300 |
| input_opening_reduction_allocated | 44,502,340 | 37,963,772 | 47,198,300 | 47,198,300 |
| input_opening_after_ipa | 44,506,436 | 37,967,740 | 47,198,300 | 47,198,300 |
| input_opening_complete | 44,506,436 | 37,967,740 | 47,198,300 | 47,198,300 |

C 完整 opening 原采样：copy 前 RSS=37,955,764 KiB；copy ready=46,344,360 KiB；IPA 完成但 full copy 仍存活=46,348,344 KiB；copy 释放后=37,959,732 KiB。D 新采样：view 创建前/后均 37,963,772 KiB；IPA 后/返回=37,967,740 KiB。

- 旧/新 opening 采样最高 RSS 差：**46,348,344 − 37,967,740 = 8,380,604 KiB = 7.99237 GiB**。
- opening 前背景 RSS 本轮多 8,008 KiB；旧 opening 自身增量=8,392,580 KiB，新增量=3,968 KiB；增量差 **8,388,612 KiB ≈ 8 GiB + 4 KiB**，与删除完整 8 GiB touched copy 一致。
- old/new complete 后 RSS（分别是 copy-release/view-return）差 8,008 KiB；收益发生于 opening 的重复 witness 同时存活窗口，不是 owner cc.w 永久释放。
- 新 collapsed allocation 16,384×32=524,288 bytes，在已有 allocator pages 中复用时阶段 RSS 可以不增加；capacity 与 RSS 不是同一口径。
- 新最高直接采样在 `lasso_fold_2_after_workers`，VmRSS=47,198,092 KiB、VmHWM=47,198,300 KiB，opening 未刷新 HWM。

**完整 Peak RSS 没有下降。**消除 8 GiB full copy 降低了局部 opening 驻留，但原 Lasso 峰值更高，仍决定整个进程峰值。GKR-only Peak 同样没有下降；不能将局部约 8 GiB 降幅称为完整部署内存下降。

## 9. 对要求逐项回答与剩余限制

1. **Hyrax input opening 确实只读 witness。**API 返回 const Fr&；reduction 只读 view；每列写独立 collapsed；IPA 只修改自有临时向量。审计、输入元素/guard、ASan/UBSan 和旧参考一致性均支持这一结论。
2. **原 8 GiB copy 已消除。**生产 GKR 不再构造完整 vector，只有长度为 N 的 read-only view，删除 8,589,934,592-byte allocation 与等量 payload copy；cc.w owner、数据与 padding 不变。
3. **新 API 保留 borrowed lifetime 约束。**view 不拥有/延长 backing storage，只允许在 owner 存活的 synchronous call 使用；worker 全部 join。Proof 不保存 pointer，测试 owner 析构后可验证。非法几何读取前拒绝；接口不能检查 caller 虚报的存储长度或自行提前释放，因此仍要求 caller 遵守 owner 契约。
4. **Hyrax 局部 opening 采样最高 RSS 降约 7.99 GiB**，对应窗口实际增量消除了约 8 GiB；这是 sampled RSS，不是独立重置的局部 HWM。
5. **GKR-only Peak 未下降**，45,889,604 → 45,890,004 KiB，+400 KiB，Lasso 限制。
6. **完整 Peak 未下降**，47,190,148 → 47,198,300 KiB，+8,152 KiB，约 7.96 MiB，基本不变。该任务完成了零拷贝但没有降低新的部署整体峰值。
7. **未见明显整体性能退化，也没有显著加速证据。**单次完整 wall +0.075%、GKR −0.126%，GKR-only wall −0.64%；Hyrax arithmetic 完整运行 −7.66%、GKR-only 则 2.933664 → 2.961823 s（+0.96%），显示运行波动。历史 timer 不含 copy，不从 wall 差推导精确 memcpy 时间；未进行多轮统计。
8. **Challenge Trace 一致。**75 entries 的 1/8 线程固定向量、70 opening 专项 vectors、9,334 entries 的真实 GKR trace 均匹配相应旧参考/任务七 C。
9. **完整 Proof bytes 一致。**Proof 16,683,344 bytes、Statement 562,578 bytes；与 C 实际 cmp 均 0，A/B 亦相同，完整 SHA-256 不变。
10. **独立 production verifier 完整接受。**生成正常 exit 0 后新启动 file verifier exit 0；原同进程完整生产验证也通过，无接受条件调整。

尚未解决：Lasso 阶段约 45 GiB 全流程峰值、任务七 C 所记的 allocator/static 残留未在本轮优化；其它非 row-major 接口保持不动。未执行 TSan 或广泛 thread-failure 注入；一般 worker 内异常处理是历史实现，本轮无新异步保留。LSan 环境 fatal，不能宣称无泄漏。没有额外 witness ownership/Range/Circuit/Lasso/IPA/Verifier 生命周期重构，未用 malloc_trim 改统计口径。

## 10. 证据与交付

最终报告 `任务七D.md`，独立目录 `task7d_logs_20261012/`：

- `before/`、`baseline_source_sha256.json`、`git_status_before.txt`：修改前源码与状态。
- `task7d_review.diff`、`final_source_sha256.json`、`protected_source_sha256.json`、`release_binary_sha256.json`：此次最小 diff、最终源码、受保护模块与实际验收 binary。
- `unchanged_arithmetic_check.txt`：源码字节比对确认 IPA prover/verifier、全部 MLE verifier、非 row-major proving 与 GKR input-opening 前代码（含 Lasso）保持不变。
- 各 Release/Debug/ASan/UBSan logs、`test_results.json`、`lsan_hyrax.log`：回归与环境限制。
- `hyrax_threads1.log`、`hyrax_threads8.log`、`trace_final_threads1.log`、`trace_final_threads8.log`：线程一致性与固定挑战。
- `optimized_gkr_only.log/.exit.json`、`full_prover.log/.exit.json`、`independent_verifier.log/.exit.json`：全规模阶段 RSS、time/Peak、接受与退出证据。
- `artifact_prefix.txt`、`artifact_results.json`、`historical_artifacts_before.json`：新路径、SHA/bytes/cmp 与历史保护。
- `run_tests.py`、`run_acceptance.py`、`check_final.py`、driver logs：前置 gates、串行流程和最终核验。

未自动 Git commit；未清理/reset 既有未提交内容；未覆盖历史 Artifact。
