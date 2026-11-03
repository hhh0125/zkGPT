# R7 OPT3-D — Private U Streaming 执行报告

Windows 权威工作树已实施，固定 Linux 工作目录已编译和执行。基线为已成功独立 Full12 P/V 的 OPT3-C：401个源文件及当时未提交修改均冻结保留。当前404源文件；8条路径有变化。HEAD `8365fc13613a5269970f1fdef83a1cb883a45606`，branch `r7/remove-full-base-registration-20261029`。未 commit/push/merge/reset/clean/删除，未访问GitHub或修改受保护的原始远程仓库。

Oneblock P 531.958069s，相对602.071062s减少11.645%；V 215.819048s，5358/5358 VERIFIED。Full12 P 3623.474955s，相对 OPT3-C 4486.098575s 减少 19.229%；V 1084.155215s，相对基线1048.835131s增加 3.368%。本轮未优化V，这组历史/当前单次测量不能给出回归的统计置信结论。8112/8112关系由独立 V 验证。

安全状态仍是 `FS_NIZK_CANDIDATE` / `BLOCKED_CRYPTOGRAPHIC_DESIGN` / `may_enter_stage2=false`。此处的工程通过不构成联合 Knowledge Soundness 或 FS 安全证明。

## 1. U重复扫描的原因

每轮挑战前，需要当前全局前缀下的U0/U1。当前实现未持久化U的折叠状态，因此每轮前缀加权和重新从原始sealed Root计算；这是当前算法选择，并非声称协议强制所有实现扫描同样次数。Full12仍扫描32轮、45物理Root，共23,152,623,616个私有padded逻辑坐标；公开X另列。逻辑扫描数未下降，也未通过改变计数制造加速。模型实际长度之外返回零，不从源数组载入该位置。

旧SplitWeights在每个坐标计算low*high，再乘U：通常两次Fr乘法。真实公开诊断每种3次，约1M坐标旧收缩0.087CPU秒，受检oracle载入0.011，连续载入0.005，单Fr乘法循环0.039，加法归约0.0067。分项是独立公开诊断kernel，不可相加为真实端到端时间；公开地址整数运算可被编译器优化。实际新路径U收缩/K读与系数生成分别按tile计时，完整Root/round/进程时间另计。

## 2. 修改前后读取路径

OPT3-C已经按Root及Root-local连续坐标访问，也已经在构造配对时读取K，没有完整U0/U1或乘积数组。本轮保留这一关系，增加受检的原sealed RowMajor存储借用：helper指向原VectorSecret，model指向原Secret.values，模型尾部隐式零。认证前及每次Root借用前验证原地址/长度、公开Root形状与session/layout/map digest；不新增Witness副本。

新模块 `u_streaming.cpp` 为当前轮保留固定tile，先生成当前tile的U收缩值，再在主线程读取相应K并立即累加系数。默认1024pairs，配置硬上限4096pairs。回合结束释放；挑战仍由原io.coin抽取，PublicK仍在原时点advance，不读取下一轮挑战。

## 3. 是否消除重复映射

U的旧循环本身没有upper_bound/Root-Centric映射查询：已经持有Root-local Value。不能把消除getter/bounds开销说成减少原本不存在的映射查询。Root ID、padded/stored与借用入口是公开计划；必要边界和seal检查在每次读取前完成，内层只计算连续索引及模型零尾判断。K和Claims的冻结映射、全部digest与身份校验未改；`public_k.cpp/.hpp`、`classified_mapping_plan.hpp` SHA不变。

## 4. 是否减少Fr运算

公开低位前缀权重表最多16位，按真实前缀共享；较高位各自为有界表。利用Fr分配律，先完成同一低权重组的U点积，再乘该组公共高权重；省去每坐标的low*high乘法。无除法，挑战0/1也成立。U收缩乘/加、系数乘/加分别统计；不把权重构造、selector或Sigma成本冒充U收缩计数。

