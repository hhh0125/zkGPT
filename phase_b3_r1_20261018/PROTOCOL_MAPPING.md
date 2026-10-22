# 论文步骤、数学关系与实现

本轮实现是关系基础库，完整目标未验收。Z指[USENIX正式zkGPT](https://www.usenix.org/system/files/usenixsecurity25-qu-zkgpt.pdf)，L指[Lasso ePrint 2023/1216](https://eprint.iacr.org/2023/1216)。正式Z字节SHA见SPEC；上游commit/tree/blob的核验见UPSTREAM_VERIFIED.json。L当前PDF请求403，但已重新从指定服务器取得原保存PDF并重新提取59页，SHA256为`c4a6d7d8650d4706d221b400f6d34dbd8aeff69ffeaed34e809536ffd6e563ae`；提取文本与上轮同字节。正式Z20页与缓存Z19页都重新提取，完整Protocol1去空白后相同，见PDF_IDENTITIES.json和PROTOCOL1_VERSION_CHECK.json。没有把缓存文件说成重新从ePrint下载。

## 公共对象与承诺

有限域固定为MCL BN254的Fr，模数为
`16798108731015832284940804142231733909759579603404752749028378864165570215949`。
公开X、模型W、aux、L各补齐到N。W必须来自Verifier事先选择的发布方承诺，不能从Prover附件自动建立信任。代码InputSpec中的三个私有块依次是W/aux/L；x保存完整公开补齐向量。当前只有合成fixture，没有为真实参数生成新的compact-W anchor。

对长度p=2^d向量，R=2^floor(d/2)、C=p/R。
column-major: index(r,c)=r+cR；row-major: index(r,c)=rC+c。
每行承诺C_r=sum_c V[index(r,c)]G_c。PC身份包括scheme/field/curve/role/生成元算法/actual/padded/R/C/layout和有序行点；actual不自行蕴含零尾。G/U由独立domain标签导出。

MLE用低位先行的Boolean基函数chi_u(i)。column-major把u低floor(d/2)位给行，row-major把低ceil(d/2)位给列。行权重折叠承诺和witness后，IPA认证v=<collapsed,chi_column>及其承诺。Verifier的expectedSpec来自调用方。对应pc.cpp的commit/open/verify；tests_core覆盖两种布局、奇偶维、错误witness/点/值/承诺/角色。

## 对应表

| Z步骤/关系 | 新代码与Prover行为 | Verifier接受条件与测试 | 限制 |
|---|---|---|---|
| KeyGen/预处理模型承诺 | core.cpp初始化明确曲线；pc.cpp独立生成G/U，按上述矩阵提交 | 曲线/field/shape/expectedSpec一致；IPA方程成立 | 非隐藏PC；真实模型anchor未生成 |
| (1)计算X/W/aux/L及公开完整Y | InputSpec、GkrSpec、BundleStatement | 在首个挑战前吸收所有公开对象及承诺；Y全部进入根声明 | 合成算术计算已做；真实GPT算子生成未接入 |
| (2)(3)Range Lasso | range.cpp把b<=126拆为最多9-bit digits，末段使用真实位宽；承诺Q及D | Q~(r)=sum_h 2^shift D_h~(r)，且每个D同PC通过精确表memory；Q尾部为0 | 不预设81/87/88实例；真实L1打包/模型映射未完成 |
| (2)(3)indexed memory | memory.cpp承诺read/final并固定a/E；product tree归约 | prod(I)prod(W)=prod(R)prod(F)，3个产品的leaf分别连接同一a/E/read/final PC | 完整公开表、c=1实例；structured Range为多个小表加法组合 |
| (2)(3)Exp pair | exp.cpp认证原始t/E、branch和native padding；真实E表由有理区间生成 | t为u32，s为bit，x/D区间、两条degree3关系、E_native绑定及indexed memory全部成立 | 明确的新数值实例；不是旧clamp函数或完整GPT profile |
| (4)输出声明 | gkr.cpp绑定整个公开output后采样输出点 | 根MLE由公开向量本地计算；不能由proof自由报告根值 | 当前末层全部公开；真实Y/零检查层布局待生成器定义 |
| (5)一般算术层 | 稀疏gate-wiring GKR，先u再v逐轮degree2消息 | 每轮递推、终端wiring与两输入声明成立；挑战混合后进入下一层 | field算术backend已实现；真实电路/融合优化未认证 |
| (5)矩阵层 | matrix.cpp折叠输出行列，inner sumcheck | C~(col,row)=sum_k A~(k,row)B~(col,k)，最终打开原A/B PC；矩形padding另证 | 无论文grouping加速；未装入模型FCONN调度 |
| §4.3秘密分母取整 | division.cpp固定原始A/B/q PC，提交Aenc/qenc/Bminus1/S/T | B正且有界、signed A/q、S/T范围、两条degree3 sumcheck及同PC终端全部成立 | 已有独立codec/Verifier；尚未生成Softmax/LN的全图来源声明 |
| §4.3正域sqrt | positive_sqrt.cpp提交y−active、S、T并Range认证x | y²−y+1<=x<=y²+y；degree3约束最终打开原x/y PC；零尾认证 | 明确x,y>=1域；不是擅自扩展零域或完整LN |
| (6)统一输入一致性 | input.cpp把GKR/Range/indexed声明随机线性组合，先local坐标再2个selector坐标 | 所有round闭合到同一点的W/aux/L opening；X本地求值；三个私有尾部逐项随机检验 | 通用四块实现；尚未生成全模型所有查询的可信列表 |
| (7)最终PC | 三个terminal opening，包含L | in~(u,b,a)=(1-a)(1-b)X~+(1-a)bW~+a(1-b)aux~+abL~ | 这是本项目可验证代数修正，不是作者勘误；原印刷L系数重复X且漏显式L opening |

## Lasso memory 到目标查询的连接

T为完整公开2^k表。每个真实地址a_i定义唯一unit-row M_{i,a_i}=1，memory证明E_i=T[a_i]，因此MT=E。fingerprint为gamma^2 a+gamma v+c−tau，读时间c、写时间c+1；初始时间0。全部相关承诺固定后才取gamma/tau。

每个产品的内部层验证F_l(r)=sum_z eq(r,z)F_{l+1}(0,z)F_{l+1}(1,z)，degree3。两子声明固定后采selector，继续到leaf；leaf没有自由oracle。零产品无需求逆，测试覆盖。read计数在field内；Lasso的计数链论证要求reads数小于field特征，本实现上限满足，不额外假称每个计数已经range-proven。

Range的T_h[j]=j，每个digit PC同时作为地址和值。区间与sum2^shift重构在2^126<q下不回绕。signed关系是Q=V+2^(b−1)active；V/Q都补零时不能加全域常量。原始t/E与GKR不能只靠hash关联：Bundle使用同一私有块PC，且把已验证Range和indexed行声明加入最终输入sumcheck。

## 两个精确整数关系

integer_relations.cpp的rounding先证明A/q各在signed<=64-bit域，再用公开0<B<=2^62认证S=2A−2Bq+B active、T=(2B−1)active−S。S/T非负且其和为2B−1，故q−1/2<=A/B<q+1/2。两端不同时闭合；相关整数残差至多约2^128，远小于q，排除mod alias。source/helper用同一PC的随机点评估绑定。

最大值先证明score/m的signed域、D=m active−score非负b-bit。再证明product_i(D_i+inactive_i)=0，最终leaf打开同一个D PC；padding叶为1。非空active域中必有一项D=0，因此m是真正最大值。全负score不再默认max(0,score)。这些关系模块已测试，但尚未替换旧模型的对应gates。

## 秘密分母和正域平方根

division.cpp逐坐标证明A/q为signed<=64-bit，B−active为b<=62-bit非负数，故active域1<=B<=2^b。辅助S=2A−2Bq+B，T=2B−1−S均为(b+1)-bit非负数，推出−B<=2A−2Bq<B，即q−1/2<=A/B<q+1/2。B*q是逐坐标乘积：承诺后取随机eq权重，degree3 sumcheck归约到原始A/B/q、同S/T PC的五个opening。B−active的Range及随机仿射绑定确保B正性和原始B零尾。残差保守<2^128，远小于Fr。

positive_sqrt.cpp明确使用上游LN的正域：原始neuralNetwork.cpp:505–527初始化B=1，取sigma=round(sqrt(B))并assert sigma!=0。Z §4.3印刷(2y−1)^2<=4x<=(2y+1)^2−1，对整数y>=1精确等价于y²−y+1<=x<=y²+y。S=x−y²+y−active、T=y²+y−x非负认证这两个边界；相邻y区间连续且互不相交。x unsigned<=64-bit，y−active unsigned<=63-bit，S/T各root_bits+1 bits；最大y²=2^126，残差<2^128，无field alias。源与helper均在挑战前固定，degree3 sumcheck包含逐坐标y²而非错误MLE平方。零输入不在此模块域中，未冒充论文正式零域公式。

两者具独立文件格式kind5/6、7/8及`--division`、`--positive-sqrt`入口；测试覆盖正负tie、秘密B端点、最大u64输入、错邻root、padding、篡改和跨进程。没有把局部source PC声明冒充完整Softmax分母计算或LN variance来源已认证。

## 无clamp Exp的等价推导

公开数值实例为E(t)=round(2^20 exp(−t/512)), t∈u32，K=8191。精确有理证书证明T[K]=0；单调性给出所有t>=K均为0。用s=[t<K]、x=min(t,K)、D=s(K−1−t)+(1−s)(t−K)；s bit和D u32唯一确定分支，相关差值小于2^33，不可能域回绕。

active域上的两条二次关系在eq加权degree3 sumcheck中认证，而非错误地用MLE乘积代替乘积向量的MLE。helper x/s/D及t的native尾部为0，故lookup padding应为(0,T[0])。另提交E_native并证明E_native~=E~+2^20·inactive~，把真实公开/私有E的零尾与合法native查询连接起来。L2仍是原始(t,E)，x只作为经证明等价的压缩地址。

证书：u=t/16384<=1/2，P33(u)<=exp(−u)<=P32(u)且端点正。升32次方并乘2^20后，两端在全部8192项上取整一致。首零7453；u32尾由单调性覆盖。代码及优化模式测试均显式检查错误，不依赖Python assert。

## Transcript顺序

Bundle：完整公开statement/模型PC/所有块PC/电路/Y/表 → Range helper承诺及row挑战 → Range openings/memory及零尾 → indexed row/opening及memory → GKR输出点/逐层消息/混合挑战 → 所有终端声明 → batch权重 → local/selector sumcheck → W/aux/L opening → 各私有块零尾证明。

Exp独立格式：固定实例/表digest/源与全部helper承诺 → Range证明 → 两条branch sumcheck及同PC终端 → original/native value binding → 同地址indexed memory。每个sumcheck/IPA均先吸收消息再采挑战。

Division独立格式：固定id/位宽/原始A/B/q与全部helper承诺 → helper Range → A/q signed绑定 → B正域仿射绑定 → 两条eq-weighted sumcheck与各五个源/helper终端opening。PositiveSqrt独立格式：固定id/位宽/原始x/y与全部helper承诺 → x及helper Range → y正域绑定 → 两条eq-weighted sumcheck与各三个源/helper终端opening。各格式使用互异FS domain；proof内的PC身份不能覆盖可信statement。

Fs以SHA256作framed hash chain；长度和标签明确。32-byte候选只有在canonical Fr编码被接受且往返一致时使用；无mod reduction。sumcheck可取0，IPA采F*。整个组合的ROM/提取依据见SECURITY_OBLIGATIONS.md，不能由trace相同推成安全证明。
