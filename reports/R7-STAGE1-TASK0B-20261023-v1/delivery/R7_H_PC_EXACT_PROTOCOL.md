# R7_H_PC_EXACT_PROTOCOL

日期：2026-10-23（Asia/Shanghai）。Stage1 Task0-B，suite H-PC-CSIPA-AUDIT-RESEARCH-v1。仅新增 reports/R7-STAGE1-TASK0B-20261023-v1；全部源码编辑、测试、测量在固定cwd /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work 执行。N≤1024、1≤batch K≤4、每MSM≤32 bases。没有lookup table，因而没有超过64项的表；没有读取真实120参数、R6 proof或真实W anchor，没有真实注册、生产verifier、oneblock/full12或生产ZK。

冻结源码575项重新SHA核对；真实参数/proof/anchor仅记录前后stat元数据、复用既有SHA身份，不为验证冻结而读取内容。原may_enter_stage2=false不变。以下为可人工复核的数学证明，非机器形式化或生产认证。

## 1. 选定的精确研究版本

H-PC-CSIPA-AUDIT-RESEARCH-v1 = Hyrax §6.1 blinded rows + A.2 committed-output Sigma + 完整masked-response的public-weight compact IPA + 独立row-knowledge audit。与原A.3不同：增加response认证和audit，换取可直接给出的固定commitment模拟/提取与不除公共权重的终端。不是给R6 nonhiding IPA加随机数，也不继承R6或原A.3的安全身份。

## 2. 参数和relation

加法素数阶群G，标量域Fr=Fq；实现独立使用公开BN254数值参数：

    q=16798108731015832284940804142231733909759579603404752749028378864165570215949
    p=16798108731015832284940804142231733909889187121439069848933715426072753864723
    curve y²=x³+2 over Fp, subgroup order q

所有G_scalar、H、G_j共同满足未知非平凡DL关系假设。实现分别用suite与output/G、blind/H、column/j域分离hash-to-curve；绝不能用已知scalar乘同一base，或令G_scalar=G_0。点互异不等于未知DL关系。以下简写scalar base为G。

N=2^d，R=2^floor(d/2)，C=2^ceil(d/2)，Hyrax列主序 W_ij=w[i+R*j]：

    A_i=Σ_j W_ij G_j+ρ_i H.

PC公开suite、actual、padded、R/C、layout、全部A_i与规范hash identity。每row独立均匀ρ_i时perfect hiding，binding为计算性。actual仅metadata；本PC relation不额外证明actual之后为零，未来zero-tail/Sumcheck仍须认证。诚实commit补零不作为恶意prover约束。

HiddenScalar为完整(suite,role-id,Y)，Y=yG+ηH。role-id不是全局秘密值身份；完整group commitment才是handle。相同标签但不同Y不算同handle，同batchalias冲突拒绝；跨session也必须比较完整对象。η由prover持有，wire不发y/η。

公开statement I包含全部PC、K个point r_l与Y_l、session/context。witness Ω=(W,ρ_i,η_l)，满足每个A_i以及：

    Y_l=MLE(vec(W),r_l)G+η_l H.

取L=χ(r前logR位)、a=χ(r后logC位)，χ为Boolean equality weights。令x_j=Σ_i L_iW_ij、ρ=ΣL_iρ_i，公共ξ=ΣL_iA_i。则ξ=Σx_jG_j+ρH且MLE=<a,x>。这里行位是低位；不是直接复用R6 row-major布局。

## 3. 核心opening的消息、随机币和检查

输入ξ,Y,a；P知道x,ρ,η，Y=<a,x>G+ηH。

1. P取独立dvec∈Fr^C、δ,β∈Fr，发送A=Σd_jG_j+δH和B=<a,dvec>G+βH。
2. A/B固定后V取c←Fr*。
3. P计算z=dvec+cx、s=δ+cρ、u=β+cη、t=<a,z>，只发送s,u,t；不发送z。
4. V检查cY+B=tG+uH；公共计算Rz=cξ+A−sH。
5. 运行下面compact proof，证明Rz=Σz_jG_j且<a,z>=t。

s/u/t及final scalar是对完整fresh随机向量掩码后的response，不能描述为没有scalar；它们不是原始evaluation、ρ、η或folded witness。

## 4. response IPA精确定义

Rz,a,t已固定后V取χ←Fr*，置U=Rz+χtG；初始g_j=G_j。维护U=Σz_j(g_j+χa_jG)。

P每层发送：

    Lcross=Σ z_L,j g_R,j+χ<z_L,a_R>G
    Rcross=Σ z_R,j g_L,j+χ<z_R,a_L>G

