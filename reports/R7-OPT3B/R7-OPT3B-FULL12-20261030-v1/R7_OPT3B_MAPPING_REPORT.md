# R7 OPT3-B Root-Centric Mapping 实测报告

映射优化、等价/负例、19/19 R7 CTest 与三轮微基准完成。真实 Full12：**FULL12_OPT3B_INDEPENDENT_VERIFIED**。

基线是当前 Windows `zkGPT-R7-dev` 的 OPT3-A + No-FullBase 未提交工作树（391 文件），不是 HEAD。分支保持 `r7/remove-full-base-registration-20261029`，HEAD `8365fc13613a5269970f1fdef83a1cb883a45606`。最终 394 文件，6 项本轮增量；原有源码其余字节不变。`before/`、基线完整 patch/Git/SHA 和独立 `R7_OPT3B_MAPPING_DIFF.patch` 均保存；独立 patch 反向应用检查通过。Windows/Linux 最终 SHA 一致。

## 为什么存在重复扫描

实际调用链：`authenticate → prepareClaim → partialDot(P) → 每个Root的kernel → 原Sumcheck/opening`。`defer/mapDigest` 首次绑定 Mapping Digest；`prepareClaim` 重新检查完整边界、身份、hint、bias 与摘要，并在其后保持原 lambda 抽样时点。OPT3-A 虽然已按Root检索冻结runs，但未冻结声明在每个Root都执行原回调、RowMajor `baseCoordinate` 分区搜索和完整canonical SHA。

从上一轮真实 Proof 提取1502个公开defer消息和公开X/Y位置，以真实公开native graph/source spans重建全部映射，保持原声明顺序。每个Mapping Digest与Proof一致，准确复现1076冻结/426未冻结：29个Run数超剩余预算，397个遇到预算已耗尽。不支持stride为0；168个多Root、291个含constant/padding是重叠结构属性，并非独立失败计数。逐声明详见 `R7_OPT3B_MAPPING_CENSUS.json`。

原Root11每次fallback扫描583705824坐标，fallback仅命中108项，含已冻结/公开项共1584项。全量原runs20187390，简单按Root合并带logical step后仍19646651，按40字节约785866040字节，不能装入256MiB。没有通过直接增大内存缓存解决。

## 已实现算法与边界

`classified_mapping_plan.hpp` 将已逐项观察的非constant映射表示为 `(root, logical_begin, logical_step, count, coordinate_begin, signed_stride)`。每个Root保留最后一个候选run，可跨其他Root/constant位置合并等差项；重复物理坐标以stride0保留每个逻辑项及重数，负stride同样支持。

小计划在RAM排序，大碎片计划每4096个run排序分组，写入各Root的正向块链。Root链首/尾/总run数绑定在当前声明Plan中；所有Plan与存储仅存活于单次Authentication。Kernel只读目标Root的run链，partialDot按Root读取一次，消除全逻辑域乘Root数的重复回调与摘要。没有构造完整逐坐标数组。

RAM map上限保持普通16MiB/Full12 256MiB；weights32/256MiB。run vector容量与Root元数据计费，构造scratch有界，读scratch163840字节。公开run文件上限为RAM map预算4倍，即64MiB/1GiB，使用固定项目目录中的匿名0600 `O_TMPFILE`，不持久化文件名，不含Witness、bias数值或秘密。文件页缓存不计入进程RSS，可能使用额外有界OS内存；没有把文件字节伪称为零内存成本。

完整canonical digest/bounds/hint/bias验证通过后才公布frozen状态；随后禁止回调。RAM元数据不足或文件硬上限耗尽时，丢弃当前声明计划并保留原完整摘要检查回退，原因分别记录；已写但放弃的块仍占文件预算，不能污染先前链。实际I/O失败立即报错，不伪装为有效计划。

等价依据：对每个非constant逻辑j，Plan恰好出现一次原 `(j,root,index)`；相同index的多个j保持多项累加。`K_k[t]=Σ_i,j map_i(j)=(k,t) λ_i W_qi(j)`，`partial_k=Σ_t K_k[t] value_k[t]`，affine bias独立原样计算。按Root重排仅改变精确Fr加法结合顺序；`(λ*b[high])*a[low]` 与原权重完全相等。没有提前使用FS挑战或跨Statement/Session复用。

