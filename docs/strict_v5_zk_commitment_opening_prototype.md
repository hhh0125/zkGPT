# strict-v5 隐藏承诺与零知识线性 Opening 最小原型

日期：2026-10-14，Asia/Shanghai。项目：/home/yttan/project/zkGPT/zkGPT-main/zkGPT。

本轮实现独立、实验性的 BN254 Primitive，长度仅2/4。没有迁移 strict-v4、Range/GKR、Exp Lookup 或 Model Binding，没有修改模型/embedding，没有生成完整 GPT Artifact，没有运行12-block证明。生产版本没有升级成strict-v5。本报告的PASS严格限于下述原型与安全模型，不代表完整zkGPT零知识。

## 1. 最终验收

| 字段 | 状态 | 证据/限定 |
|---|---|---|
| Primitive design grounded in reviewed construction | PASS | 标准 generalized Schnorr 同态Sigma协议；设计独立审阅无阻塞，完整提取/模拟推导见下文 |
| Hiding commitment implementation | PASS | 新鲜系统CSPRNG字节、规范Fr拒绝采样、独立H；真实测试通过 |
| Hiding security argument | PASS | 理想均匀rho下完美隐藏；实际依赖OS CSPRNG不可区分于均匀随机字节 |
| ZK linear opening implementation | PASS | 同时检查commitment和public-y线性方程，不使用旧final_witness |
| Opening completeness | PASS | n2/n4等正例和独立进程通过；理论Sigma完备，具体有熵源失败/极小拒绝采样耗尽abort |
| Opening binding / knowledge soundness | PASS | Pedersen未知离散对数关系假设；Sigma双记录精确提取；FS依赖经典ROM/forking |
| Opening Zero-Knowledge | PASS | 完美special HVZK的代数论证；非交互版只在经典可编程ROM和安全CSPRNG模型下声称计算ZK |
| Independent verifier | PASS | 16/16预期退出码；只读取公开Statement/Proof的新进程 |
| Serialization robustness | PASS | 长度限制、规范Fr/G1、点/子群检查、重编码比对、截断/尾随/非法编码拒绝；非穷尽fuzz结论 |
| Legacy strict-v4 unchanged | PASS | 基线84份旧source中83份完全同SHA256，仅src/CMakeLists.txt有精确允许增量；2个生产二进制相同；42份现存历史证据元数据未变 |
| Full zkGPT Zero-Knowledge | NOT CLAIMED | 原协议的秘密消息、隐藏claim、跨承诺桥接均未迁移 |

PASS来自本轮真实执行与明确的模型内论证。没有声称SHA256在标准模型下被证明ZK、QROM安全、simulation extractability、并发可组合安全、constant-time或第三方密码学认证。未做机械化形式验证。

## 2. 起始状态、旧缺口和范围

首先核实docs/softmax_exp_lookup_zero_knowledge_audit.md存在，读取真实Hyrax、IPA、Range、Sparse、Transcript和CMake。旧行承诺为sum(w_i G_i)，没有rho H；旧IPA每轮a'=c·a_L+c^-1·a_R并最终公开folded witness。Sparse还公开模式级内积。这些不是本轮新的修复结果，旧生产代码保持原状。

旧定位：src/hyrax_rp.cpp:127–131、331–374；src/range_logup.cpp:199–209；src/hyrax_opening.cpp:266–290；src/sparse_opening.cpp:264–283。旧generator_cache.cpp:43–54使用hashAndMapToG1。旧Transcript的setHashOf/hash链与Fr字段均可公开重建（range_protocol.cpp:207–252），不能用随机挑战代替隐藏。

服务器起始Git为main，已有35个未暂存修改、2个删除、1712个未跟踪项，无暂存改动。未reset/stash/checkout/commit。用户授权在指定当前目录增加独立组件，故采用独立目标和构建目录隔离，未复制HEAD而丢失脏工作树。

**本轮开始时task8_logs_20261013已不存在；task9_logs_20261014现存42个文件。** baseline.json记录的就是本轮实际基线。不能用过去的344/132文件清单证明当前完整历史证据仍存在，也不把此前PASS当本轮复测。本轮没有删除或重建那些缺失文件。

