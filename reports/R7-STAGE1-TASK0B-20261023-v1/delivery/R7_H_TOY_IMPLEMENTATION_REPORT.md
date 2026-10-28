# R7_H_TOY_IMPLEMENTATION_REPORT

日期：2026-10-23（Asia/Shanghai）。Stage1 Task0-B，suite H-PC-CSIPA-AUDIT-RESEARCH-v1。仅新增 reports/R7-STAGE1-TASK0B-20261023-v1；全部源码编辑、测试、测量在固定cwd /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work 执行。N≤1024、1≤batch K≤4、每MSM≤32 bases。没有lookup table，因而没有超过64项的表；没有读取真实120参数、R6 proof或真实W anchor，没有真实注册、生产verifier、oneblock/full12或生产ZK。

冻结源码575项重新SHA核对；真实参数/proof/anchor仅记录前后stat元数据、复用既有SHA身份，不为验证冻结而读取内容。原may_enter_stage2=false不变。以下为可人工复核的数学证明，非机器形式化或生产认证。

## 实现、测试与证据

独立Python stdlib实现：curve.py负责Jacobian乘法、规范point和q-subgroup/H2C；entropy.py负责OS源及进程内重复guard；protocol.py负责PC/HiddenScalar/audit/core/responseIPA/simulator/trusted replay。test_hpc.py/test_helpers.py是局部测试与代数辅助；measure.py/run_all.py是有界测量。函数物理行号和SHA在evidence/CODE_INDEX.json。没有链接/导入R6实现，没有安装依赖或运行生产build。

最终全部research test suite 21/21通过：

| 要求 | 测试及解释 |
|---|---|
| 正确commit/open | 独立手算w=[2,3,5,7]，r=(2,3)的MLE=19、r=(0,1)为5 |
| wrong value/point/layout/blind/PC身份 | 修改statement拒绝；fresh verifier下实际错value20应为19、错rowρ和错η均被方程拒绝 |
| 多点 | 同PC两个不同point，4点上限通过，5点拒绝 |
| repeated/samehandle | 同point同Y使用fresh A；不同point的常量vector共用Y通过，unequal evaluation共用Y拒绝 |
| zero/padding | zero W/y、actual1/padded4的诚实zero rows、N1零维point；不声称恶意zero-tail已证 |
| zero challenge/weight | 0 verifier coin拒绝；实际MLE半权重及f²=−1令a_final=0仍通过；audit实际a=0 |
| invalid group | 直接parser拒绝超p、非规范长度/编码，合法identity保留 |
| entropy | 短读/异常/同对象和跨对象重复熵拒绝；仅进程内，不跨重置 |
| session/suite拼接 | 全statement、prefix和coins匹配；换session/suite、拼core拒绝 |
| wire无rawsecret | 特定高熵toy W/ρ/η的规范hex不出现，原始folded x与wire final对比；只是辅助，ZK来自独立分布证明 |
| 提取辅助 | scalar差商、2×2Vandermonde、C4三叉递归表示恢复；非完整恶意P extractor |
| 固定PC模拟 | API无W/ρ/y/η输入，生成可检查的固定PC/Y视图；不以此测试代替HVZK证明 |

保留全部日志：RED_01（15项未实现）、GREEN_01（15通过）、RED_02（夹具非规范Fr先触发rejection，1error）、RED_03（修正夹具后跨实例guard缺失，1failure）、GREEN_02（guard补齐后19通过）、GREEN_03（21通过）。没有删失败；旧entropy源码保存在logs/entropy_BEFORE_PROCESS_GUARD.py。最后两项是补充验证，不声称具有独立先红后绿周期。

## 资源实测

每N三trial，每trial fresh进程/新toy数据/两个opening+一个audit，共15次均通过。worker cap CPU120s、AS512MiB，单MSM最多32。完整raw trials在R7_H_RESOURCE_MEASUREMENTS.json及logs。

| N | R/C | commit ms中位 | interactive P ms中位 | V replay ms中位 | proof JSON B | max RSS KiB |
|---:|---|---:|---:|---:|---:|---:|
|1|1/1|9.398|44.498|51.336|2072|17292|
|16|4/4|31.537|127.794|95.038|3360|17144|
|64|8/8|101.096|231.216|152.558|4005|17168|
|256|16/16|359.531|431.656|256.325|4651|17816|
|1024|32/32|1406.762|816.600|445.380|5297|19444|

commit含冷H2C/setup与row commitments，不含toy输入生成。P含进程内verifier抽币/prefix，不含PC/handle创建和wire编码。V为同worker缓存已被proving预热的replay，不是冷启动独立Verifier。RSS为Linux worker整体ru_maxrss，非P/V分相峰值；wire是canonical JSON/hex且含public coins，statement另计。timing_scope原错误说明已修正，数值和raw日志未被替换。

N1024：proof5297B、public statement4074B、PC JSON2362B，P约817ms、V约445ms、PC约1.407s、RSS19444KiB。不能外推2^24/2^28或正式R7的成本。

每core代数payload：A/B两G、s/u/t三Fr、IPA 2logC个G及一个masked finalFr，共32(2logC+6)B；Y作为instance另计。若记录public challenges，再加32(2+logC)B；每batch额外audit与b、PC rows另32R。JSON有hex/headers，实测不等于原HyraxA.3的4+2logC格式。

## 局部局限与恢复

variable-time Python、自编曲线和H2C未审计为生产密码库；nonceguard不跨重启；receipt必须来自可信V。evidence/public_toy_session_N16.json和N1024保存公共PC/points/Y/transcript及当时receipt，不含W/ρ/η，仅用于SHA核对后的研究恢复replay，不充当NIZK。

源码/测试通过仅表示局部实现正确性证据；安全来自EXACT/FIXED/MULTI所列证明和假设。归档必须逐文件SHA、gzip完整性和restore-read，不读取真实模型/proof/anchor来验证恢复。
