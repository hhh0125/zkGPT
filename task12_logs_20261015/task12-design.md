# Task 12 设计与实施边界

目标：在现有 GKR、Range/Field LogUp、Exp pairing、Sparse 和 Model Binding 路径补齐 ZK，不扩展 Task11 full-vector O(n) 响应。用户任务为完整规范；本文件是工程与协议适配选择，不是既成安全认证。相关审计见同目录4份 audit/map/DAG。

## 选择

1. 保留 MCL BN254、C++17、真实生产 Sumcheck 内核和 Hyrax 行列折叠。
2. 独立新 protocol version/ZK mode，旧 strict-v4 继续精确回归；不重新实现 toy Lookup。
3. 以 [69] separable mask 隐藏轮消息。隐藏初始 claim 时不公开 mask 总和 G，改为 D_G；通过常数维承诺等式验证公开 masked total。终端保留秘密原始 MLE 的 D，而不是公开 Fr。这是本次适配，必须给出自己的模拟和提取论证。
4. Scalar commitment `D(v,r)=v*U+r*H`，固定独立未知离散对数关系的 U/H。同一逻辑 claim 直接共享 D。所有盲因子独立均匀；线性组合的盲因子按同态计算。
5. Hyrax 每行 `C_i=Σ_j w[i,j]G_j+rho_i H`。公开 MLE 点把行聚合成长度 c 的向量和盲因子；用共同 witness 的 Sigma 同时证明该聚合 C 和隐藏 D_y。响应长度 c≈sqrt(n)，不是 n。禁止调用 Task11 的 n2/4 API 偷改维度限制；其代码保留原样，代数仅作参考。新向量接口只被行折叠后的 opening 调用，wire 限长由声明尺寸计算。
6. 秘密乘积只在 Sumcheck terminal 证明，不能逐元素增加76032个证明。公共常数乘法同态处理。

## Scalar 关系具体定义

等式：对 `E=Σ alpha_i D_i - public_value*U` 证明知道 r 使 E=rH。Sigma 首消息 A=tH，challenge e，response z=t+er；检查 zH=A+eE。alpha/全部 D/公开值/上下文先绑定；该证据与承诺计算绑定组合才约束值相等。

乘积：公开 Cx/Cy/Cz，秘密 x,rx,y,ry,z,rz，z=xy。取独立 a,b,c；A=aU+bH，B=aCy+cH；挑战e；sx=a+ex，sr=b+erx，st=c+e(rz-x*ry)。检查 `sx U+sr H=A+e Cx` 和 `sx Cy+st H=B+e Cz`。双 transcript 提取 x/rx 及 Cz=xCy+delta H，结合 Cy/Cz 开口绑定获得乘积；不能只声称存在开口。模拟器均匀选 sx/sr/st 并反算 A/B；FS 在可编程经典 ROM 下，需新鲜首消息和规范绑定。

## Masked Sumcheck

mask 有1+dL个系数，对其逐系数 scalar hiding commitment，可同态算 D_G 和任意 D_g(r)。在 rho 前固定。初始 `T=H+rho*G` 公开，由等式证明连接 `D_H+rho D_G`；每轮发送真实生产 round 加 rho*mask_round。Verifier 按现有递推检查 masked 轮；终端使用隐藏 D_F 和 D_g(r) 验证 `T_final=F(r)+rho*g(r)`。degree3 的 D_F 通过 eq(r0,r)*D_product 连接隐藏 f/g endpoints。degree1 的 D_F 就是同一 terminal D。Mask 系数 commitment 数 O(dL)，线性关系计算 O(dL)；每轮不发送 witness 向量。

安全目标不是不打印秘密，而是联合模拟：mask commitments 用 hiding 混合替换，masked rounds 在一致性约束下模拟，terminal Sigma 与隐藏 opening 联合模拟。必须检查条件分布、退化零系数/eq、rho非零、L=0、重复证明、shared claims 和先后时序。未完成该论证前只能报告 correctness，不报告 ZK PASS。

## 生产接入

首批改 range_sumcheck.hpp/.cpp 的可控新模式，共用真实 degree1/3折叠循环；后续 GKRProofBuilder 的 phase1/phase2、双 FCONN 和 Lasso 必须都走同一轮适配器，verifyGKR 使用隐藏线性/乘积终端。测试使用 tests/gkr_test_fixture，而非另一套 circuit evaluator。

FieldLogUp 保留 q/T/m/a/b 和四个 Sumcheck；两个 sum 共享 D_S；表一侧公开 T，可同态 beta+T~；q/reciprocal 侧隐藏乘积。Exp 保留8列/6 membership/2 normalization/12 opening/2 Sparse结构。Range 同步迁移。隐藏 Sparse 每 pattern 的 D 与总 D 满足同态和，并用同一原 val0 行承诺的聚合 opening；pattern 个数与 layout公开，禁止旧 IPA fallback。

Model Binding 必须迁移 mixed-row IPA 和全行比较的盲因子关系；确定性模型标识保留在公开声明边界，不能声称可撤销既有候选模型识别。新的隐藏 val0 必须连接 trusted model anchor 的实际参数，不仅 hash 相同。

## 验证门槛与资源

按用户 Phase B(2/4/8变量)、C(多层真实GKR)、D(8/16/32/64查询)、E(256/1024/4096/76032)顺序；不得用后面的单组件 benchmark 冒充前面集成完成。完整 GPT proof 仅在所有小规模独立拒绝、组合论证和内存门槛通过后执行。每项记录 prove/verify、真实 codec bytes、RSS、运算统计范围。所有新日志/构建在 task12_logs_20261015，现有源码修改先 hash 比对，记录精确 delta，不 reset/stash/commit；本任务日志不混入旧目录。

## 尚须闭合的安全问题

设计审阅后的必需补充：mask 的每个 coefficient commitment 在 rho 前增加共享挑战的 Schnorr representation PoK，O(dL) 通信。Hyrax 双 transcript 仅提取一个行聚合，完整向量知识需对足够多独立行权重的嵌套 rewind/interpolation 论证及接受概率分析，不能误称一次提取整个矩阵。每个乘积 Cy 的 opening provenance 必须来自前序提取或已知常数，GKR 自由终端需额外 representation PoK。确定性模型 anchor 必须直接由参数算未加盲承诺，不能复制本次隐藏 val0 行；全行差转为知道纯 H blinder 的证明。L=0 直接连接同一常数mask；eq=0不用除法。

新的 mask-hiding 适配的联合模拟/提取；Hyrax 行承诺到所有坐标的 evaluation-binding/knowledge soundness；多层双父点及共享 claims 的联合分布；Sparse 多 pattern 通信随布局的最坏增长；Model anchor 迁移；FS 组合论证。这些是实现与审阅必须解决的工作，不是可以省略的免责声明。