初次检查无运行中的Prover/Verifier；MemAvailable约503698856KiB。构建始终单线程，只构建mcl依赖和zk_opening_tests目标，不构建gpt_lib或demo_llm_run。初始配置有MCL CMake版本弃用警告；第一次编译依赖有gmp_util.hpp的maybe-uninitialized警告，未修改第三方库，也未把该警告宣称为已证明漏洞。

## 3. 选择的具体构造及与Hyrax的区别

采用标准Schnorr型Sigma协议，证明公开线性同态的preimage：

\[
F_{\mathbf b}:\mathbb F_r^{n+1}\longrightarrow G_1\times(\mathbb F_r,+),\quad
F_{\mathbf b}(\mathbf w,\rho)=
\left(\sum_iw_iG_i+\rho H,\langle\mathbf b,\mathbf w\rangle\right).
\]

这是标准generalized Schnorr，同态目标群也可写为同阶群的直积，不是把随机项拼到旧IPA。Sigma completeness/special soundness/special HVZK定义见Damgård讲义；公开linear-map proof见CFRG draft。[R1][R2]

Hyrax Appendix A.2/Figure6使用向量和标量隐藏承诺、随机masked responses证明dot product。[R3] 本轮将y明确公开，使用scalar nonce <b,d>作为同态首消息的第二分量；不需要对公开y再建秘密标量承诺。这个public-y变体不是逐字移植Hyrax的committed-y字节协议，也不是Appendix A.3的对数通信opening。以下直接推导完整证明义务。

## 4. 公开/秘密与数学协议

公共参数pp=(suite,n,G0..Gn-1,H)，n∈{2,4}。公开Statement：

\[
x=(pp,C,\mathbf b,y),\quad
R(x;\mathbf w,\rho)=
[C=\sum_iw_iG_i+\rho H]\land[\langle\mathbf b,\mathbf w\rangle=y].
\]

秘密witness为(w,rho)。负整数以Fr模r编码，如-5对应r-5。y本身是明确允许公开的线性函数；本原型不保证y隐藏。若发布足够多的独立public-y查询而恢复w，这是Statement主动释放信息，不能用opening ZK来禁止该行为。

### Prover

1. 检查pp由Verifier固定派生，C/w/rho和public-y一致，长度/编码有效。
2. 独立均匀采样d∈Fr^n、a∈Fr；每次proof重新采样，不复用。
3. 首消息：

\[
A_G=\sum_i d_iG_i+aH,\qquad A_F=\langle\mathbf b,\mathbf d\rangle.
\]

4. Challenge c∈Fr\\{0}，交互版均匀；实现使用规范FS。
5. 响应：

\[
\mathbf z=\mathbf d+c\mathbf w,\quad z_\rho=a+c\rho.
\]

### Verifier

独立重导出pp、检查类型/版本/编码，重算c，检查：

\[
\boxed{\sum_i z_iG_i+z_\rho H=A_G+cC}
\]
\[
\boxed{\langle\mathbf b,\mathbf z\rangle=A_F+cy}.
\]

证明只有(A_G,A_F,z,z_rho)及公开域/版本元数据。rho、d、a、原w都不序列化；z/z_rho是随机化响应，不是改名的原folded witness。新的rho H要求群验证式中的z_rho H；旧无blinding IPA方程无法原样验证新承诺。

## 5. Hiding、Binding、Completeness与Knowledge Soundness

**Hiding。** G1为素数阶r、H非零，所以rho↦rho H是双射。固定w时C=constant+rho H在理想均匀rho下均匀于G1，与w无关。实现使用安全系统字节源加拒绝采样；均匀字节模型下输出均匀Fr，真实OS CSPRNG的伪随机性是实现安全假设。不能仅以“两个承诺不同”作为隐藏证明。

commit(pp,w,rho)是低层代数接口，本身不会强制调用方rho随机。正常数据入口randomCommit生成fresh blind；若调用方自己指定固定rho，不能声称该承诺隐藏。这不是用于测试或生产的随机数后门。