修改文件与函数：`classified_commitments.cpp/.hpp` 的prepareClaim/kernel/partialDot/Weights::scaled及诊断；新 `classified_mapping_plan.hpp` 的Builder/Store/Plan；新test-only OPT3-A reference与mapping tests；CMake测试注册。构造、tails/defer/MapHash以及authenticate消息/控制主体除一行诊断外与基线一致；FS suite/消息顺序/挑战采样、45Root身份、Hyrax A.3、GKR、Range/Exp/Lasso、Memory/Product、R6、模型和Anchor均未改。

## 验证

20组新fixture变体逐项比较Mapping身份/重数、每个K坐标、partialDot、bias与聚合Fr，另有交错预算契约与流式/故障专项；覆盖跨Root、正负/零stride、重复坐标、非幂实际长度、padding、constant和模型/非模型根。原OPT3-A/OPT2测试继续通过。预算契约在原OPT3-A按预期失败，再在新实现通过。错误Root/index/hint、变化摘要/回调、缓存耗尽、部分文件写入后超限均有真实拒绝或精确回退证据。

补充I/O故障只存在测试可执行文件：open失败、EINTR、短传输、部分pwrite/pread失败和提前EOF均覆盖，生产P/V没有注入开关。强制流式32768逻辑位置/2小Root完整认证，在固定随机源下与旧Reference的121消息37724字节完全一致，新独立Verifier回放与EOF一致；实际file491536字节。原小RAM transcript121消息37264字节也一致。二进制与SHA在mapping归档中。

19/19 R7 CTest、FS I/O、RowMajor可信参数重算与身份负例、分块布局、Full12plan/10块Anchor、新组件P及独立V全部通过。独立只读审查及补充审查无未解决项，见 `REVIEW_RESULT.md`。初次CMake target尚未configure及两处Werror缩进构建失败均保留原日志；修正运行器/括号后完成回归，没有为掩盖证明错误修改协议。

## 三轮配对微基准

同一Linux Release/四CPU affinity、`ZKGPT_THREADS=4 MALLOC_ARENA_MAX=2`；每组旧/新相同输入、点、lambda和RAM预算。旧实现是冻结OPT3-A，非pre-OPT3-A。真实样本为24个最高碎片实际声明、原相对顺序、45个真实物理Root；数值是确定性测试值，不是私有Witness。完整Full12另验收。原始JSON中的合成fixture标签 `interleaved45roots` 是运行器命名不准确：其实际映射是分段跨Root正stride，真正交错/重复坐标由专项预算契约和流式Transcript覆盖；不把该标签当作交错模式证据。

组合时间包含首次完整验证/计划/权重构建、全部物理Root Kernel和partialDot；不含公开graph/fixture创建、defer首次绑定及密码学Sumcheck/opening。Root11和Root0复用单独计时，不混入组合。

| 样本 | 旧/新组合wall中位秒 | 倍数 | 旧/新Kernel中位秒 | 旧/新计划构建中位秒 |
|---|---:|---:|---:|---:|
| interleaved45roots/1024 | 0.003472/0.003731 | 0.9304x | 0.002127/0.002131 | 0.000905/0.001073 |
| interleaved45roots/16384 | 0.033078/0.033183 | 0.9968x | 0.013220/0.013288 | 0.013725/0.013634 |
| interleaved45roots/1048576 | 1.375562/1.367518 | 1.0059x | 0.616766/0.617005 | 0.523092/0.514306 |
| native24fragmented-45roots/16777216 | 42.385716/12.608517 | 3.3617x | 38.107175/8.986060 | 2.176555/2.259622 |

真实样本每次fallback查询 `385875968 → 0`，冻结16→24。各次原始值、CPU/wall min/median/max、首次构建与复用、分配/字节、RAM/file/read字节、坐标数和RSS见JSON。

小规模无fallback样本有额外计划管理开销：1024组合略慢，16384/2^20接近持平；收益集中在真实碎片回退场景，不声称所有场景加速。RSS是包含新旧计划与public graph的同进程累计峰值，不能比较独立实现峰值。ordinary C++ new统计不含C malloc/aligned/worker/stack/OS页缓存；没有单独hook全部Fr复制次数，不能据此声称复制数为0。