仍使用旧monomial形式c0=a0*b0，c1=a0*(b1-b0)+(a1-a0)*b0，c2=(a1-a0)*(b1-b0)。任务第5D的裸cross-sum不是该monomial c1；按任务要求的逐轮精确一致性保留原公式。小Root维度耗尽后，按suffix先分别组合全部U和K，再计算乘积；保留高位selector、跨Root交叉项及K_X=0但U_X非零情形。

## 5. 是否减少分配

一个tile跨该轮全部Root复用，公开低表按前缀共享，避免每Root重复分配相同大表。实测发现初版每tile的std::function包装分配，针对性RED后改为同步借用闭包，全部worker完成后才释放，并加入调度分配上界回归。旧代码并未分配完整U临时数组；本轮也没有。原始日志记录每轮ordinary C++new次数和累计字节（主线程、包含K provider/fallback；不含C malloc/aligned/worker系统分配），不能把它们称为活跃峰值。源Fr副本仅限tile和有限局部累加器。真实45-Root形状公开fixture的主线程 ordinary-new 中位数：reference 13136次，选中 stream4 7926次。这是包含K provider的实测次数，不能全部归因于U。

## 6–8. 单位坐标时间、额外私有内存、并行收益

下表为至少3次的中位数，秒。small逐case串行，用于比较1/2/4线程独占同一CPU0–3的调度收益；native每批3个同方案case共享总共CPU0–3，用于大形状吞吐比较，两类不能混作相同并发。native从真实makeLayout取得45 Root尺寸、分区、顺序和X32768，但使用重复的公开U backing buffer及合成公开K，不等价于真实Witness读带宽或真实1502Claims的Kernel构建。

|fixture n|reference|span/unfactored|getter/factored|stream1|stream2|stream4|
|---|---:|---:|---:|---:|---:|---:|
|small 1024|0.009120|0.011457|0.007565|0.007407|0.010682|0.010907|
|small 16384|0.131351|0.108967|0.072484|0.070711|0.110166|0.110607|
|small 1048576|5.264507|5.664507|3.174491|3.053434|2.422111|2.051765|
|small 16777216|98.595250|102.022039|64.245000|61.283388|49.712372|40.893224|
|native45 16777216|1342.083462|1384.519225|842.977159|814.894028|707.099789|705.033413|


各规模完整32轮系数、挑战、终点摘要精确相等。reference未独立计时的U子阶段/RAM字段为null；不是零耗时。原reference、span但未factored、保留旧getter的factored，以及完整stream1/2/4逐项列出。已经存在的Root-major/fused机制如实作为基线，不伪称新增。真实运行另列private_U_source_Fr_loads与public_X_source_Fr_loads：由逐Root源载荷字节除以本次构建实测sizeof(Fr)=32得到，包含各轮重复载入，排除隐式零padding。它们与padded逻辑坐标、K折叠坐标分别统计；载荷字节不是硬件DRAM/cache/bus流量测量。

确实新增了有界私有tile和线程局部空间，没有新增完整私有Witness/Root-folded状态。Private tile默认65,664bytes；最多(2*4096+4)*sizeof(Fr)。每个后台worker请求1MiB stack并计入guard，最多3个；主线程参与。公开权重、tile、stack reservation和保守元数据先按公开尺寸预算，超过8MiB在分配前拒绝。`U_TOTAL_EXTRA_RAM_BYTES`是该明确分配预算（包括公开表和所请求虚拟stack保留），不是独立测量的进程RSS，不声称涵盖精确allocator/TLS/内核开销。实际P/V RSS见下表。

workers只读已验证U span，写各自tile槽；每tile等待完成，再由主线程读取PublicK可变file cache。没有并发K读取、共享Fr累加器竞争、提前coin或秘密文件。线程结果在Fr精确合并。小规模调度可能使多线程变慢；需按各规模表判断，生产真实大Root使用基准门槛选中的配置。

## 9–10. 系数、Proof与Transcript

保留原93测试并新增1测试，最终94/94。专项独立dense weights逐Root每tile检查U0/U1及局部收缩，比较全部3系数、递推/终点，28/32轮，1/2/4threads，0/1和固定随机Fr；涵盖四分区、高selector、不同尺寸、短非零stored-span、边界落在组内部、部分/全零padding、公开X零K。原有测试覆盖映射重复物理坐标、affine bias及rho0/1/随机。