**Binding。** 两个不同开口产生非零关系sum(Delta w_i G_i)+Delta rho H=0；安全依赖hash-to-curve派生点之间离散对数关系未知、prime-order group的DLog/representation困难。不是信息论唯一：循环群中的多项向量承诺天然有很多开口，但高效找碰撞应困难。点派生没有直接采用“公开hash scalar乘固定生成元”。

**Completeness。** 将z=d+cw、z_rho=a+c rho代入两式即成立，包含w=0、b中含零、b=0/y=0。理论Sigma有完美完备性；有限采样实现有fail-closed的熵源不可用异常和极小耗尽概率，不谎称任何机器状态都接受。若b=0/y≠0，没有合法witness；local producer拒绝。恶意首消息可能猜中单个挑战，因此soundness属于概率性论证，而不是一个负例证明绝对不可能伪造。

**Special soundness/knowledge extraction。** 两份相同(A_G,A_F)、挑战c≠c'的接受记录，输出：

\[
w_i=(z_i-z_i')/(c-c'),\quad
\rho=(z_\rho-z_\rho')/(c-c').
\]

群式和Fr式相减得到上述witness同时满足C与y关系。这一代数提取是精确的，不需要先把scalar relation转成未知DLog假设。Binding用于保持已承诺数据不可改开，知识提取是另一层性质。非交互知识结论依赖经典RO、rewinding/forking及足够高熵challenge；本轮没有QROM extractor或simulation-sound证明。

实际BN254为MCL的mcl::BN254，**不是自动等于Ethereum的BN_SNARK1套件**。本轮公开探针输出：

~~~text
Fr_order=16798108731015832284940804142231733909759579603404752749028378864165570215949
Fp_modulus=16798108731015832284940804142231733909889187121439069848933715426072753864723
Fr_bytes=32
G1_bytes=32
~~~

factory检查这两个modulus和编码宽度。默认MCL hash/map与序列化设置是库API前置条件；独立CLI固定初始化。未承诺其他组件改变全局MCL配置后仍兼容，也不声称BN254达到某个未经本轮评估的安全位数。

## 6. 独立审查发现与均匀采样修复

设计审阅没有发现同态/提取/模拟公式阻塞。代码审查却发现真实依赖缺陷，远程读取确认：

- fp.hpp:505–514的setByCSPRNG使用setArrayMask。
- setArrayMask:404–406先保留字段bitSize位，若值≥r再清最高位。一些结果出现概率是其他结果的两倍，不是uniform rejection sampling。
- fp.hpp:510–513的读取结果检查写成if(!pb)，检查指针而非*pb；熵读取失败时可在最终异常前处理未填满数据。

因此最初37/37功能测试通过不足以支持原先的uniform coins论证。这个中间版本没有作为最终安全原型交付，也没有生成完整GPT证明。

只在任务十实现sampleUniformScalar：Linux getrandom填满32字节，EINTR重试；错误或0字节读取立即抛出，无fallback，不解码未填满buffer。Fr规范解码和重编码比对拒绝值≥r，不使用mask/mod归约；接受0。每次最多4096个候选。FS的挑战同样规范拒绝，但额外排除0。

理想源下，每个允许Fr编码恰对应一个32字节串，因此成功输出条件分布精确均匀。每个secret draw的耗尽概率=(1-r/2^256)^4096；FS为(1-(r-1)/2^256)^4096。加入次数的union bound即可，实际熵源错误单列为可用性失败。

确定性candidate测试拒绝r和全FF、接受0和r-1。测试还fork一个小进程，通过仅该子进程的seccomp使getrandom返回EIO，证明randomCommit与prove的mask路径均fail closed。没有生产RNG注入接口、固定blind或绕过验证开关。此前RED阶段保留旧采样行为的characterization已由最终实现替换；只有历史日志/RED-only stubs留在证据目录。

