# B3-R1 Implementation Plan

> For agentic workers: execute using executing-plans; test-driven-development and verification-before-completion apply. No Git commits.

**Goal:** 在完整复现目标下，完成已具充分数学依据的基础模块，并精确阻塞未解决的论文语义。
**Architecture:** 新 `src/paper_r1` 独立基础库；不依赖旧 strict-v4、mixed-C0 或 B1/B2 生产接口。PC、FS、Sumcheck、Lasso memory、Range 独立可测试。未获依据的组合不伪造实现。
**Tech stack:** C++17、现有 MCL BN254、SHA256、Python跨进程驱动。
**Spec:** SPEC.md。

## Constraints and decisions

保护已有工作；所有既有文件修改需 fresh baseline hash guard。只新增独立模块和构建目录；不创建 Git worktree，因本任务已指定实际目录且不能 checkout/commit。完整 HEAD/status、模型哈希、文献版本记录于 BASELINE/ENVIRONMENT。现有日志永不覆盖。

## Tasks

- [x] 1. 固定正式论文和官方上游版本，fresh capture所有相关报告及实际模型/anchor哈希；对旧源码逐文件列出唯一状态及替代验收。
- [x] 2. `src/paper_r1/core.hpp, core.cpp, pc.hpp, pc.cpp`：先编译 PC/Transcript 测试观察缺接口的失败，再实现 fresh FS、明确 PC 身份、balanced commit/MLE IPA。验证 n=1、奇偶维、两种布局、错误 witness、非预期 role/长度/点/claim/round。
- [x] 3. `sumcheck.hpp, sumcheck.cpp, memory.hpp, memory.cpp`：先运行缺实现测试，再实现 degree2/3 sumcheck、product tree 和 indexed memory。验证重复地址、错误地址/值/计数、拼接产品、修改 transcript 和大于256的 query。
- [x] 4. `range.hpp, range.cpp`：先运行缺实现测试，再实现精确 partial digit、随机点评估重构及零尾线性声明；验证 b=1/9/10/64/126、2^b边界、signed bias代数、padding、reconstruction tampering。不接有歧义的 Exp profile。
- [x] 5. `wire.hpp, wire.cpp, tests_core.cpp及模块tests_*.cpp, verify_main.cpp, independent.py, CMakeLists.txt`：canonical有界解码；独立 Verifier只消费公开声明和证明，不读witness、模型文件或调用Prover。跨进程验证及reject矩阵、截断/尾随/超长输入。
- [x] 6. 独立数学和代码审查（依据适用技能的明确review要求）；修复重要问题并重跑所需suite。保留完整构建、测试、diff/hash和最终报告，不宣称完整协议成功。

## Shared interfaces

Task2 -> 3/4: PCSpec/Commitment/OpenProof, Fs, mle/eq/point；所有 Fr/G1 从相同runtime field导入。Task3 -> 4/5: SumcheckProof/ProductProof/MemoryProof 及 public Statement；Verifier使用调用方的期望Spec。Task4 -> 5: RangeStatement/RangeProof，不包含模型语义。Wire按确切 public shape解码。

## Review focus

随机标量 canonical rejection 的方向/字节序；奇数维的 row/column 点拆分；degree3 product树LSB位序；未认证的内部 root/leaf；Verifier不能用Proof选择expectedSpec；actual padding必须单独证明；Fr wrap与partial-limb；proof解析资源上限；不把基础库提升为完整协议。

## Continued implementation: exact integer relations

用户指示继续后，独立实现已由论文明确的关系，不因真实模型profile尚未确定而停止。

- [x] `integer_relations.hpp/.cpp/tests_integer.cpp`：A/q 都以signed Range+active-mask绑定到各自PC。对公开0<B<=2^62，证明 S=2A−2Bq+B·active，T=(2B−1)·active−S，S/T非负有界。故 −1/2<=A/B−q<1/2，排除闭端点双解。A/q位宽<=64，整数表达式远小于field，禁止靠模回绕。
- [x] 最大值：score和m均signed Range；D_i=m−score_i在active域为非负b-bit值，tail为0。证明product(active·D+inactive)=0；padding叶必须用1，不能用0。affine随机点证明D=m·active−score；产品叶最终打开同一D PC。由真实active域至少一项相等，得到真正最大值。测试全负score、偏高max、末尾padding造成的伪零产品。
- [x] 先观察缺接口失败，再实现并测试；文档明确这两条是可验证约束实例化，未自动替代旧真实模型电路。它们的wire/全模型adapter不得计作已完成。