## 真实 Full12

P/V均退出0，P退出并关闭FD后新独立V读取磁盘Proof。动态必须关系 **8066/8066 VERIFIED**；45Root、10可信W、1502声明、11跨Block连接完整。Statement/Transcript/Challenge/Proof身份及完整EOF通过。

Proof **263710426 bytes**，SHA256 `8de9903f6fa11511e14bc4b5c8a5c269817b0e562cbf44c53060c4630ee6d9b3`；Statement SHA `ce1f1c7253802da5ceb90355c8c34b0547f4febe710fe6ef2691e14271fcb76b`。完整注册行/响应字节0/0；zero-tail54行/15040响应字节与Anchor绑定保留。

| 指标 | OPT3-A | OPT3-B | 变化 | 百分比 | 倍数 |
|---|---:|---:|---:|---:|---:|
| prover_wall_seconds | 5749.672307198867 | 3330.973576589953 | -2418.698730608914 | -42.06672313447475 | 1.7261236617448734 |
| verifier_wall_seconds | 3251.4675149610266 | 1018.4756479156204 | -2232.9918670454063 | -68.6764316964788 | 3.1924842990751676 |
| prover_peak_rss_kib | 52079148 | 52077412 | -1736 | -0.0033333878657154682 | 1.0000333349898416 |
| verifier_peak_rss_kib | 6493924 | 6312208 | -181716 | -2.798246483944068 | 1.0287880247292231 |
| proof_bytes | 263710425 | 263710426 | 1 | 3.792038179757209e-07 | 0.9999999962079619 |
| full_registration_response_bytes | 0 | 0 | 0 | None | None |
| prover_cpu_seconds | 5834.195118000001 | 3413.4548360000003 | -2420.740282 | -41.492274993192986 | 1.7091760103194171 |
| prover_user_cpu_seconds | 5715.39569 | 3322.742505 | -2392.653185 | -41.863298969594176 | 1.7200838408030659 |
| prover_system_cpu_seconds | 118.799428 | 90.712331 | -28.087097 | -23.642451376112685 | 1.3096282136107824 |
| prover_peak_vmsize_kib | 52743092 | 52742392 | -700 | -0.00132718802303058 | 1.0000132720563755 |
| verifier_cpu_seconds | 3302.0510689999996 | 1074.318657 | -2227.7324119999994 | -67.46511078869082 | 3.073623498470063 |
| verifier_user_cpu_seconds | 3272.036495 | 1051.378203 | -2220.658292 | -67.86777272788335 | 3.11214031797842 |
| verifier_system_cpu_seconds | 30.014574 | 22.940454 | -7.074120000000001 | -23.5689502039909 | 1.3083687881678367 |
| verifier_peak_vmsize_kib | 6903048 | 6643596 | -259452 | -3.7585136305006137 | 1.0390529466270977 |

prover: user/system CPU 3322.742505/90.712331秒，峰值RSS 52077412 KiB，采样VmPeak 52742392 KiB。详细Kernel/Map/partial/Authentication Barrier及全部阶段CPU、MSM、存储和Root11实测见 PERFORMANCE.json。

### prover 认证计算与访问计划

| 指标 | OPT3-A | OPT3-B | 绝对变化 | 百分比变化 | 倍数 |
|---|---:|---:|---:|---:|---:|
| kernel_cpu_seconds | 2301.44 | 68.7613 | -2232.6787 | -97.0122488528921 | 33.46998965988136 |
| kernel_coordinates | 28283013108 | 2461708448 | -25821304660 | -91.29615915178537 | 11.489180666775695 |
| map_cpu_seconds | 434.251 | 419.344 | -14.906999999999982 | -3.4328072934777314 | 1.0355483803273684 |
| partial_cpu_seconds | 198.064 | 128.812 | -69.25199999999998 | -34.96445593343565 | 1.5376207185665929 |
| weights_cpu_seconds | 0.137952 | 0.142613 | 0.0046609999999999985 | 3.378711435861748 | 0.9673171450008063 |
| fallback_map_queries | 26850467904 | 0 | -26850467904 | -100.0 | None |
| builtin_RowMajor_base_coordinate_lookups | 21810771592 | 1319730272 | -20491041320 | -93.9491811812652 | 16.526688865707857 |
| kernel_and_partial_Fr_multiplications | 8288459448 | 7427166942 | -861292506 | -10.39146673038051 | 1.115965146970033 |
| kernel_and_partial_Fr_additions | 4923416896 | 4923416896 | 0 | 0.0 | 1.0 |
| partial_visited_coordinates | 2599956852 | 2461708448 | -138248404 | -5.317334550904309 | 1.0561595359159281 |
| kernel_matched_coordinates | 2461708448 | 2461708448 | 0 | 0.0 | 1.0 |
| total_map_callback_queries | 32104788768 | 5254320864 | -26850467904 | -83.63384072709685 | 6.110169058758114 |
| Authentication Barrier wall s | 3161.89 | 846.528 | -2315.362 | -73.2271521147162 | 3.735127485446435 |

