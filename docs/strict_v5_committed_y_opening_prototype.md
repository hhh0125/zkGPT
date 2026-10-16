# strict-v5 Committed-y 隐藏线性求值证明原型

2026-10-14，Asia/Shanghai。实际项目：`/home/yttan/project/zkGPT/zkGPT-main/zkGPT`。

本轮已实现并编译长度 **2/4** 的独立实验原型。新增单元测试 **59/59**、本轮重新执行任务十测试 **43/43**、独立 Verifier **30/30** 预期退出码通过。未修改 Field LogUp、Range/GKR/Exp Lookup、模型/embedding、val[0] 或 Hyrax；未启动12-block证明，未正式上线strict-v5。下列PASS只适用于本原型与明确写出的安全模型。

## 1. 最终验收与证据边界

| 项目 | 状态 |
|---|---|
| Committed-y primitive design | PASS |
| Hidden scalar commitment | PASS |
| Shared-witness binding | PASS |
| Completeness | PASS |
| Special soundness | PASS |
| Knowledge soundness | PASS |
| Special HVZK | PASS |
| Fiat-Shamir NIZK assumptions | DOCUMENTED |
| Independent verifier | PASS |
| Negative tests | PASS |
| Serialization robustness | PASS |
| Legacy strict-v4 unchanged | PASS |
| Field LogUp integration | NOT IMPLEMENTED |
| Full zkGPT Zero-Knowledge | NOT CLAIMED |

Special soundness和Special HVZK为交互式Sigma的精确代数性质；Knowledge soundness为该关系的交互式知识证明，以及下文明确条件下的经典ROM Fiat–Shamir知识论证。没有宣称固定真实SHA256的标准模型安全、QROM、simulation extractability、并发/UC可组合安全、恶意Verifier交互式ZK、constant-time、秘密内存擦除或第三方密码认证。测试不能替代分布论证或形式化安全归约。

## 2. 对任务十真实代码与日志的审计

读取了远程 `src/zk_hiding_commitment.hpp/.cpp`、`src/zk_linear_opening.hpp/.cpp`、`src/main_zk_opening_tests.cpp`、`src/CMakeLists.txt`、任务十报告和真实日志，字节副本在 `task10-audit-snapshot.json`。本轮没有把历史43/43、16/16当新结果：任务十43项是在任务十一隔离Release构建中重新执行；其历史16个进程结果仅审阅，没有冒充本轮重跑。

1. **曲线与字段。** 调用 `mcl::bn::initPairing(mcl::BN254)`；实际曲线为 `y²=x³+2`，Fp与Fr均为素数域，G1为阶r的群。运行 `field-probe` 确认：

   ```text
   r = 16798108731015832284940804142231733909759579603404752749028378864165570215949
   p = 16798108731015832284940804142231733909889187121439069848933715426072753864723
   curve_b = 2
   Fr_bytes = 32
   G1_bytes = 32
   ```

   这是MCL BN254套件，不能自动等同Ethereum的BN_SNARK1。API沿用任务十默认MCL hash/map和序列化设置的前置条件，独立程序固定初始化。未解决共享进程中其他代码改变MCL全局设置的问题；正式集成须冻结套件并测互操作。

2. **ρ与采样。** `randomCommit` 调用 `sampleUniformScalar`；Linux `getrandom` 填满32字节，EINTR重试，错误/零字节立即异常；规范Fr解码并重编码比对，拒绝值≥r，不做mask或模归约，允许0，最多4096次。任务十一的η、d、a、e全部调用同一接口，没有新增秘密RNG。OS CSPRNG安全是实现假设；理想均匀字节下成功输出精确均匀Fr。

3. **public-y消息。** 旧Statement为 `(pp,C,b,y)`；首消息 `A_C=Commit(d,a)`、`A_F=<b,d>`；响应 `z=d+cw`、`z_ρ=a+cρ`。验证 `Commit(z,z_ρ)=A_C+cC` 与 `<b,z>=A_F+cy`。y确实公开，不能隐藏后续LogUp秘密claim。

