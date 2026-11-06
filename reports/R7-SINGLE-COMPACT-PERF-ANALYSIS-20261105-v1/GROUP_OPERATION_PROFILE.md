# MSM 与直接群运算范围

源码锚点：crypto.cpp:38 msm、39 scalarCommit；hyrax_a3.cpp:9 wideMSM、37-68 prooflog；streaming_master.cpp:20 masterMSM；single_hyrax.cpp:112-123 commit；依赖3rd/mcl/include/mcl/ec.hpp:2062 mulVec（依赖源码在上一阶段dependencies归档）。

|scope/role|全census calls|wrapper calls|全terms|wrapper terms|wrapper CPU秒|
|---|---:|---:|---:|---:|---:|
|Full12 P|1426753|729281|730247709|16036381|166.488|
|Full12 V|276607|276607|10290249|10290249|80.5799|
|Oneblock P|522432|457328|68061591|1395095|62.1364|
|Oneblock V|176144|176144|678067|678067|17.1582|

P未计时wrapper的差额恰好等于single::commit自行归并的worker calls/terms：Full12697472/714211328，Oneblock65104/66666496。commitment分别499.154/43.6014秒，是包含生成元检索、随机blind、readRow清零/拼接/标量合法性检查、基复制、MSM和结果存储的inclusive总wall，不能把它全叫MSM wall或用它补全wrapper CPU。

masterMSM接受size≤16384，复用thread_local1024点scratch；每1024 chunk复制基后调用mulVec；一个逻辑masterMSM可产生多个wrapper/census调用。single::commit绕过MSMTrace，以每worker1024点scratch直接mulVec，结束后合并计数，因此不会造成当前MSMTrace静态bins的并发写入。MSMTrace本身不是原子全局计数器，若以后将wrapped路径并行化需先修复聚合安全，不能把当前thread-local allocation counters当成全进程allocation。

crypto::scalarCommit是两个基的msm，是size2 bins的大来源；wideMSM直接调用G1::mulVec。MCL当前源码mulVec先尝试已安装GLV hook，失败再mulVecN；mulVecGLVT按n分small/large，size1还可能用单点路径。当前归档提供编译配置/依赖SHA，但无每次内部branch trace，所以不能以泛称Pippenger替代实际路径，也不能把2项wrapper都称为2次独立标量乘。

原bins为1、2、3-8、9-64、65-1024、>1024。用户要求的1和2-8可精确合并，其余边界穿过原bins；MSM_PROFILE.csv同时保留原bins及请求bins，缺失项明确NOT_MEASURED。wrapper只有CLOCK_PROCESS_CPUTIME_ID，没有wall，平均CPU latency=bin CPU/calls，不能命名平均wall latency；百分比仅占同wrapper CPU。Full12 size2 CPU P65.8595/V17.6554秒，>1024 CPU P92.2821/V62.4575秒。不能跨census与wrapper再次相加。

直接群运算单列：IPA generator folding（见A3报告）、U/L/R更新、knowledge和final equation中的H/G/commitment标量乘、single::commit每行H*blind和point addition，均不属于MSMTrace bin。point serialization/deserialize、isValid/isValidOrder/curve checks走crypto.cpp:9-30，暂无独立timer/census。群运算wall总占比未知；把P166.488 CPU秒除6107.571 wall得到“2.7%总MSM”是错误口径。
