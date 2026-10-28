# R7_H_MULTI_OPENING_ANALYSIS

日期：2026-10-23（Asia/Shanghai）。Stage1 Task0-B，suite H-PC-CSIPA-AUDIT-RESEARCH-v1。仅新增 reports/R7-STAGE1-TASK0B-20261023-v1；全部源码编辑、测试、测量在固定cwd /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work 执行。N≤1024、1≤batch K≤4、每MSM≤32 bases。没有lookup table，因而没有超过64项的表；没有读取真实120参数、R6 proof或真实W anchor，没有真实注册、生产verifier、oneblock/full12或生产ZK。

冻结源码575项重新SHA核对；真实参数/proof/anchor仅记录前后stat元数据、复用既有SHA身份，不为验证冻结而读取内容。原may_enter_stage2=false不变。以下为可人工复核的数学证明，非机器形式化或生产认证。

## 1. 结论范围

以下独立证明公开权重IPA、masked核心、row audit及同PC多点知识；适用于理想交互、独立未知DL基、K≤4研究relation。手算差商/Vandermonde/递归toy树仅为辅助。生产893开点、任意并发/FS没有从本结果自动获得知识定理。

## 2. 公开权重IPA的表示提取：三叉归纳

固定χ，置h_j=Gj+χa_jG，fold后h'=f⁻¹h_L+fh_R。C=1时终端z_final直接给U=z_final*h_final表示，不要求a_final或h_final非零。

归纳步：父U及Lcross/Rcross已固定。取得三个f_k≠0、s_k=f_k²互异，各子extractor给v^(k)，满足：

    U+s_kLcross+s_k⁻¹Rcross
      =Σ_j f_k⁻¹v_j^(k)h_L,j+Σ_j f_kv_j^(k)h_R,j.

矩阵rows为(1,s,s⁻¹)。各列乘s，成为Vandermonde矩阵rows的排列，故非零且可逆。可取唯一λ使Σλ=1、Σλs=Σλs⁻¹=0。三式线性组合后：

    U=Σ_j z_L,j h_L,j+Σ_j z_R,j h_R,j
    z_L,j=Σ_k λ_k f_k⁻¹ v_j^(k)
    z_R,j=Σ_k λ_k f_k v_j^(k).

这是精确group线性代数，不使用binding，不除a_final。叶数3^log2C≤C²。原双秘密向量Bulletproofs的4叉提取不是本适配必须照搬的复杂度。

## 3. 两χ把表示变成真实response开口

在相同Rz,a,t下，对χ0≠χ1提取z0,z1：

    Rz=Σz_b,j Gj+χ_b(<a,z_b>−t)G.

相减得：

    Σ(z0−z1)_j Gj
      +[χ0(<a,z0>−t)−χ1(<a,z1>−t)]G=0.

非平凡系数直接给DL关系；否则z0=z1=z，χ不同迫使<a,z>=t，于是Rz=ΣzGj。G若复用G0，prover可在χ后改z0来适应任意t，推论失效，因此独立G为必要条件。

## 4. 两c提取精确核心witness

在相同A/B下，两c得到已认证z_b及wire s_b/u_b/t_b。Δc=c0−c1：

    x=(z0−z1)/Δc, ρ=(s0−s1)/Δc
    y=(t0−t1)/Δc, η=(u0−u1)/Δc.

由Rz_b=c_bξ+A−s_bH=Σz_bGj，相减得ξ=ΣxGj+ρH。由c_bY+B=t_bG+u_bH相减得Y=yG+ηH；t_b=<a,z_b>给y=<a,x>。唯一除数是不同挑战之差。core完整fork树≤4·3^log2C叶；或输出DL关系。

## 5. Vandermonde row audit与完整W

固定所有Ai、全部batch points/Y后，取得R个互异b_k，各audit提取(x^(k),ρ^(k))：

    Σ_i b_k^i Ai=Σ_j x_j^(k)Gj+ρ^(k)H.

V_ki=b_k^i可逆，定义W_ij=Σ_k(V⁻¹)_ik x_j^(k)、ρ_i=Σ_k(V⁻¹)_ikρ^(k)。等式左乘V⁻¹，逐行得Ai=ΣW_ijGj+ρ_iH，恢复完整同一PC开口。不是要求一个固定MLE点自然给出R个独立rowweights。

## 6. 多点、重复点与sameY

任一evaluation提取(x_l,ρ_l,y_l,η_l)。audit的W给x*_l,j=Σ_iL_l,iW_ij、ρ*_l=Σ_iL_l,iρ_i，也开同一ξ_l。不同开口相减给Gj/H非平凡关系，否则相同，故y_l=MLE(W,r_l)。