4. **兼容与隔离。** 任务十API、实现、报告、日志未修改。新API在 `zkproto::committed`，保留向量生成元域及原C以便复用，标量生成元/Transcript/codec独立。生产 `gpt_lib` 原本排除任务十源码；本轮增加排除新committed源码。实际 `compile_commands.json` 中26个生产source无任何原型source，旧生产二进制未重建。

## 3. 新协议完整定义

公共参数 `pp=(suite,n,G_0,…,G_{n-1},H,U,H_y)`，n∈{2,4}。公开：

\[
x=(pp,version,domains,context,C,D_y,\mathbf b).
\]

秘密为 `(w,ρ,η)`，`y=<b,w>`仅在Prover内部计算。关系：

\[
C=\sum_i w_iG_i+\rho H,\qquad
D_y=\langle\mathbf b,\mathbf w\rangle U+\eta H_y.
\]

同态 `F_b:Fr^{n+2}→G1²` 将同一个w映射到这两个分量。不能改为两份互不关联的w。

Prover先验证两个真实开口，再独立均匀采样 `d∈Fr^n,a,e∈Fr`，发送：

\[
A_C=\sum_i d_iG_i+aH,\qquad A_D=\langle b,d\rangle U+eH_y.
\]

FS挑战 `c∈Fr\setminus\{0\}`；响应：

\[
z_i=d_i+cw_i,\qquad z_\rho=a+c\rho,\qquad z_\eta=e+c\eta.
\]

Verifier独立重算c并同时验证：

\[
\boxed{\sum_i z_iG_i+z_\rho H=A_C+cC},\qquad
\boxed{\langle b,z\rangle U+z_\eta H_y=A_D+cD_y}.
\]

两式使用**同一个响应向量z**。Proof仅有 `(A_C,A_D,z,z_ρ,z_η)` 和公开元数据；不发送y、未掩码 `<b,d>`、w或原始盲因子/nonce。

## 4. Completeness与共同Witness的Special Soundness

将响应代入，第一式为 `Commit(d,a)+cCommit(w,ρ)`；第二式为 `(<b,d>U+eH_y)+c(<b,w>U+ηH_y)`，因此理论协议完美完备，含零w、零b、零y。实现对熵源异常和有限拒绝采样耗尽fail closed，因此不声称所有机器状态均能产生proof。

对同一Statement、相同首消息、挑战c≠c′的两份接受记录，定义：

\[
w_i=\frac{z_i-z'_i}{c-c'},\quad
\rho=\frac{z_\rho-z'_\rho}{c-c'},\quad
\eta=\frac{z_\eta-z'_\eta}{c-c'}.
\]

分别相减两条验证式并乘 `(c−c′)^{-1}`，得到同一w满足两个承诺。无需先求离散对数；知识提取是代数事实。交互式挑战空间大小r−1，知识误差为1/(r−1)，经典可重绕PPT模型中超过该误差的成功率允许知识提取。测试验证两条记录的两式和提取后的两式，不在生产API暴露任意挑战或检查绕过入口。

注意循环群向量承诺不是信息论唯一开口；带独立盲因子的F也是满射。证明“某个preimage存在”本身很弱；协议证明知道共同开口，且要与计算绑定假设一起使用，才能保护此前固定的w。

## 5. Special HVZK与隐私审查

仅输入公开x和任意给定非零c的模拟器，独立均匀选择 `z∈Fr^n,z_ρ,z_η∈Fr`，令：

\[
A_C=Commit(z,z_\rho)-cC,\qquad
A_D=\langle b,z\rangle U+z_\eta H_y-cD_y.
\]

