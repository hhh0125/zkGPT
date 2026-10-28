## 实现及等价边界

仅三个源码文件变化：mapped_input.cpp、独立差分测试、CMake测试注册。单点以 child/master Equality 权重乘积求值；2..16长度直接累加；较长区间最多32个共同对齐的dyadic blocks；奇偶错位、超出32块或私有无效范围走原 DP。原DP函数仅改名，函数体逐字节不变。

对共同按2^k对齐的区间，低k位完整遍历，求和为 `product_{j<k}((1-c_j)(1-r_j)+c_j*r_j)`；高位固定，乘以双方相应Equality因子。低/高位可分离，因此与原域内逐项求和严格相等。无除法，0/1坐标和消失因子均适用；有效范围限制保证k不超过双方维度。重复索引继续独立累加，bias在原batch处理；zero-tail与最终等式原样保留。

缓存不是全局映射：每次kernelValue拥有master点快照；每个当前child/claim拥有子点快照，当前对象结束即销毁。逻辑键是完整有序Fr坐标和维度，索引只作用于该不可变快照；没有地址键、跨调用/跨claim/跨角色持久状态。维度上限28，与原有效PC域边界一致；两个split表对象容量合计最多2MiB，另有O(d)坐标/低位pair向量及构造临时分配。最终测试验证同地址外部点突变、快照所有权、并发独立调用和实际2MiB表边界。

所有51 terminal、FCONN双点、spans顺序、lambda累加、bias、padding、零尾、最终等式及Fiat–Shamir绑定保留。Batch、dense prover kernel、tail、prove/verify suffix均做逐字节区段核对。R2/GKR/Range/Exp/Lasso、编码、Profile、参数、X/Y/W锚、P0-A群校验文件均未改变。正常小fixture proof/statement逐字节一致；真实模型产物兼容性仍须oneblock验收。