同一完整Y的两个开口满足(y_l−y_m)G+(η_l−η_m)H=0；不同则给G/H关系，否则两y相等。different points只在真实evaluation相等时允许sameY。重复point可用sameY或freshY，evaluation一致。字符串label相同不等于完整handle相同。

这些是计算性binding结论，不是信息论唯一性；Pedersen拥有许多信息论表示，证明/提取和未知DL假设排除恶意已知表示的自由切换。

## 7. 有界batch定理、完整树和knowledge error

使用Hyrax extended A.6 Lemma13所述GSS→witness-extended-emulation条件：从合适accepting tree能多项式时间提取、失败至多negligible，且叶数对安全参数为多项式。

本protocol树：audit b为R叉，每core c/χ各2叉，每fold为3个不同平方类；顺序audit及K个evaluation的保守总叶数：

    m=log2C, T=R·(4·3^m)^(K+1).

按§2–6从该树恢复relation witness或非平凡DL关系。K≤4是明确限制，故T对R/C为多项式，满足引理的规模前提；这是有界精确交互knowledge/WEE依据，而不是原A.3 n+2 theorem。

可给保守knowledge-error阈值。自底向上剪去不足所需children的节点：b若不足R叉，至多R−1个coin；c/χ若不足2叉，至多1个；fold不足3个平方类时至多4个非零coin。逐深度union bound：

    κ ≤ [(R−1)+(K+1)(2+4m)]/(q−1).

若成功概率高于该阈值且间隙非negligible，可按GSS forking引理取得所需树；抽取时间依赖T和成功间隙。DL reduction/生成元假设/FS不在该分子内，不将此数称为R7安全位数。

extractor若随机寻找children还需处理重复挑战和平方碰撞，每pair平方相等≤2/(q−1)；至多L个coin用2L²/(q−1)粗界，可重采样。完整树节点粗界L≤2T+1。此为抽取器budget，不是诚实a_final=0 abort率。

N=1024、R=C=32：K=2时T=29,386,561,536，κ≤97/(q−1)；K=4时T=27,763,953,154,228,224，κ≤141/(q−1)。

没有执行如此大的完整rewinding tree。理论expected-polynomial不等于现实高效，具体DL归约损失/抽取运行量仍未达生产可审查预算。K一般增长时此保守树对K指数，绝不能套到893个opening。必须另有更强联合提取/组合定理。

## 8. 状态划分

- 公开权重表示/内积提取、两c核心、Vandermonde audit：PROVEN_FOR_EXACT_INTERACTIVE_RELATION，依赖以上明确参数/挑战/DL条件。
- K≤4 samePC/sameY一致性与bounded knowledge：PROVEN_UNDER_DL_RELATION_AND_GSS_FORKING；非机器形式化，非实际完整extractor实现。
- test_helpers中的差商/2×2Vandermonde/C=4三叉递归：TOY_ALGEBRA_ONLY，不替代一般证明。
- 893开点、无限制多session/并发高效提取、FS及R7组合：PARTIAL/BLOCKED_FOR_PRODUCTION。

新增audit及response compression均已在本文推导，不把原文不同协议定理未经条件检查拼接。
## 原始文献与确切适用性

[Hyrax extended 29页全文](https://eprint.iacr.asia/2017/1132.pdf)，本地literature/hyrax-mirror-0.pdf，SHA256 6440b28eb3d692b144772c660c42607c5ef158306a33691a03921925a8d48296。已读所需完整§5（PDF7–8页）、§6.1（9页）、A.2（18–19页）、A.3（19–20页）、A.4（20–21页）、A.5/A.6（21页）及整体证明（21–25页）。作者conference版可由[作者站](https://www.cs.nyu.edu/~mwalfish/papers/hyrax-oakland18.pdf)交叉核对。官方ePrint下载403，因此extended使用先前保存镜像；不宣称镜像bytes已与官方当前版认证一致。

[Bulletproofs作者45页完整版本](https://web.stanford.edu/~buenz/pubs/bulletproofs.pdf)，SHA256 f7b6c9297bdd5b5705b3a4866f1f0dcd9d236517642265854667e6794619e4bc。已读相关定义（8–12页）、完整Protocol1/2与验证（12–17页）、AppA通用forking和AppB完整提取证明（38–41页）。本版Thm1/AppB对应Hyrax引用的较早版Thm2/AppA，不混用编号。裸IPA不是ZK。

Hyrax A.2 Figure6提供masked-response Sigma关系；A.3 Figure7/8是另一紧凑协议，终端有其特定公共权重方程；§6.1/A.5提供row compression与多row提取思路。本文选择A.2响应压缩并独立证明公开权重IPA及新增audit，不套用A.3 Lemma12的n+2提取数。Hyrax §5/A.4的coeff commitment只作未来接口参考，没有实现sumcheck。整体Theorem15替换input commitments的hybrid没有用于固定PC模拟。