两式必定接受。真实执行中 `(d,a,e)→(d+cw,a+cρ,e+cη)` 是Fr^{n+2}上的双射，所以固定x,c时响应的联合分布与模拟器完全相同，首消息也由这些响应及x唯一确定。**模拟器不需要y、w、ρ或η。** 实际CSPRNG版本对应计算不可区分而非物理机器真随机声明。

对 `b=0`，y=0可由公开b逻辑推出；这是Statement本身的信息，不能声称此时y未知。但w仍为秘密，A_D=eH_y，z仍含独立d。不存在旧无mask endpoint的 `z/c=w` 恢复；数学模拟器覆盖此边界，测试亦覆盖n2/n4。测试观察fresh nonces或简单恢复失败只是回归证据，不能单靠统计结果证明ZK。

审计header、encoder、challenge输入和输出日志：公开字段中无原始y、未掩码 `<b,w>`、原始ρ/η/d/a/e。z和两个blind responses是掩码响应；其隐私来自上述联合模拟性。fixture发射使用OS随机w/盲因子；已知零边界仅在内存中构造，不输出其开口。单元测试源码含用于字段编码回归的公开负数常量，实际随机fixture的秘密从未写入文件。异常诊断只包含固定错误描述。

## 6. Hiding、Binding与生成元假设

H、H_y非单位元且在素数阶r群中。固定任意w时，均匀独立ρ、η分别使 `C=constant+ρH`、`D=constant+ηH_y` 独立均匀于G1。因此 `(C,D)` 的**联合**分布为G1²上的product-uniform，与w无关；共享w没有额外可观察相关性。Proof在给定这两个承诺下仍可联合模拟，不能仅分别讨论两个commitment的hiding。

低层 `commit/scalarCommit` 接受调用方盲因子；固定/偏置/相关盲因子不自动具备上述隐藏保证。正常 `randomCommit/randomScalarCommit` 各自重新采样，允许0是均匀Fr的正确行为，不应以排除0作为隐私证明。

不同C开口碰撞产生非零关系 `ΣΔw_iG_i+ΔρH=0`；不同标量D开口碰撞产生 `ΔyU+ΔηH_y=0`。计算绑定依赖素数阶群DLog/representation困难以及公开生成元之间未知离散对数关系。D只绑定求值，不绑定w所有坐标；不同w若内积相同，D对应同一个y是合法语义。

复用旧向量生成元域 `zkGPT/experimental/zk-hiding-commitment/v1` 的 `/g/i`、`/h`。新标量域 `zkGPT/experimental/zk-committed-linear-opening/generators/v1` 使用 `/u`、`/h-y` 的hashAndMapToG1；Verifier固定派生并检查完全匹配，检查非零、曲线、子群及与所有旧bases的相等/负相等碰撞。它们在数学上不“线性独立”；需要关系未知假设，不能改用公开hash标量乘同一basis来制造已知关系。

## 7. Fiat–Shamir模型、Nonce和重复证明

新Transcript固定域 `zkGPT.zkproto.committed-linear-map.MCL-BN254.fs.sha256.v1`，吸收length-prefixed规范Statement（版本、向量/标量域、n、context、C、D、所有b），所有G_i/H/U/H_y、Proof版本/域和两个首消息。响应只做shape检查，按Sigma顺序不提前吸收。追加LE32 counter后SHA256，复用任务十 `scalarCandidate`，拒绝0与非规范编码，最多4096次；不接受外部提供challenge。

**经典可编程ROM模拟。** 先选均匀c和所有响应并计算A。令 `q=(r−1)/2^256`，成功counter依几何分布选择，显式包括4096次全失败abort。对每个较早counter，独立抽取32字节串，**均匀条件于该串被scalarCandidate拒绝或等于0**，终点program为canonical(c)。成功counter下条件分布与真正RO拒绝采样相同；若按成功条件模拟，差别最多为全失败概率 `(1−q)^4096`。其他RO输入正常随机回答。