sampler-red.log：2 PASS/3 FAIL、exit1，随后prototype-final-tests.log：43 PASS/0 FAIL、exit0。熵故障回归约束的是新getrandom接口；它不是实际破坏旧MCL fread的运行实验，旧读取结果问题由源码确认。

共享MCL未修改，故旧协议的随机采样及其他隐私问题没有因此修复。新原型不调用其setByCSPRNG用于任何secret mask/blind。

## 7. Zero-Knowledge模拟器与FS

### 7.1 精确special HVZK

模拟器只知x和给定c，独立均匀选择z∈Fr^n、z_rho∈Fr，设：

\[
A_G=\operatorname{Commit}(z;z_\rho)-cC,\qquad
A_F=\langle b,z\rangle-cy.
\]

返回(A_G,A_F,c,z,z_rho)。总是满足验证式。真实执行给定c时，d↦z=d+cw、a↦z_rho=a+c rho为双射，真实z/z_rho同样独立均匀；首消息由它们与x确定，联合分布完全相同。实现的CSPRNG版本对应计算不可区分模型。

b=0/y=0时A_F=0，但A_G由独立aH保证完整nonce熵；z仍为uniform mask+w，不可用旧z/c方法提取w。测试只验证旧简单提取不成功和新鲜nonce，不替代此分布证明。

### 7.2 非交互安全模型

FS吸收规范Statement（version、generator domain、n、C、每个b_i、y），所有G_i/H、proof version/opening domain、A_G/A_F，再吸收LE32 counter并SHA256。响应不在挑战前吸收，这是Sigma顺序；域分离字串固定为zkGPT.zkproto.linear-map.fs.sha256.v1。

经典可编程ROM模拟器使用上述public-only算法选c/z/z_rho，产生首消息。按拒绝采样的几何分布选择成功counter：先前counter的SHA输出模拟为“不可接受Fr编码或0”，终点输出program为canonical(c)。重算c即等于模拟c。若先前对手已查询相同首消息对应的相关oracle输入，模拟器重试/abort；群nonce在理想模型至少有r种等概率值，这个bad event对多项式查询数可界为negligible。4096 cap加其极小abort项。

真实oracle和模拟oracle其余回答按RO分布提供；raw SHA256实现没有模拟/编程后门。不能要求该模拟器在固定真实SHA256下凭x直接找合法proof，这是ROM编译与标准模型的区别。FS知识提取通过fork最终有效challenge查询，保持同一nonce而获得不同c，再应用special soundness；需要标准resettable经典RO分析，未声明量子或强组合定理。

同一witness的多次proof必须fresh d/a/rho（新承诺）或fresh d/a（同一公开C）；本轮benchmark同一C的每轮opening使用fresh masks。没有把Fiat–Shamir本身当作mask或隐藏论据。

## 8. 文件、函数与隔离

| 文件 | 内容 |
|---|---|
| src/zk_hiding_commitment.hpp | Public Parameters、SecretWitness、RandomizedCommitment和独立API |
| src/zk_hiding_commitment.cpp:22、44 | 固定experimental域hashAndMapToG1，验证重导出参数、非零/有效/子群和碰撞 |
| 同文件:63、75 | commit、randomCommit；增加rho H |
| 同文件:81、90 | canonical scalarCandidate、OS均匀sampleUniformScalar，立即检查读取失败 |
| src/zk_linear_opening.hpp | 明确区分public Statement和masked LinearOpeningProof |
| src/zk_linear_opening.cpp:93–124 | 两个独立规范codec |
| 同文件:125、147、166 | FS、Prover、本轮新Verifier两式检查 |
| src/main_zk_opening_tests.cpp | 单元/公开fixture/独立Verifier/小benchmark；不生成witness文件 |
| src/CMakeLists.txt | 从gpt_lib source list始终排除新增两个cpp；默认OFF的独立library/test目标 |

Generator Domain=zkGPT/experimental/zk-hiding-commitment/v1。Gi用/g/i，H用/h独立派生；Verifier不接受prover任意提供的known-related points。Opening Domain=zkGPT/experimental/zk-linear-opening/v1。生产generator_cache支持域没有改变。

