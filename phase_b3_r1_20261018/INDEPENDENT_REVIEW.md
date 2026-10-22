# R1 独立审查记录

本记录汇总实际独立agent上下文的只读审查，不是第三方密码学审计或完整安全证明。最初设计审查和代码审查分离。审查未编辑源码、未运行远程C++构建；代码审查另行运行了指数表的只读有理证书校验。远程测试由主执行者完成。

## 设计审查

确认PC/MLE、Lasso完整表memory、product tree、partial-digit Range可独立实施。提出：尾部必须随机加权；signed bias必须乘active mask；Protocol1(7)可采用显式代数修正但不能称作者勘误；memory属于c=1完整表Lasso/Surge实例，需证明M·T=E及连接原始查询。上述条件已纳入SPEC和实现。

## 主要代码审查及实际修复

初审发现1个Important：Input仅认证L尾，没有认证W/aux尾。反例N=2、actual=1，W=[4,9]、aux=[0,1]、L=[0,0]、表=[0,0]满足原来的所有等式却违反补零。主执行者新增非零W/aux尾拒绝测试，实际RED（`review-red`），再给三个块加入独立随机zero-tail sumcheck/同PC opening并修改wire，实际GREEN（`review-green`）。不是仅修改metadata。

两个Minor也已修复：domain隔离测试现在使用相同历史/计数，只改变domain；oversized fixture现在修改真实长度字段，并断言`wire count limit`拒绝原因。另补invalid G1、canonical Fr、截断/尾随、换模型重新生成proof的替换拒绝。

审查未发现PC布局点拆分、IPA等式、FS canonical rejection、Lasso product/leaf、Range partial limb、四块selector/L opening、稀疏GKR或初版Bundle中另一个可证验收绕过；这不等于证明不存在漏洞。

## 整数及矩阵模块增量审查

确认round的B<=2^62、A/q<=64-bit使整数等式残差远小于field；max的差值/Gap range也不能回绕。product leaf为D+inactive，padding取1，必须有真实active score取等。提出的B端点、signed64、N=1和source padding测试已补。

确认Matrix的C点[col,row]、A点[inner,row]、B点[col,inner]，随机矩形padding及同A/B PC终点正确。提出的actual_columns<columns、B/C列padding、inner=1零轮sumcheck测试已补。

## Exp与wire增量审查

确认u32分支唯一性、差值<2^33的field lifting、eq加权degree3关系、native(0,T0)及原始E尾绑定。独立重建全部8192项exact-rational证书，首零7453，u32-LE表摘要为`1512c3a6be872285a1f51dea8d5356e5a345072692bb316b3dd7a8fb05c1e8b1`。

发现Minor：Python `assert`会在-O或PYTHONOPTIMIZE下关闭，使错误表文件也被报告校验成功。主执行者新增-O错误文件测试，实际RED后改显式异常，GREEN；CMake加入优化模式回归。

主执行者另发现Digest临时迭代器UB，ASan实际复现`stack-buffer-overflow`，改成同一个局部Digest对象的begin/end后ASan通过。普通测试此前通过不能掩盖这个bug；red/green日志均保留。

最后增量审查Exp kind3/4 wire、expectedSpec来源、字段覆盖、64MiB限制和`--exp`入口，未发现新的Critical/Important/Minor。没有把既有通用malformed-wire用例冒充Exp专用全面fuzz。

## 未判定的范围

秘密分母/正域sqrt及其kind5–8 codec已经追加独立静态审查：未发现Critical/Important或新增有价值Minor；确认正B、半开端点、正sqrt区间、degree3逐坐标乘积、原始PC、补零和保守<2^128残差。codec完整覆盖helper/Range/binding/endpoint，先检查u64位宽再转换，确切数量/expectedSpec/资源限制、CLI及新增10个跨进程case顺序一致。审查者未运行远程二进制；实际构建与32case结果由执行者记录。

主执行者用独立C++ probe确认MCL直接Fr(UINT64_MAX)构造产生−1，导致最初sqrt边界fixture失败。改为32字节canonical decode生成真实2^64−1后，13项完整suite通过；没有放宽关系或改预期root来掩盖失败。失败、诊断、通过日志均保留。

真实GPT量化配置、完整模型接入、全部fused算子、全模型性能、完整ZK/模拟器、knowledge提取、具体安全位数均未通过这次静态审查。Protocol1(7)修正没有作者勘误认可。依赖二进制的运行结果由执行日志提供，审查者没有独立重构建这些依赖。无未处理Critical或Important被故意略过；上述未完成目标仍明确列在验收缺口中。