- [x] `matrix.hpp/.cpp/tests_matrix.cpp`：Z Protocol1(5)的专用矩阵sumcheck。行主序A(i,k)、B(k,j)、公开完整C(i,j)；固定输入PC和C后取输出行列点，degree2 sumcheck归约到同一A/B PC的两次MLE opening。actual矩形之外分别以随机权重线性证明为0，公开C的补零直接检查。非2次幂实际维数与padding必须覆盖；当前不承诺论文grouping加速或真实模型adapter。

## Explicit unclamped Exp instance

- [x] 定义一个公开、明确的新数学实例 `unclamped-u32-St2^-9-Se2^-20`，不更改旧profile，不宣称整图已经一致。表T[j]=round(2^20 exp(-j/512))，0<=j<8192。u=j/16384<=1/2，以正的有理数Taylor交错区间P33<=exp(-u)<=P32，取32次方并乘2^20；两端取整一致才生成条目。8191经证明为0，单调性证明全部更大u32索引为0。生成8192项及重建测试，无浮点。
- [x] 对原始t/E的PC，Range证明t为u32；辅助s为bit、x为13bit、D为u32并具有zero tail。active域：s=[t<8191]，x=s*t+(1-s)*8191，D=s*(8190-t)+(1-s)*(t-8191)。以eq加权degree3 sumcheck认证这两条逐坐标二次关系，不能用MLE乘积冒充乘积向量的MLE。
- [x] native lookup 用同一x及E_native=E+2^20·inactive；其padding为合法表项(0,T[0])，通过随机点的仿射opening认证与原始E的关系。生产数学查询仍是原始(t,E)，x只是已证明等价的压缩地址。用完整公开表的indexed memory认证；不把helper分散membership当作Exp。
- [x] tests覆盖0、6891、7453、8191、8192、2^32−1、2^32拒绝、旧clamp1尾值拒绝、非2次幂actual/padding与branch篡改。先观察缺接口失败，再实现。wire/真实模型adapter若未实现必须明确记录。

## Continued: committed variable denominator and positive sqrt

独立实例化Z §4.3的明确关系，仍不选择完整GPT量化profile。`division.hpp/.cpp/tests_division.cpp`：A、B、q均由调用方选择的原始PC固定，signed A/q各<=64 bits，B−active在b<=62-bit无符号范围，故active域1<=B<=2^b。认证S=2A−2Bq+B、T=2B−active−S；S/T均(b+1)-bit无符号。两个eq加权sumcheck闭合到原始A/B/q和同一S/T PC；不可用MLE(B)*MLE(q)替代逐坐标乘积。由0<=S<=2B−1得到半开取整，所有整数残差小于2^129，排除field alias。补零由helper Range与仿射绑定和约束共同覆盖。

`positive_sqrt.hpp/.cpp/tests_positive_sqrt.cpp`：明确限定x>=1、y>=1，吻合上游LN中`B>=1`、`sigma!=0`的上下文。Z §4.3的平方根式对整数正y等价于y²−y+1<=x<=y²+y。对原始x/y PC认证S=x−y²+y−active、T=y²+y−x，S/T非负；x unsigned<=64 bits，y−active unsigned<=63 bits，slack各root_bits+1 bits。degree3 eq-weighted sumcheck和相同PC opening。该正域实例不声称证明x=0的sqrt；论文印刷下界对x=y=0不成立，零域处理保留明确边界，不自行伪造正式公式。

验收：先缺接口RED，再正确实例、正负tie、变量B=1及2^62、零/负B、错误q、修改原始commitment、helper/endpoint/sumcheck、非2次幂padding；sqrt测试1/2/3/6/7/12/13、最大u64输入和相邻错误输出、源域及补零、独立审查。新模块不计入wire/全模型集成，除非实际完成相应代码与测试。

## Full-target work still open

- [ ] Canonical full-model profile and complete witness/query builder, including Attention scale and every fused operator.
- [ ] Fresh compact-W real-model anchor and full12 X/W/Y inference proof in an independent process.
- [ ] Scalable model scheduling and full-model wire deployment.
- [ ] Hiding PC, protocol masking, simulator, joint knowledge extractor and composed security proof.
- [ ] Retire replaced production files after complete replacement acceptance.
