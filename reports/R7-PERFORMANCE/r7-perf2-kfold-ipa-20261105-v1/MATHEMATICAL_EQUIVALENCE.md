# 有限域精确等价

令 chi_j(a)=prod_t (j_t ? a_t : 1-a_t)，K_z=sum_i rho_i sum_{j:map_i(j)=z} chi_j(a_i)。原终点为 sum_z K_z chi_z(r)。rho、Claim、canonical map、Root 和 r 均沿用原协议产生顺序；只在最终 r 已知时收缩。

若 j0,z0 均按 2^b 对齐，连续块恒等式为：

sum_{u<2^b} chi_{j0+u}(a) chi_{z0+u}(r)
= high(a,j0,b) high(r,z0,b) prod_{t<b}((1-a_t)(1-r_t)+a_t r_t)。

重复目标块为 high(a,j0,b) chi_z(r)，因为低位基之和为1，重复映射贡献并未去重。全部运算不使用逆元，因此零、一及任意非布尔挑战均成立。不同 Claim 和快/稠密分块按有限域线性相加。dense 的活动域外系数是公开零值，而不是关于隐藏物理 Source 尾部的假设；原 Source 零尾约束和 opening 仍存在。

测试先以参考 stub 验证“连续 Run 不再展开全部坐标”的断言确实失败，再实施候选。perf2_k_tests 比较：各维度 dyadic block 的字面和；n=64/128/1024/4096、6 Roots、连续/stride3/逆序/重复/置换、四个单独 K_i 及混合 rho；0/1/-3 终点；n<=128 的全部布尔基查询逐项恢复每个 K 系数；n64 每轮的每个折叠坐标；缓存耗尽原回调路径及变异回调 digest 拒绝。不是仅比较哈希。测试输出 PERF2_KI_AGGREGATE_EVERY_FOLD_AND_FALLBACK_PASS 已在最终完整 CTest 日志核实。

独立完整有界 Single 协议使用 n=524288、同一测试随机源和固定测试 nonce，原库与新库生成的 proof 字节完全相同，四种新旧 P/V 交叉组合均成功且严格 EOF。生产随机源、FS Transcript、A.3、IPA、Root/coverage、Statement/public X 绑定无实现修改。只读审查未发现阻断数学/协议问题；安全状态仍保持 FS_NIZK_CANDIDATE / BLOCKED_CRYPTOGRAPHIC_DESIGN。
