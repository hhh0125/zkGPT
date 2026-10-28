# P0-A 群验证安全边界

只适用于当前冻结 MCL BN254 **G1**。这是 MCL 的 b=2 曲线，不是 Ethereum BN_SNARK1 的 b=3 曲线。运行时逐次检查 curve ID、p、Fr 模数、G1 order、a=0、b=2、specialA=Zero、specialB=GenericB、Jacobi 模式、非 ETH 序列化和 32 字节长度。任一不匹配即保留旧式 `isValid() && isValidOrder()` 路径。未改变 MCL verifyOrder 标志、断言或编译检查。

u=-4647714815446351873；p=16798108731015832284940804142231733909889187121439069848933715426072753864723；r=16798108731015832284940804142231733909759579603404752749028378864165570215949。

E: y²=x³+2 over Fp。BN 参数给出 p=36u⁴+36u³+24u²+6u+1，r=36u⁴+36u³+18u²+6u+1，trace=6u²+1，#E(Fp)=p+1-trace=r，cofactor=1。另以固定非零点 P=(-1,1) 的未约减 generic [r]P=O 作独立运行检查。给定已公布的素数 p/r，Lagrange 定理得 r | #E，Hasse 区间内唯一 r 的正倍数为 r，故 #E=r。这避免只凭曲线名称或标量域相等推断 cofactor。CURVE_ARITHMETIC.json 给出整数检查；SymPy 的 >64-bit primality 检查只是对公布素数的交叉检查，不宣称生成了形式化素性证书。参数来源见冻结 `3rd/mcl/include/mcl/curve_type.h`、`bn.hpp`，以及 [MCL API](https://github.com/herumi/mcl/blob/master/api.md)。

因此，对**当前参数下字段合法且在 E 上**的 G1 对象，r 倍为无穷远已由曲线群阶蕴含；独立 order 乘法不再增加判定信息。有效点的加、减、标量乘和 MSM 结果仍在群内。实现比单纯信任闭包更保守：每次 encode、shape、生成元检查都重查当前对象的 x/y/z 字段合法性和 MCL 原 Jacobi 曲线方程。不存在对象地址缓存、validated 标志、跨角色授权或“只检查一次”的不可变性假设；对象变化、释放后地址复用都重新检查。仅 p/r 数学常量静态保存，曲线状态每次读取。

外部字节必须长度准确、严格字段坐标范围、平方根存在及正确 parity、on-curve、规范 encode roundtrip。无穷远使用原全零编码；无效 Jacobian 无穷远拒绝；可合法表示同一无穷远的非标准 Jacobian 与原编码一致。禁止非规范坐标、trailing/truncated 字节及破坏的群点。当前 G1 cofactor1 使外部输入的 subgroup 条件同样由 on-curve 蕴含；不是把外部输入当可信内部对象。

G2 未修改：测试实际找到 on-curve 非 subgroup G2 并保留拒绝。BN_SNARK1 使用旧严格 order 路径；当前 256-bit 构建拒绝 BLS12_381 初始化。不得把本结论推广到任意 G2/BLS12 或其他 b/p/r 配置。MCL 是全局曲线配置；仍要求已有工程前提：线程启动前完成初始化，不支持并发重初始化或任意写坏底层 MCL 运算表。无新增缓存的并发失效问题，也不宣称对全局状态数据竞争提供保护。

PCSpec、role、尺寸、生成元非零、Sumcheck、Lasso、IPA/opening、W 绑定和独立验证均未删减。一个“在曲线上但内容错误”的承诺仍须通过原协议关系验证；通过群元素合法性不等于通过证明。