Wire primitive version=1，magic分别ZKLS/ZKLP；它不是生产strict version=5。每个Fr/G1默认MCL32字节；u32小端；domain最长128 bytes；n只能2/4；总codec最长4096 bytes，先检查长度后分配；G1有效/子群、Fr规范和反序列化后重编码完全匹配；拒绝尾随字节。群单位元作为合法C/nonce不被禁止，只有生成元禁止零。

构建路径task10_logs_20261014/build。新library只链接MCL，无gpt_lib。compile_commands核对新增源码在gpt_lib中数量为0。既有CMake内容和脏改动保留；没有重配/重建旧cmake-build-release生产目标。

## 9. 真实测试、退出码和恢复

主要命令及结果来自commands.jsonl；progress.md逐阶段记录，不依赖聊天恢复：

~~~text
c++ ... main_zk_opening_tests.cpp red_stubs.cpp ...        exit0
red_tests --commitment                                    exit1 (expected RED)
commitment_tests --commitment                             exit0 (5/5)
commitment_tests --opening                                exit1 (expected RED)
cmake -S PROJECT -B task10_logs_20261014/build
  -DCMAKE_BUILD_TYPE=Release -DZKGPT_BUILD_ZK_OPENING_PROTOTYPE=ON
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON                       exit0
cmake --build BUILD --target zk_opening_tests --parallel 1 exit0
zk_opening_tests                                         exit0 (initial37/37)
zk_opening_tests --sampler                                exit1 (new regression RED)
cmake --build BUILD --target zk_opening_tests --parallel 1 exit0
zk_opening_tests                                         exit0 (final43/43)
zk_opening_tests --emit task10_logs_20261014/fixtures       exit0
zk_opening_tests --benchmark                              exit0
~~~

首次本地SSH命令的alias拼写错误在远程执行之前失败，已纠正；不影响远程源码。一次本地编排模板错误未发出工具调用，同样没有远程副作用。日志中真实早期失败与最后通过都保留，没有覆盖旧日志或掩盖审查缺陷。CMake依赖警告见第2节；新primitive编译无警告。

43个unit案例逐项PASS保存在prototype-final-tests.log，包含：5个sampler/entropy案例、5个commitment案例、n2/n4正例、零witness、负整数/零系数、随机系数、b=0 privacy、roundtrip、opening entropy失败、修改y/C/b/z/A_G/A_F/z_rho、混合witness/材料、wrong generator point/domain、transcript domain/version、b0非零y、3个local producer拒绝、8个codec拒绝及special soundness/simulator代数检查。代数检查并不执行RO编程，也不是统计证明ZK。

local producer拒绝是被测试捕获的异常，整体unit process仍exit0，不能冒充独立Verifier拒绝。独立CLI只读取Statement与Proof并重导出pp；错误分类为accept=0、crypto=1、decode/format=2：

| 独立案例 | 真实exit | 分类 |
|---|---:|---|
| positive-n2 | 0 | ACCEPT |
| positive-n4 | 0 | ACCEPT |
| positive-b0（n4） | 0 | ACCEPT |
| wrong-y | 1 | cryptographic rejection |
| wrong-C | 1 | cryptographic rejection |
| wrong-b | 1 | cryptographic rejection |
| wrong-response | 1 | cryptographic rejection |
| wrong-blind-response | 1 | cryptographic rejection |
| mixed-proof | 1 | cryptographic rejection |
| wrong-transcript-domain | 2 | decoding/domain rejection |
| wrong-generator-domain | 2 | decoding/domain rejection |
| mutated-magic | 2 | decoding rejection |
| truncated | 2 | decoding rejection |
| trailing | 2 | decoding rejection |
| invalid-G1 | 2 | decoding rejection |
| noncanonical-Fr | 2 | decoding rejection |

16/16符合expected值，见independent-results.json和各independent-*.log。mixed-proof独立样本使用同值向量/不同rho的不同SecretWitness，unit案例另覆盖不同w值重算proof及混合响应。未混淆拒绝路径。样本和SHA256清单仅包含公开材料，无rho/d/a/w文件；它们不是GPT Artifact。