如果已有查询与需program的首消息counter输入冲突，模拟器重试/abort。给定x，两个首消息由独立aH和eH_y具备G1²的均匀熵；对多项式先前查询，碰撞bad event概率可忽略，有限采样cap再加可忽略abort项。raw SHA256实现中没有programming接口；该模拟器只属于ROM论证，不能要求在固定真实SHA256下仅凭x求出接受proof。

**知识提取条件。** 经典forking在固定Statement、两个首消息和更早拒绝counter回答的情况下，重绕有效challenge查询，获得另一份接受记录及不同非零c，再用共同preimage提取器。论证依赖可重绕PPT adversary、经典RO、规范无歧义绑定、足够高熵challenge及成功概率超过查询/猜测误差；实现SHA256作为RO实例化的假设与理论Sigma性质分开。未做机械化完整reduction，没有QROM或强组合声明。

每次 `prove` 重新采样全部d/a/e，没有cache、确定性nonce或跨调用复用。若相同mask在不同challenge重用，则差分直接恢复w、ρ、η；跨Statement复用也可能泄密。固定C/D/b的多次fresh-mask证明可按上述逐个模拟与RO碰撞界分析；不同Statement的应用组合还必须明确公开输入泄露、挑战时序和context。未推广到任意并发协议。

context是公开应用绑定，由调用方固定并检查预期值。改变context使原proof被密码学拒绝；CLI可额外接收EXPECTED_CONTEXT。不改变Statement/context的重放自然仍合法，本原型不是会话防重放服务。

## 8. 新增文件、函数与codec

| 文件 | 职责 |
|---|---|
| `src/zk_committed_linear_opening.hpp` | Parameters/Statement/Proof/ScalarCommitment；公开类型没有y |
| `src/zk_committed_linear_opening.cpp` | `makeParameters/validateParameters`、`scalarCommit/randomScalarCommit`、`challenge/prove/verify`、四个encode/decode函数 |
| `src/main_zk_committed_opening_tests.cpp` | 单元测试、仅测试的恶意证明构造、公开fixture、匹配public-y benchmark |
| `src/main_zk_committed_opening_verify.cpp` | 独立公共文件Verifier，固定MCL初始化，可检查EXPECTED_CONTEXT |
| `src/CMakeLists.txt` | 一处已有source排除正则扩展，追加默认OFF的独立选项目标 |

Statement magic `ZKCS`，Proof magic `ZKCP`，原型version1，不能冒充生产strict-v5。与旧 `ZKLS/ZKLP` 交叉解码拒绝。u32为小端，每个Fr/G1为默认MCL32字节；n仅2/4；每个域/context≤128 bytes，每条消息≤4096；先检查长度再分配；解码点检查曲线/子群，所有element重编码逐字节匹配；拒绝尾随字节和截断。单位元可作为合法C/D/首消息，生成元禁止单位元。

独立选项 `ZKGPT_BUILD_COMMITTED_Y_PROTOTYPE=ON` 构建library、test和专用Verifier，只链接MCL。为同条件比较，隔离库含未修改的public-y实现；没有生产链接或运行路径。RED stubs只在任务十一日志目录用于失效测试，从未链接最终CMake目标。未增加生产绕过检查或RNG注入接口。

## 9. 正向、负向与真实退出码

最终 `unit-tests-reviewed.log`：**59 PASS / 0 FAIL，exit0**。任务十 `task10-regression.log`：**43 PASS / 0 FAIL，exit0**。最终 `ctest-reviewed.log` 中两套原型测试 **2/2** 通过。

正例覆盖n2/n4、全零w、负数Fr、零系数、b全零、非零b且y0、非零y、同w新ρ、同w新η、同C/D多次fresh proof、规范roundtrip、合法identity和context边界。nonce新鲜性检查8次重复证明；其无碰撞观察不替代理论随机性。

