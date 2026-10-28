# R7_H_FIXED_COMMITMENT_SECURITY

日期：2026-10-23（Asia/Shanghai）。Stage1 Task0-B，suite H-PC-CSIPA-AUDIT-RESEARCH-v1。仅新增 reports/R7-STAGE1-TASK0B-20261023-v1；全部源码编辑、测试、测量在固定cwd /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work 执行。N≤1024、1≤batch K≤4、每MSM≤32 bases。没有lookup table，因而没有超过64项的表；没有读取真实120参数、R6 proof或真实W anchor，没有真实注册、生产verifier、oneblock/full12或生产ZK。

冻结源码575项重新SHA核对；真实参数/proof/anchor仅记录前后stat元数据、复用既有SHA身份，不为验证冻结而读取内容。原may_enter_stage2=false不变。以下为可人工复核的数学证明，非机器形式化或生产认证。

## 1. 定理输入和范围

固定I中的全部A_i、points、Y handles、suite/layout/dimensions/session，假设诚实P有有效witness。simulator仅得到I，不得到W/ρ_W/y/η、注册者秘密、生成元DL或trapdoor。在理想独立uniform-Fr模型下，精确交互opening对固定I具有perfect HVZK。下面直接模拟固定bytes，不使用替换Ai的hybrid。

这不修复历史非隐藏PC本身的泄漏；opening只保证在已有公开信息条件下不增加额外泄漏。stablePC的公开链接性属于允许信息。

## 2. 单核心模拟器与完整双射

给定ξ,Y,a：

1. S采样c←Fr*、z←Fr^C、s,u←Fr，令t=<a,z>。
2. 计算A=Σz_jG_j+sH−cξ，B=tG+uH−cY。
3. 输出A/B，再输出预采样c和s/u/t。
4. S知道masked z，诚实运行response IPA；χ/fold使用独立诚实随机币。

不需要ξ/Y的任何开口或离散对数。检查由构造成立。

严格分布证明：固定有效x,ρ,y,η和c，真实随机量(dvec,δ,β)与模拟量(z,s,u)之间：

    z=dvec+cx, s=δ+cρ, u=β+cη
    inverse: dvec=z−cx, δ=s−cρ, β=u−cη

这是Fr^(C+2)上的双射；两边均匀，产生完全相同A/B/t。给定这些量，IPA的已知z和未来χ/fold同分布，故整个transcript相同。再对c平均即联合perfect equality。a=0时t=0，双射仍成立，不依赖y非零。

S先选c是HVZK模拟权限，不是live prover能够提前知道挑战。若允许prover提交自己的receipt，自然可模拟任意视图，因此原型明确禁止把receipt当FS/NIZK proof信任来源。

## 3. 固定PC、多point与多session

先与真实V同分布取audit b，再公共求ξ_b，以a=0/Y=identity模拟。每个evaluation ξ_l是固定Ai和公开point的公共组合，各core使用fresh独立z/s/u/c及未来IPA coins。对每个固定public prefix，真实/模拟条件分布相同；链规则给出整个有限batch的联合HVZK。samePC、重复point、sameY均不改变该论证。

多个顺序session共享固定PC、后续输入依赖已观察公开transcript时，同样归纳模拟诚实V的视图。这里不是live恶意V或任意并发/重置ZK。若本次产生fresh Y，η均匀使Y均匀，S可先取均匀Y再对其模拟，仍不需知道相对于真实y的η。固定外部Y则原样保留，不能替换。

## 4. 固定C_W的准确回答

未来C_W若是相同suite/layout的hiding row PC，局部opening模拟在不知道W和ρ_W时成立，不需要模型注册者秘密。真实P仍须持有W和对应ρ_W，这是relation的witness，不是S的额外能力。只把W交给外包P而不给承诺开口随机性，未必能诚实打开原C_W；注册/密钥分发策略需未来明确。

冻结R6 nonhiding anchor的suite、bytes、layout不属于本变体，不能自动升级或重注册；本轮没有读取真实anchor。结论：固定PC下局部交互式opening已有充分数学依据；固定模型生产NIZK还不足，缺FS/注册/全关系组合等。

## 5. 初始hiding、随机源与实现偏差