恢复时先读progress.md、commands.jsonl、final测试与结果JSON，不重跑已完成阶段。新日志均独占创建；初始RED-only stubs不链接到最终CMake目标，没有正式数据路径调试后门。没有重复运行历史完整proof或legacy测试。

## 10. 真实性能与内存

每种n使用100次fresh masked opening；Release构建、单线程。下表平均时间来自C++steady_clock，**不包含commitment初次生成、序列化或进程启动**，包含Prover/Verifier重复重导出/验证pp、MSM、随机数及FS；不是仅一个MSM的时间。

| n | 样本数 | 平均Prove | 平均Verify | Proof字节 | Statement字节 |
|---:|---:|---:|---:|---:|---:|
| 2 | 100 | 4876.17 us | 3253.32 us | 215 | 186 |
| 4 | 100 | 6900.39 us | 4460.21 us | 279 | 250 |

来源：prototype-benchmark.log、benchmark-results.json。该benchmark整个进程peak RSS=4952KiB≈4.836MiB，是共享n2+n4进程总峰值，不是每个proof新增workspace。最终unit process peak=4908KiB；独立verifier最大记录=5084KiB。初次隔离build peak=327244KiB，最终增量build=298752KiB。

没有报告未测的完整Lookup开销或速度倍率；原型通信O(n)，不能把这两个小n时间外推到val0/76032 queries。Commitment单独性能、时间方差、单proof新增RSS、ASan/UBSan、fuzz与constant-time未实测。

## 11. strict-v5后续集成设计（未执行）

| 模块 | 必需工作 |
|---|---|
| Exp Lookup 8列 | 每个secret row fresh blind；派生pair commitment同时线性派生blind，仍在alpha之前固定原列 |
| multiplicity/reciprocal | 均匀独立隐藏；m在beta前、reciprocal在beta后；成员关系和倒数sum恒等式不变 |
| Sumcheck秘密claim | 当前public-y API不能用于隐藏claim。改为public隐藏标量承诺D_y，使用Hyrax committed-y dot-product/相等/乘积证明，不发送y数值 |
| Sparse Opening | 每个pattern评价与总评价都hidden-committed；用ZK线性/相等关系关联，不再调用旧公开Fr输出的opening |
| 原val0 | 建立明确同witness桥，不以相同长度/相同摘要替代坐标绑定 |
| 原Range/GKR | 所有secret round多项式值、terminal evaluations、IPA endpoints一并零知识化，保留真实circuit与registry约束 |
| Model Binding | 公开选择mask不是随机隐私mask；混合行/参数相等也改用隐藏evaluation opening |
| 版本/Transcript | 正式strict-v5、新suite/domain、专用规范codec与context组合，不接收旧v3/v4冒充 |

### 隐藏y扩展的具体关系

今后可使用标准同态Sigma（或原Hyrax committed-y协议）证明

\[
(C,D_y)=
(\sum_i w_iG_i+\rho H,\;U\langle b,w\rangle+\eta H_y)
\]

的共同preimage(w,rho,eta)。两个group nonce用同一d向量、各自独立blind mask，第二分量不公开<b,d>数值；Verifier检查两个group方程，提取同一w。须有新的scalar generators/域、blind consistency、codec和证明审查，**本轮没有实现这项扩展**。秘密product/倒数关系还需要额外已有ZK multiplication/committed-sumcheck构造，线性opening单独不够。

### 原deterministic val0的桥接

设原行C0=sum_j u_j G_j^old，公开布局L选出新查询，Cq=sum_i(Lu)_i G_i^new+rho Hnew。需要证明共同u满足两式，而不是将两个不同basis承诺直接相减。可先以标准joint-representation Sigma建立baseline：同一随机mask d用于旧行nonce及新(Ld) nonce，响应z=d+c u同时进入两个Verifier方程；提取共同u、rho并核对public L。

