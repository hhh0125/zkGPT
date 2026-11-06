# 负向验证与兼容性

候选启用版本的完整 CTest 101/101；默认关闭版本重新完整 CTest 101/101，定向11/11。完整逐用例输出在恢复目录 build/.../Testing/Temporary/LastTest.log，命令/退出码与阶段原始日志在 reports/...。正常拒绝通过 C++ 异常和测试期望检查，不把崩溃/超时当成拒绝。

| 要求 | 实际验证路径与证据 |
|---|---|
| Statement/public X/Claim/Root | unified_input_tests.cpp mappedProtocolTests 改 root commitment、Claim handle、completion、map/bias、public X 和零尾响应；Single 集成测试含恶意 map/Y 的实际代数关系拒绝 |
| Challenge | hyrax_a3_tests.cpp 更换挑战后实际 Schnorr 方程拒绝，零挑战 fail closed；oneblock_bindings 旧聚合挑战被拒绝 |
| K系数/终点 | unified_input_tests.cpp mode4 对 V 实际K终点加1，调用 ui::reduce 后拒绝；新 perf2_k_tests 逐项 K_i/聚合/每轮坐标等价并拒绝变异回调。未新增单独序列化K攻击格式 |
| IPA response/A.3 guard | hyrax_a3_tests.cpp cross/blind/vector-guard/scalar-guard 和 xi/tau抵消伪造，经过实际知识方程/最终Schnorr拒绝；有界proof中间字节变异触发 A3 vector knowledge equation failed |
| 截断/追加 | 独立候选V与最终默认V均exit1：FS truncated proof or file IO failure；FS appended buffered data rejected |
| 缓存身份/Session | opt3c/opt3d PublicK foreign session、wrong endpoint 拒绝；映射预算耗尽及变异回调拒绝；FS会话/Statement/context绑定回归在完整CTest中保留 |

独立文件负向三项均正常exit1（非信号/断言/超时）；有效proof新旧V四向均exit0并完成EOF。它们不能代替理论密码学设计审查。未运行真实Oneblock/Full12恶意证明任务。
