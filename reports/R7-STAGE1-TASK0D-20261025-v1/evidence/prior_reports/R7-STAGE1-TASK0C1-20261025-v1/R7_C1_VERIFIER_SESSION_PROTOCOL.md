# R7_C1_VERIFIER_SESSION_PROTOCOL
日期：2026-10-25。状态：独立交互式 research-only 会话；不是 NIZK。

## 1. 权威与进程
可信启动器以固定绝对脚本路径运行 python3 -I -B；启动前导入环境不会采用 P 的 PYTHONPATH、cwd sitecustomize。V 预加载本地可信数学模块并设置/检查 Linux PR_SET_DUMPABLE=0 后才 READY。P 与 V 是不同 PID、不同地址空间、匿名管道连接。V 不导入 P 回调，不反序列化 Python 对象，没有共享可写 tape。
部署假设：OS、启动器、已加载 V 代码及标准库可信；启动前任意替换 V 可执行文件、内核/root 权限、伪造启动器输出不在此研究边界内。同 UID 内存写 /proc/PID/mem 实测 PermissionError。P 可中断进程造成不可用，但不能通过协议提交状态取得接受。

## 2. 会话和对象身份
传输 domain 为 R7-C1-INDEPENDENT-INTERACTIVE-RESEARCH-v1；数学 suite 保持 Task0-C 原值，因此群基和关系不变。每个 V 独立生成 32-byte session id，并在 READY 提供。随后 bind 固定外层 statement、PC、claim、layout、degree、wiring 和 mode。
外层 initial hidden claim 的业务授权仍是原 Task0-C 的外部义务；本接口固定它，不声称创建模型注册。
每个请求严格含 op,sid,seq 及该 op 的规定字段；seq 从 0 递增。子 session 由原数学函数派生，可以不同于外层 sid，但须与 V 重算的完整 child statement 相同。prefix 完全由 V 根据收到的实际消息计算，不能从 P 提交的 hash 取信。

## 3. 消息字典
| P op | 额外字段 | V 行为 |
|---|---|---|
| bind | statement | 验证 mode/suite/session/范围，固定外层 |
| open | family,statement | 与独立重建组件比较；只有合法组件能开始 |
| message | label,value | 必须为下一条预定消息；先校验再吸收 |
| finish | 无 | 当前组件全部消息完成后，执行原数学检查 |
| complete | statement,proof | 与在线重建完整对象相同，再执行全链数学检查 |

未知字段/op、state/receipt/seed/preselected、提前 complete、提前 challenge、重复 seq、错误 sid 均终止。JSON 禁止重复 key、浮点/NaN/Infinity、超长整数和非法类型。没有导入 receipt 的消息。
V 合法回复 CONTINUE，含对应 sid/seq 和零到多个公开 coins。每个 coin 为 [label,规范 Fr hex]。挑战由 V 在固定触发点自动发出。P.challenge 只消费队列；P 自己绕过适配器的保护也无法改变 V grammar。

## 4. 挑战触发和方程不变
| 组件 | 发挑战前必须固定 | V 发出的挑战 |
|---|---|---|
| SC round i | d+1 合法 coefficient handles，固定 round/degree/id | r_i |
| relation | 完整 spec、所有 representation/product first messages | rho_l |
| relation | B 群点 | 本 batch c |
| aggregation | 完整已验证 L0 SC 与 endpoint/product；X/Y | λ0,λ1 |
| H-PC | PC、points、完整 handles、parent/session | audit beta |
| audit/opening | A,B | c |
| response IPA | s,u,t，且 V 重算 R 与收到的 (R,t) 相同 | chi |
| IPA round | L,R | fold x |

原方程保持：
- 2 C_i0 + sum(j>=1) C_ij - S_(i-1) = Delta_i H，S_i=sum r_i^j C_ij；
- z_x G+z_eta H=A+cC，z_D H=B+c sum rho_l R_l；
- alpha+cX=z1 G+z2 H；beta+cY=z3 G+z4 H；delta+cZ=z3 X+z5 H；
- endpoint T=A_public X+A_public Y+M_public Z，Z=XY；
- aggregation S'=λ0X+λ1Y；
- H-PC response/core/IPA 方程由 SHA 相同的 hpc_protocol.py 检查。
两层是四个独立 relation c，不存在全局共享 c。

## 5. 逐组件认证与最终接受
SC rows 和 terminal 由 V 构建 proof；递推 relation spec、session、parent digest 用原 helpers 重算。Endpoint 首次提供三个新 X/Y/Z 承诺；V 仅取这三个合法新点，重新构造全部其它 statement 字段，在发 rho/c 之前比较。
H-PC 的 R 由 V 从自己发出的 c、已固定 A、xi 与收到 s 重算。proof 中 c/chi/x 字段由 V 本地 coins 填充。P 最终提交的字段只能是冗余副本。
每个组件结束须通过原 verifier，才进入下一组件。两层 aggregation 在 L0 验证后开始。原 receipts 全在 V 内构造；complete 不含 receipt。最终 proof/statement canonical 相等比较只消除 JSON tuple/list 表示差异，不改变 scalar、point、layout、handle 或数学方程。

## 6. 终态、超时与重放
状态 READY→ACTIVE→ACCEPT/REJECTED/FAILED/TIMED_OUT。终态关闭进程，不允许恢复。缺消息/EOF 为 REJECTED；熵失败为 FAILED；idle/partial-line/total/输出超时为 TIMED_OUT。可信启动的 --rng-fail 只供故障测试，P wire 不可设置。
V CPU 120s、address space 512MiB、total wall 60s、idle 10s、单消息 2MiB、总输入 16MiB、2048 events。wall alarm 覆盖计算和阻塞；每次接收及发 coin、最终 ACCEPT 前检查 deadline。输出 nonblocking+bounded wait；失败终态尽力写出后非零退出，不因对端不读而挂起。
只支持当前 V 进程中本地 replay；没有磁盘 replay/import API、外部密钥或自签 receipt。客户端日志和审计日志供人工复核，不能重新提交成为权威状态。

## 7. 安全范围
以上是实现来源和顺序不变量；不推出 FS NIZK、malicious-V/concurrent/reset ZK、完整 joint knowledge 或生产 R7 安全。压缩 1/(q-1) 仍仅是挑战前已固定非零错误向量的界；跨 rho 分支 openings 一致性需独立 binding/forking 归约。