负例覆盖C/D/b/A_C/A_D/z/z_ρ/z_η/context修改、spliced commitments、错误共同w、错误η、generator替换与domains/version、维度、所有Proof/Statement截断位置、尾随字节、非法G1、各响应非规范Fr、精确Fr modulus、巨型长度、域/context限制、整消息cap、旧codec交叉解析、熵源失败。熵故障在隔离fork中seccomp使getrandom返回EIO，不影响父进程或其他服务器用户，scalar blinding与opening masks均异常退出，不返回proof。

明确区分三条路径：

- **Prover本地拒绝**：错w/长度/η或拼接D，捕获实际异常；整体单元测试进程exit0，不冒充独立crypto拒绝。
- **解码/格式拒绝**：不规范wire独立进程exit2。
- **密码学拒绝**：规范Statement/Proof独立进程exit1。特别 `wrong-shared-w` 和 `wrong-eta-forged` 由测试代码自行构造FS首消息/响应，显式验证第一群式通过、第二群式失败，再送给专用Verifier；真实诊断均为 `committed D equation failed`。没有只依赖Prover assert。

## 10. 独立进程结果

专用Verifier每个样本启动新进程，仅打开 `.statement.bin/.proof.bin`，没有Witness文件或加载路径。30/30结果包含 **6 ACCEPT、12 CRYPTO_REJECT、12 DECODE_REJECT**，逐项真实退出码如下；完整诊断/RSS在 `independent-results.json` 和各独立日志中。

| 样本 | 预期exit | 真实exit | 分类 |
|---|---:|---:|---|
| invalid-G1 | 2 | 2 | DECODE_REJECT |
| noncanonical-Fr | 2 | 2 | DECODE_REJECT |
| oversized-message | 2 | 2 | DECODE_REJECT |
| oversized-n | 2 | 2 | DECODE_REJECT |
| positive-b0 | 0 | 0 | ACCEPT |
| positive-n2 | 0 | 0 | ACCEPT |
| positive-n4 | 0 | 0 | ACCEPT |
| positive-repeat | 0 | 0 | ACCEPT |
| positive-zero-w | 0 | 0 | ACCEPT |
| positive-zero-y | 0 | 0 | ACCEPT |
| spliced-D | 1 | 1 | CRYPTO_REJECT |
| trailing | 2 | 2 | DECODE_REJECT |
| truncated | 2 | 2 | DECODE_REJECT |
| wrong-A-C | 1 | 1 | CRYPTO_REJECT |
| wrong-A-D | 1 | 1 | CRYPTO_REJECT |
| wrong-C | 1 | 1 | CRYPTO_REJECT |
| wrong-D | 1 | 1 | CRYPTO_REJECT |
| wrong-b | 1 | 1 | CRYPTO_REJECT |
| wrong-context | 1 | 1 | CRYPTO_REJECT |
| wrong-eta-forged | 1 | 1 | CRYPTO_REJECT |
| wrong-magic | 2 | 2 | DECODE_REJECT |
| wrong-scalar-domain | 2 | 2 | DECODE_REJECT |
| wrong-shared-w | 1 | 1 | CRYPTO_REJECT |
| wrong-transcript-domain | 2 | 2 | DECODE_REJECT |
| wrong-vector-domain | 2 | 2 | DECODE_REJECT |
| wrong-version | 2 | 2 | DECODE_REJECT |
| wrong-z-eta | 1 | 1 | CRYPTO_REJECT |
| wrong-z-rho | 1 | 1 | CRYPTO_REJECT |
| wrong-z | 1 | 1 | CRYPTO_REJECT |
| wrong-expected-context | 2 | 2 | DECODE_REJECT |

更改Statement或任一首消息通常先改变FS挑战，导致第一式失败，这不意味着第二式未被约束：保持第一式有效的两个专用恶意样本均在第二式拒绝。独立domain变更按格式拒绝；内存API另测不可信generator参数被拒绝。公开manifest给每个fixture的SHA256和字节数，不保存任何秘密开口。

## 11. 同条件性能与内存