实际计划分类 `{'FROZEN_ROOT_RAM': 1310, 'FROZEN_ROOT_FILE': 192}`；存储/读取记录 `{'RAM_claims': 1310, 'file_claims': 192, 'fallback_claims': 0, 'RAM_bytes': 2619520, 'RAM_limit': 268435456, 'file_bytes': 784365232, 'file_limit': 1073741824, 'file_read_bytes': 1568730464, 'file_read_blocks': 10714, 'bounded_read_scratch_bytes': 163840}`。未冻结声明如存在，逐声明原因保留在PERFORMANCE.json；不得把回退计为缓存命中。

Root11 实测：fallback查询 583705824→0，匹配 1584→1584，Kernel CPU 52.5834→0.00094612秒，wall 52.6173→0.000946644秒。

MSM calls/terms **1281061/558035360**，已与OPT3-A核对规模不变。Kernel Fr运算、权重、分配和逐Root统计保留原始日志；不是CPU机器指令数或完整Fr复制数。

verifier: user/system CPU 1051.378203/22.940454秒，峰值RSS 6312208 KiB，采样VmPeak 6643596 KiB。详细Kernel/Map/partial/Authentication Barrier及全部阶段CPU、MSM、存储和Root11实测见 PERFORMANCE.json。

### verifier 认证计算与访问计划

| 指标 | OPT3-A | OPT3-B | 绝对变化 | 百分比变化 | 倍数 |
|---|---:|---:|---:|---:|---:|
| kernel_cpu_seconds | 2298.54 | 70.4571 | -2228.0829 | -96.934702028244 | 32.62325585356196 |
| kernel_coordinates | 28283013108 | 2461708448 | -25821304660 | -91.29615915178537 | 11.489180666775695 |
| map_cpu_seconds | 419.325 | 420.706 | 1.3810000000000286 | 0.329338818339004 | 0.9967174226181703 |
| partial_cpu_seconds | 0 | 0 | 0 | None | None |
| weights_cpu_seconds | 0.152439 | 0.144411 | -0.00802799999999998 | -5.266368842619002 | 1.0555913330701954 |
| fallback_map_queries | 26266762080 | 0 | -26266762080 | -100.0 | None |
| builtin_RowMajor_base_coordinate_lookups | 21365314172 | 1319730272 | -20045583900 | -93.82302426551932 | 16.189152151235945 |
| kernel_and_partial_Fr_multiplications | 2913375500 | 2482729247 | -430646253 | -14.781694052139864 | 1.17345679297103 |
| kernel_and_partial_Fr_additions | 2461708448 | 2461708448 | 0 | 0.0 | 1.0 |
| partial_visited_coordinates | 0 | 0 | 0 | None | None |
| kernel_matched_coordinates | 2461708448 | 2461708448 | 0 | 0.0 | 1.0 |
| total_map_callback_queries | 31521082944 | 5254320864 | -26266762080 | -83.33077301520774 | 5.999078427045981 |
| Authentication Barrier wall s | 2614.54 | 387.746 | -2226.794 | -85.16962830937756 | 6.742919333790677 |