测试专用固定Random和getrandom nonce、相同FS context下，小型Classified集成夹具以及补充的完整28/32轮、45个原VectorPC/Hyrax Opening模块夹具中，旧OPT3-C U和新U完整规范Proof文件逐字节一致，挑战和SHA256 Transcript一致，独立Verifier与EOF通过。补充模块共有8份P Proof和8次独立V进程，比较reference与1/2/4线程完整文件；这是公开toy模块测试，不能替代真实模型/GKR/8112关系验证。生产随机源不变。真实Proof使用新context和新随机性；不声称其随机payload与旧Proof相同，仅逐消息tag/顺序/label/长度结构和coverage对比原OPT3-C成功Proof。

一次独立审查的两个Important问题均有针对性RED→GREEN：异形Root重复低权重表超过预算；vector reserve导致cached span失效。前者共享并预先预算；后者借用前核对活跃vector地址/长度。初版seal复现曾因无关claim/Sigma拒绝而过宽，收紧为读取前seal错误后获得真实RED；所有日志保留。补充FS模块首次编译因引用私有fold helper失败，改为独立r1::mle oracle后通过；失败源码和日志保留，未改运行时源码或重跑90项已完成基准。未声称进行TSAN、pthread创建故障注入或模型别名专项ASan。

## 11. 真实独立P/V与恶意Proof

P完全退出、文件关闭后才启动新Verifier进程。Statement、120参数及Anchor身份保持，Proof存新独立路径。Oneblock重跑16条独立恶意Verifier拒绝路径；若运行Full12，重跑原4条关键独立拒绝路径，未把16条Oneblock说成16条Full12。

### Oneblock

|指标|值|
|---|---:|
|Prover wall s|531.958068725653|
|Verifier wall s|215.8190476084128|
|P CPU s|562.913841|
|V CPU s|219.098091|
|P RSS KiB|4413280|
|V RSS KiB|1211668|
|private U logical coordinates|1996492288|
|U Fr payload read bytes|61111386112|
|U/K fused payload load bytes|65676895552|
|U contraction CPU s|53.57068117|
|U contraction wall s|26.852264066|
|K read/coefficient wall s|12.247957027|
|U contraction Fr multiplications|1926085696|
|U contraction Fr additions|1997420810|
|tile bytes|65664|
|thread stack/local reservation bytes|3159344|
|total extra allocation upper bound bytes|6401536|
|main-thread C++ allocations including K provider|1371|
|Fr_storage_bytes|32|
|private_U_source_Fr_loads|1908813312|
|private_U_source_payload_bytes|61082025984|
|public_X_source_Fr_loads|917504|
|public_X_source_payload_bytes|29360128|
|kernel_calls|6|
|kernel_coordinates|230062580|
|kernel_cpu_seconds|5.84742|
|fallback_rebuild_calls|0|
|K_fold_coordinates|142672114|
|K_fold_cpu_seconds|9.64102|
|K_fold_wall_seconds|11.3265|
|K_io_wall_seconds|6.92377|
|K_file_read_bytes|8589934720|
|K_file_write_bytes|4294967168|
|K_RAM_peak_bytes|672928504|
|K_file_peak_bytes|2147483648|


### Full12