Intel Xeon Platinum8352V@2.10GHz；Ubuntu GCC12.3.0；Linux5.15；C++17 Release `-O3 -DNDEBUG`，native tuning OFF，无sanitizer，单线程构建。每种协议/n各100次，**同一机器、同一二进制与构建、同一w/C/b**，交替执行顺序；每轮fresh masks。测量steady_clock的prove/verify API调用，包含参数重复派生/验证、local opening checks、OS randomness、FS和群式；排除初始承诺生成、codec与进程启动。两个接口各自正常工作量不同，这正是本轮API性能，不代表仅群运算的纯协议开销。共享服务器未做负载隔离，均值不能外推生产场景。

| 协议 | n | rounds | Prove均值us | Verify均值us | Proof bytes | Statement bytes |
|---|---:|---:|---:|---:|---:|---:|
| public-y | 2 | 100 | 3352.11 | 2211.18 | 215 | 186 |
| committed-y | 2 | 100 | 8868.59 | 5533.92 | 263 | 280 |
| public-y | 4 | 100 | 4484.56 | 2864.33 | 279 | 250 |
| committed-y | 4 | 100 | 11100.2 | 6664.46 | 327 | 344 |

Proof数学payload为2个G1和n+2个Fr，通信O(n)。此codec的n2/n4为263/327 bytes；Statement数字对应25-byte context与固定域。旧public-y本轮复测，不使用任务十历史时间直接算差值。性能为单次100-round观测，没有置信区间或方差估计。

benchmark进程总Peak RSS **4900 KiB**，并非每proof新增内存；最终59项unit进程 **5004 KiB**。独立Verifier最大RSS **4944 KiB**。初次完整隔离构建峰值 **327112 KiB**。初始CMake依赖有既有版本弃用警告与MCL gmp_util.hpp的maybe-uninitialized警告；第三方未改。新增源初次有两处misleading-indentation，已格式修正，后续增量编译无新增源警告。未做ASan/UBSan或穷尽fuzz；wire robustness PASS严格限于实现审查和已执行恶意边界。

## 12. 生产及历史完整性

开工前检查Git/进程/内存。服务器main已有脏修改与两个删除；未reset、clean、stash、checkout、commit或恢复缺失旧文件。用户要求保留当前工作树，采用原地独立文件/opt-in目标和 `task11_logs_20261014/build` 隔离构建，未从HEAD复制而遗漏untracked任务十实现。

baseline保存97份原有source hash（含顶层CMake），最终96份逐字节不变，只有 `src/CMakeLists.txt` 为精确允许的两项增量；408条现存历史文件size/mtime记录不变，15个既有生产二进制SHA256不变。生产compile_commands列26份source，新旧原型均为0引用。任务八日志在起始基线中不存在，没有补写；历史完整性仅针对起始确实现存材料。元数据比较不等同对全部历史大型文件重新hash；源码和生产二进制则做SHA256比较。

任务十所有源码/报告/日志保持原样，新测试单独重编未修改其旧build。没有重新构建生产二进制，没有运行完整旧生产测试/12-block proof；“Legacy unchanged”依据字节/元数据/生成源列表证据，不伪称全部legacy功能重新验收。

## 13. Field LogUp / 隐藏Sumcheck后续可行性

本轮 **NOT IMPLEMENTED**。隐藏S、m̃(r)、q̃(r)、ã(r)时，可以各自形成新鲜η的D，使用公开MLE系数b与已隐藏承诺向量证明一致，Verifier只读D而非Fr。但这一步只覆盖线性关系：