之后V取f←Fr*，更新：

    U'=U+f²Lcross+f^(-2)Rcross
    g'=f^(-1)g_L+f g_R
    a'=f^(-1)a_L+f a_R
    z'=f z_L+f^(-1)z_R

C=1时P发送z_final，V检查U=z_final(g_final+χa_final G)。没有除a_final或要求其非零；a_final=0时仍检查U=z_final*g_final。零W/零y/identity commitment均不免检查。fold平方互异仅为extractor的分叉条件，不是要求实际各轮coin互不相同。

## 5. 整向量row audit

单个ξ知识不能推出所有row开口知识。固定整个batch的PC/points/Y/shape/layout/suite/session后，V取独立b←Fr*。公共ξ_b=Σ_i b^i A_i；使用同核心，a=0向量、Y=identity、η=0，P的x=Σb^iW_i、ρ=Σb^iρ_i。完整运行，包括zero t的认证。

每batch仅一次audit，随后K个evaluation core。它不是模型注册或lookup；R个不同b的Vandermonde提取证明在MULTI报告。它增加一个compact core，不发送整个W。

## 6. Completeness证明

z=d+cx、s=δ+cρ给出ΣzG=A+cξ−sH；t=<a,d>+cy、u=β+cη给出tG+uH=B+cY。

IPA把Σ(fz_L+f⁻¹z_R)(f⁻¹g_L+fg_R+χ(f⁻¹a_L+fa_R)G)展开，恰为旧U+f²Lcross+f⁻²Rcross。归纳到终端，任何a_final均成立。row/columnχ权重相乘是原vector索引i+Rj的χ权重，因此y是精确MLE；audit取a=0亦成立。

理想群、规范编码、非零挑战下perfect completeness；实现的有限H2C搜索、entropy health/预算失败单独列事件，不声称实现无条件perfect。

## 7. 交互与wire

statement固定所有公开身份；A/B在c前，s/u/t及Rz在χ前，LR在fold前。挑战由可信交互V独立抽样，SHA仅作前缀记录，不生成挑战。每个(label,prefix,coin)保存于trusted VerifierCoins。离线replay同时验证prefix和方程，必须使用V自己的receipt；不能接收prover自带receipt作为NIZK权威。

canonical JSON/hex、research magic、1MiB cap，N≤1024/K≤4、R/C≤32；规范Fr、curve、q-subgroup检查。函数位置与SHA在evidence/CODE_INDEX.json。curve/entropy/protocol完全独立，没有导入生产模块。

新增假设：真实群参数/独立基、理想随机币、DL-relation hardness、可信交互verifier边界。OS熵、variable-time Python、自写曲线实现不构成生产认证。没有FS或全GKR/Lasso组合定理。
## 原始文献与确切适用性

[Hyrax extended 29页全文](https://eprint.iacr.asia/2017/1132.pdf)，本地literature/hyrax-mirror-0.pdf，SHA256 6440b28eb3d692b144772c660c42607c5ef158306a33691a03921925a8d48296。已读所需完整§5（PDF7–8页）、§6.1（9页）、A.2（18–19页）、A.3（19–20页）、A.4（20–21页）、A.5/A.6（21页）及整体证明（21–25页）。作者conference版可由[作者站](https://www.cs.nyu.edu/~mwalfish/papers/hyrax-oakland18.pdf)交叉核对。官方ePrint下载403，因此extended使用先前保存镜像；不宣称镜像bytes已与官方当前版认证一致。

[Bulletproofs作者45页完整版本](https://web.stanford.edu/~buenz/pubs/bulletproofs.pdf)，SHA256 f7b6c9297bdd5b5705b3a4866f1f0dcd9d236517642265854667e6794619e4bc。已读相关定义（8–12页）、完整Protocol1/2与验证（12–17页）、AppA通用forking和AppB完整提取证明（38–41页）。本版Thm1/AppB对应Hyrax引用的较早版Thm2/AppA，不混用编号。裸IPA不是ZK。

Hyrax A.2 Figure6提供masked-response Sigma关系；A.3 Figure7/8是另一紧凑协议，终端有其特定公共权重方程；§6.1/A.5提供row compression与多row提取思路。本文选择A.2响应压缩并独立证明公开权重IPA及新增audit，不套用A.3 Lemma12的n+2提取数。Hyrax §5/A.4的coeff commitment只作未来接口参考，没有实现sumcheck。整体Theorem15替换input commitments的hybrid没有用于固定PC模拟。