|指标|值|
|---|---:|
|Prover wall s|3623.4749548565596|
|Verifier wall s|1084.155215264298|
|P CPU s|4037.055184|
|V CPU s|1129.978537|
|P RSS KiB|52088808|
|V RSS KiB|6313936|
|private U logical coordinates|23152623616|
|U Fr payload read bytes|736798703616|
|U/K fused payload load bytes|783106057632|
|U contraction CPU s|605.02958352|
|U contraction wall s|262.633905185|
|K read/coefficient wall s|92.795975707|
|U contraction Fr multiplications|22430285536|
|U contraction Fr additions|23153828382|
|tile bytes|65664|
|thread stack/local reservation bytes|3159344|
|total extra allocation upper bound bytes|6469440|
|main-thread C++ allocations including K provider|7894|
|Fr_storage_bytes|32|
|private_U_source_Fr_loads|23023910912|
|private_U_source_payload_bytes|736765149184|
|public_X_source_Fr_loads|1048576|
|public_X_source_payload_bytes|33554432|
|kernel_calls|115|
|kernel_coordinates|6504025872|
|kernel_cpu_seconds|179.732|
|fallback_rebuild_calls|70|
|K_fold_coordinates|2109804452|
|K_fold_cpu_seconds|63.2104|
|K_fold_wall_seconds|71.6906|
|K_io_wall_seconds|32.3567|
|K_file_read_bytes|36775664032|
|K_file_write_bytes|18387827424|
|K_RAM_peak_bytes|1010259296|
|K_file_peak_bytes|4294967296|


Full12如PASS，保持12blocks/277layers/48FC/76032Exp/120参数/45Root/10Wchunks/1502Claims/11跨Block连接、32轮单一全局Sumcheck、45原Hyrax Opening、54行物理zero-tail、无FullBase注册。GKR/Range/Exp/Lasso/Memory/Product/FS suite/消息格式与恶意拒绝机制保留。完整逐Root逐轮计数、时间、分配、K折叠和IO见JSON与原始日志。

## 12. 剩余瓶颈与证据边界

实际P顶层阶段按wall降序如下。CPU来自原始PHASE_CPU，RSS是阶段结束值/累计峰值，缺失时保留null。可与内部Sumcheck/K/Root计时交叉核对，不能重复相加。

|阶段|wall秒|CPU秒|阶段末RSS KiB|累计峰值RSS KiB|
|---|---:|---:|---:|---:|
|protocol1-unified-input-sumcheck-and-opening|1163.27|1493.9|30097920|52088808|
|native-Range-Exp-Lasso-Memory-Product|1068.06|1067.81|30097780|52088808|
|all-classified-roots-prechallenge-seal|740.122|739.62|36491032|52088808|
|frozen-witness-and-prechallenge-lookup|304.107|320.4|34576832|34576832|
|all-native-GKR|205.774|241.858|36491280|52088808|
|all-source-original-handles|60.029|60.0274|30097780|52088808|
|classified-public-X-Y-constants|0.0303278|0.0303578|36491032|52088808|
|classified-physical-zero-tails|0.0279504|0.0279388|36491032|52088808|


U逻辑扫描仍在每轮进行，仍需原Witness读取及Fr乘加。Kfallback/校验IO、Claims映射准备、原45次Hyrax Opening及GKR/Lasso等非U成本没有消失；不能仅用U总坐标数确定下一瓶颈。`UNIFIED_SUMCHECK`包含其endpoint流程，Root的K-read/coefficient时间含K载入校验；这些嵌套时间不能相加。真实P总时间是收益验收依据；历史单次P/V与当前单次运行无统计置信区间含义。

四阶段Windows归档含404源码精确bytes、构建/依赖/配置、参数、Proof/Statement、正/负原始日志、审计与资源测量。容器SHA、每成员SHA/gzip与实际恢复后再读SHA均验证。恶意Proof以新正例+精确delta恢复，逐个校验完整SHA和长度。最终diff相对401 dirty baseline，并在独立小恢复目录git apply后逐文件SHA验证；未改动索引。两次部分候选基准分别因补齐指标及实测每tile调度分配而停止，均保留且从最终正式结果排除；未停止任何Proof或其他用户任务。

本地阶段：`C:/Users/tanyt/Desktop/program/zkGPT-backups/r7-opt3d-u-streaming-20261102-v1`；远端：`reports/R7-OPT3D-U-STREAMING-20261102-v1`。详见机器可读交付、各`*_WINDOWS_VERIFIED.json`、`PATCH_RESTORE_WINDOWS_VERIFIED.json`和最终交付恢复收据。归档不含SSH配置/凭据/私钥或生成的私有Witness/blinding文件，未授权任何清理。