1. **reciprocal_sum**：对secret reciprocal向量a和b=全1证明S=<1,a>的D；仍需证明每个a_i满足 `(β+q_i)a_i=1`，β时序、零分母检查和范围/成员约束不能省略。
2. **两个Sumcheck共享隐藏claim**：优先让两协议引用同一个D及固定上下文；若用独立盲因子的两个承诺，需要证明相同值的承诺相等关系，不能各自证明“存在开口”后假定相等。后续每轮也要隐藏round多项式系数/求值，并证明g(0)+g(1)=前claim、g(r)=后claim等承诺线性关系和正确终端约束。
3. **隐藏MLE一致性**：b为Verifier公开随机点的多线性基底，D通过共同w opening与正确已承诺向量关联。新承诺必须在相应挑战前固定，域/维度/坐标/layout/session/circuit一同绑定。
4. **秘密乘积关系**：Sumcheck终端的m̃·ã、(β+q̃)ã及其他乘积不是本原型的公开系数线性关系，需经过安全分析的ZK multiplication/committed-sumcheck组件和盲因子一致性证明。不能把秘密求值当作b公开。
5. **桥接val[0]**：新隐藏向量承诺和原行/布局必须用共同Witness的坐标映射证明连接，不能凭长度、摘要或承诺看起来相似。不同basis时需joint-representation或高效隐藏MLE桥，固定public layout映射及原锚；新primitive不会撤销旧确定性承诺已泄露的信息。
6. **Sparse Opening**：模式evaluation及总evaluation都改为隐藏承诺，使用隐藏线性组合/相等证明；若再调用旧公开Fr/最终witness输出路径，隐私仍失败。
7. **规模**：当前σ响应为n个Fr，n131072仅响应部分已超过4MiB且接口明确拒绝；不能把n2/4bench外推全部76032查询，也不能用每次O(n)开口当生产可扩展方案。
8. **高效路线必要条件**：选定经过审查的次线性ZK dot-product/隐藏MLE及committed-sumcheck；隐藏所有round/terminal消息、公共表soundness、乘积/倒数证明、共享claim和val0桥、挑战依赖时序、参数/domain/codec/上下文固定、联合模拟/知识论证、独立错误Witness拒绝与低内存性能验证。完整Zero-Knowledge还需迁移其余旧泄露通道。

## 14. 记录、恢复与停止点

`progress.md` 逐阶段记录修改/结果/下一步，`commands.jsonl` 记录实际argv、退出码和日志。初期经历本地sandbox网络禁止（后获授权）、Windows命令长度限制（远程未执行）、RED脚本假定静态MCL库（实际为共享库；上传文件保留并校验后恢复），以及一次本地引用错误（远程未执行）。这些操作问题未被当作密码测试失败或掩盖。真实RED为3个缺失功能案例exit1，其后GREEN、独立验证和复测都保留单独日志。

独立静态审阅未发现Critical/Important问题；两项Minor已落实：ROM较早拒绝输出的均匀条件分布写清，Statement增加所有截断位置和专属header长度回归，最终59项通过。Reviewer未独立运行远程测试，运行数据由执行者真实采集，未称其为密码认证。

断线后优先读progress、final-summary、commands及最终测试/结果JSON；不要从头重跑fixture发射（O_EXCL会拒绝覆盖）。交付含源码、报告、设计/实现计划、review、hash/manifest、逐进程日志及bench。完成后保持工作树及历史证据，不提交或自动迁移生产。

到此停止。下一阶段必须补充隐藏Sumcheck、秘密乘积/倒数、共同claim相等证明、val0坐标桥、隐藏Sparse以及次线性opening；不能据此标记Field LogUp或完整zkGPT ZK PASS。

## 一手参考资料

Schnorr型Sigma的special soundness/HVZK定义参见[Damgård《On Σ-protocols》](https://cs.au.dk/~ivan/Sigma.pdf)，公开线性同态接口参见[CFRG《Sigma Proofs for Linear Relations》draft03](https://www.ietf.org/ietf-ftp/internet-drafts/draft-irtf-cfrg-sigma-protocols-03.html)。本报告的双承诺提取、联合隐藏和模拟器是对本协议的直接代数推导；draft与讲义不构成本实现认证。只向互联网检索通用一手资料，没有上传项目源码、witness、模型或内部proof。