实际计划分类 `{'FROZEN_ROOT_RAM': 1310, 'FROZEN_ROOT_FILE': 192}`；存储/读取记录 `{'RAM_claims': 1310, 'file_claims': 192, 'fallback_claims': 0, 'RAM_bytes': 2619520, 'RAM_limit': 268435456, 'file_bytes': 784365232, 'file_limit': 1073741824, 'file_read_bytes': 784365232, 'file_read_blocks': 5357, 'bounded_read_scratch_bytes': 163840}`。未冻结声明如存在，逐声明原因保留在PERFORMANCE.json；不得把回退计为缓存命中。

Root11 实测：fallback查询 583705824→0，匹配 1584→1584，Kernel CPU 50.1191→0.000800323秒，wall 50.1413→0.000800493秒。

MSM calls/terms **281356/1299146**，已与OPT3-A核对规模不变。Kernel Fr运算、权重、分配和逐Root统计保留原始日志；不是CPU机器指令数或完整Fr复制数。

| 阶段 | P wall s | V wall s | P CPU s | V CPU s | P wire bytes |
|---|---:|---:|---:|---:|---:|
| frozen-witness-and-prechallenge-lookup | 302.659 | 5.72307 | 319.367 | 5.72039 | 0 |
| all-classified-roots-prechallenge-seal | 747.169 | 95.9587 | 747.026 | 95.9188 | 24274886 |
| classified-physical-zero-tails | 0.0278477 | 0.0307508 | 0.0278896 | 0.0307679 | 19442 |
| classified-public-X-Y-constants | 0.0298324 | 0.0450069 | 0.0298839 | 0.0449443 | 2680208 |
| all-native-GKR | 208.15 | 55.2625 | 243.424 | 78.5772 | 24520709 |
| native-Range-Exp-Lasso-Memory-Product | 1085.14 | 362.588 | 1084.53 | 362.481 | 182248997 |
| all-source-original-handles | 59.4338 | 25.6298 | 59.4281 | 25.6216 | 20225 |
| mandatory-classified-authentication-barrier | 846.528 | 387.746 | 846.362 | 387.581 | 27242747 |

真实Full12每版本各一次，受共享服务器状态影响；全部比较是本轮实测而非微基准外推。Runner wall含启动与最多5秒结束轮询延迟；VmPeak为每5秒采样。P峰值RSS基本未变，V降低约2.80%；匿名文件可能额外使用有界OS页缓存，不把该部分当作零内存成本。

实际消息类别与归一化类型均与OPT3-A一致：`{'actual_message_categories_equal': True, 'normalized_message_types_equal': True, 'new_context': 'zkGPT/R7/Full12/opt3b-mapping/20261030/v1', 'old_context': 'zkGPT/R7/Full12/opt3a-kernel/20261029/v1', 'context_utf8_length_delta': 1, 'proof_bytes_delta': 1}`。Suite v2未改；Proof实测增加1字节，恰与context长度增加1字节一致。不同真实运行的随机性不同，不要求消息值或Challenge摘要跨运行相同；固定测试随机源下完整消息、挑战和Transcript字节一致。

全部45Root/10可信W/1502声明/11连接、GKR/Range/Exp/Lasso/Memory/Product及来源认证屏障通过。旧完整注册header/masks/responses/base-registered消息全部0；54行zero-tail/15040响应字节及模型Anchor绑定保留。旧registered_rows字段仍仅计完整基础行知识注册，不含tail-only或模型Anchor绑定。

## 保存与结论

映射阶段811项完整归档已完成Windows容器/逐成员SHA、gzipCRC和解压恢复读取，394源码与120参数CAS恢复核验通过。最终Full12归档以 `WINDOWS_ARCHIVE_VERIFIED.json` 为收据；缺少该收据不代表Full12归档完成。源码完整字节、独立patch、基线差异、参数恢复表、依赖/配置身份和所有原始正负例日志均保留；私有Witness和SSH凭据不归档。

建议保留Root-Centric实现：三轮真实映射样本与精确等价测试提供可重复收益证据，本轮真实Full12结果见上述验收与性能。该优化消除重复映射扫描，没有改变认证方程或协议语义。没有自动提交、推送、合并、清理或删除历史文件。安全状态继续 `FS_NIZK_CANDIDATE / BLOCKED_CRYPTOGRAPHIC_DESIGN / may_enter_stage2=false`。