这个baseline通信线性于u，当前n2/n4 API不支持生产大行；未来应选已有ZK logarithmic dot-product/隐藏MLE桥并证明同一坐标关系。即使旧/新basis恰相同且差为rho H，也不能省略Query→原row坐标绑定和knowledge组合论证。

若保留旧确定性C0和公开legacy view，第一阶段目标只是Sim_inc(x,v_old)的条件增量隐私。expContext含旧proof hash，不能把该hash当模拟器免费私有输入。完整Sim(x)需要原全部proof也可模拟，且Statement已经公开的模型digest/确定性锚泄露不会被新primitive撤销。

未来Soundness回归仍须：verifier固定表、全76032查询、正确t/E mapping、uint32尾部E=1、原C0共享绑定、独立严格Verifier、旧版本拒绝、真实指数倍率×2拒绝、原Range/GKR/Model Binding不削弱。只有另行授权才能迁移和完整证明验收。

## 12. 独立审查、剩余问题与停止点

设计审阅接受线性map/Sigma结构；代码审阅发现sampler Critical/Important，本轮远程确认后以RED→GREEN修复，完整43/43验证了修复，无第二轮重复审查。该独立审阅不是正式密码认证。

审阅中延后的小项：

1. 额外边界覆盖：n2的b0、合法零C/零nonce的显式案例、超大domain及完整4096输入上限。代码有检查/允许语义，但当前测试不穷尽这些情形；Fr模数/0/FF边界已作为sampler修复必须回归完成。
2. MCL全局配置hardening：API声明默认hash/map/序列化前置条件；未实现防止其他组件修改全局配置。正式集成需冻结suite并做互操作测试。

已列出的非本轮范围：完整GPT/隐藏y集成未做；time/cache/内存zeroization未审计；未重新执行历史proof与原回归，Legacy unchanged由源码/二进制/现存证据基线证明。原LayerNorm/GeLU/rounding的历史Soundness风险仍需其他任务处理。共享MCL的有偏采样仍存在于旧代码，不能把本原型的修复扩大为旧协议修复。

本轮全部要求范围内的Primitive实施、测试、独立验证及报告完成。到此停止，不自动实现Field LogUp、Sparse、Range/GKR或Model Binding迁移。

## 13. 引用、来源及证据定位

2026-10-14核对一手资料；互联网只检索通用协议/论文，没有上传项目源码、模型、witness或proof。

- **[R1]** Ivan Damgård. *On Σ-protocols*，Definitions 1–2、special soundness/HVZK、knowledge extraction与RO讨论：[作者讲义](https://cs.au.dk/~ivan/Sigma.pdf)。
- **[R2]** *Sigma Proofs for Linear Relations*, draft-irtf-cfrg-sigma-protocols-03，2026：[CFRG/IRTF Internet-Draft](https://www.ietf.org/ietf-ftp/internet-drafts/draft-irtf-cfrg-sigma-protocols-03.html)。是draft，不称为最终RFC或对当前实现的标准认证。
- **[R3]** Riad S. Wahby, Ioanna Tzialla, abhi shelat, Justin Thaler, Michael Walfish. *Doubly-efficient zkSNARKs without trusted setup*, 2018，Appendix A.2/Figure6：[作者版PDF](https://cs.nyu.edu/~mwalfish/papers/hyrax-oakland18.pdf)。用于对照hiding/dot-product路线；未声称本原型与其完整Hyrax implementation等价。
- **[R4]** Timothy Kassis, Vinayak Agarwal, Yuhuan He, Darshil Patel, Aubrey M. Brueckner. *Scientific Agent Skills: A Library of Procedural Knowledge for Research Agents*, 2026：[来源](https://doi.org/10.48550/arXiv.2609.00065)。仅披露引用/证据流程技能来源，不作为密码安全证据。

运行证据：task10_logs_20261014/progress.md、commands.jsonl、prototype-final-tests.log、independent-results.json、benchmark-results.json、public-artifact-manifest.json、integrity-verification.json。baseline.json保存84份旧source hash、42份现存证据元数据及2个旧生产二进制hash；报告后的最终核对另外保存，不覆盖这些记录。