理想每行ρ_i独立均匀，Ai各自均匀，与W独立，故PC perfect hiding；binding取决于未知Gj/H关系。模拟本身不使用DL假设，knowledge和一致性才使用。

实现OS urandom+规范Fr rejection，guard拒绝全零、重复block、短读、异常和预算耗尽。若OS blocks理想均匀、总D≤200000，全零/重复事件union bound：

    [D+D(D−1)/2]/2^256.

在无guard/预算失败事件下与理想采样耦合。每scalar最多4096尝试，reject尾概率(1−q/2^256)^4096；nonzero coin将q换q−1；再乘调用数。硬budget耗尽就是显式失败，不能fallback到旧nonce。真实OS质量不是信息论保证。

guard仅进程内跨Entropy实例，不跨重启/VM回滚/机器。没有reset安全、malicious OS或生产RNG认证。实际采样带这些条件事件，故不把理想perfect HVZK直接称为实现无条件perfect。

## 6. 退化与abort概率

- c/χ/fold/b按Fr*定义非零，输入0拒绝，不存在除0。
- a_final=0、zero y/W、identity合法承诺不abort，全部方程照查。测试实际构造MLE a=(1/2,1/2)、f²=−1使a_final=0；每audit也使用a=0。
- duplicate point/sameY不额外abort，fresh masks必须独立。
- 对未条件化独立uniform原始抽样，平方碰撞概率为2/(q−1)每pair；已筛选accepting children不能直接用此条件概率，搜索预算另见MULTI报告。不是诚实协议退化事件。
- 理想SHA512、固定素数阶group下，单H2C最多4096搜索失败概率(1−(q−1)/2^257)^4096，乘生成元数作union bound。简单相等/相反基碰撞O(M²/q)不替代更一般DL-relation假设。
- 非法curve/subgroup/非规范点拒绝，合法identity保留。不能用拒绝identity来伪装解决零relation。

## 7. 未来FS的必要条件和未证明项

必须固定完整参数/instance/C_W/layout/PC/handles/session及每挑战前缀，定义各stage域、nonzero sampling、重试/grinding预算、RO查询损失、joint模拟编程和expected-poly extractor。不能把HV simulator先选c权限当作真实SHA可编程，也不能删除trusted receipt参数便称FS。

未证明：malicious-V、任意并发、reset、生产R7的ROM知识损失与注册组合；全GKR/Lasso/bridge/master的隐藏关系未实现。上述均保持OPEN/PARTIAL。
## 原始文献与确切适用性

[Hyrax extended 29页全文](https://eprint.iacr.asia/2017/1132.pdf)，本地literature/hyrax-mirror-0.pdf，SHA256 6440b28eb3d692b144772c660c42607c5ef158306a33691a03921925a8d48296。已读所需完整§5（PDF7–8页）、§6.1（9页）、A.2（18–19页）、A.3（19–20页）、A.4（20–21页）、A.5/A.6（21页）及整体证明（21–25页）。作者conference版可由[作者站](https://www.cs.nyu.edu/~mwalfish/papers/hyrax-oakland18.pdf)交叉核对。官方ePrint下载403，因此extended使用先前保存镜像；不宣称镜像bytes已与官方当前版认证一致。

[Bulletproofs作者45页完整版本](https://web.stanford.edu/~buenz/pubs/bulletproofs.pdf)，SHA256 f7b6c9297bdd5b5705b3a4866f1f0dcd9d236517642265854667e6794619e4bc。已读相关定义（8–12页）、完整Protocol1/2与验证（12–17页）、AppA通用forking和AppB完整提取证明（38–41页）。本版Thm1/AppB对应Hyrax引用的较早版Thm2/AppA，不混用编号。裸IPA不是ZK。

Hyrax A.2 Figure6提供masked-response Sigma关系；A.3 Figure7/8是另一紧凑协议，终端有其特定公共权重方程；§6.1/A.5提供row compression与多row提取思路。本文选择A.2响应压缩并独立证明公开权重IPA及新增audit，不套用A.3 Lemma12的n+2提取数。Hyrax §5/A.4的coeff commitment只作未来接口参考，没有实现sumcheck。整体Theorem15替换input commitments的hybrid没有用于固定PC模拟。
