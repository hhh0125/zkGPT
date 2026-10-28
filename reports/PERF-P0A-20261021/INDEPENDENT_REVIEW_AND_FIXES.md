# 独立审查与修正

按 requesting-code-review skill 由全新只读 reviewer `/root/p0a_security_review` 审查初始补丁和测试，不向其提供完整会话历史。无 Critical；发现 Important：仅匹配 a_/b_ 不足以固定 MCL 曲线方程分派。specialB_=Plus4 时 (0,2,1) 属于 b=4 方程且阶为 3；旧严格 order 拒绝，初始快速路径误接受。已先加入反例并记录 RED（14-review-red-test.log），再要求 specialA=Zero/specialB=GenericB，记录 GREEN 及最终 66 项回归。

审查另指出显式表示边界覆盖不足，原为 Minor，按本轮 codec 安全要求提升为 Important 并同次修复：signed-zero、内部字段超范围、合法/非法 Jacobian 无穷远、非归一化 projective 点以及确保 nonsquare 样例实际找到。修复后 focused 与回归均通过。初始和修复后微基准分目录保存，最终报告只采用 measurement-final。

审查没有证明整个原协议安全、没有完成真实 oneblock 或本地归档核查，也没有认可并发 MCL 重初始化；这些范围分别标为待门禁、由主代理独立核验、已有工程前提。没有把审查意见当成真实运行证据。
