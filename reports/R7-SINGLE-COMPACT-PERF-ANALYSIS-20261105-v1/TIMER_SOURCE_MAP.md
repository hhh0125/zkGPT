# 源码计时点索引

全部文件均在ANALYZED_SOURCE.zip，以SOURCE_IDENTITY_VERIFIED.json记录SHA。

|记录|源码|边界|
|---|---|---|
|PHASE_BEGIN/COMPLETE/CPU + census.phases|oneblock_main.cpp:229-234，classified_controller.inc:124|fn()包络；CPU仅Full12输出|
|SINGLE_HYRAX_COMMIT|single_hyrax.cpp:112-123|blind/generator/row worker整个commit|
|OPT2/OPT3A_AUTH|classified_commitments.cpp:38-68,142,146-148|mapDigest CPU+prepareClaim CPU累计；wall只有prepare；kernel累计|
|OPT3B_PLAN|classified_commitments.cpp:51|单claim storage状态，无单claimwall|
|OPT2_KERNEL|classified_commitments.cpp:55-68|每次dense result分配+冻结map遍历系数累积|
|OPT3C_K|public_k.cpp:35-109|build/fold/io不同嵌套边界，参PUBLIC_K_PROFILE|
|OPT3D_U_ROOT/ROUND|u_streaming.cpp:56-109，unified_input.cpp:80|ROOT含provider；ROUND聚合scan/product counters|
|OPT3C_ROUND|unified_input.cpp:77-86|该轮poly+Sigma+coin+advance+combine|
|UNIFIED_SUMCHECK|unified_input.cpp:70-96|含独立terminal kernel_at/endpoint|
|COMPACT_QUERY|single_hyrax.cpp:144-164|validateSecret之后到stitch/equality；preparation只fold callback|
|UNIFIED_OPENING|classified_commitments.cpp:195-201|含singlePlan/descriptor/open及完成绑定|
|A3_TIMING|hyrax_a3.cpp:88,99；classified_controller.inc|knowledge和prooflog累加；排除A3 statement/basis retrieval|
|OPT2_MSM|opt2_metrics.cpp:13-27|wrapped区域CPU，六个旧bins，非wall|
|FS hash/file_io|fs_io.cpp|FS内部API，不含K/mapping/cache总体|

原始timer数值与每条行号由EXISTING_TIMING_INVENTORY.csv及parsed/*-[PV].json提供；同一记录的多个timer也分别列出，不将它们默认视为互斥。
