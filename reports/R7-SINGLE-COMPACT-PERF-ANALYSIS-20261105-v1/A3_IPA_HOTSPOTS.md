# A.3 guard / IPA

源码 hyrax_a3.cpp:20-34 knowledge，37-68 prooflog，72-80 generators，83-89 dotProduct；single_hyrax.cpp:144-164 mapped open。A3_PROFILE.csv 有 aggregate timer和显式NOT_MEASURED内部项；A3_TERM_LEVEL_METRICS.csv 有每term帧维度/round/call/代码导出操作数，没有把总时间平均分配给term。

|scope|P guard wall|P IPA wall|V guard wall|V IPA wall|terms|每项维度/轮数|
|---|---:|---:|---:|---:|---:|---|
|Full12|65.3161|513.015|31.9587|33.8359|148|32768 / 15|
|Oneblock|4.2387|30.6463|1.63497|1.79245|19|8192 / 13|

这两个 timer只从 knowledge 进入计起，到 prooflog 返回结束。dotProduct 前面的 statement/binding/scalar编码、generators retrieval不在guard/core中。生成元在guard外检索，但prooflog内`g=bases`复制算入core。完整mapped opening减 preparation/guard/core 后，Full12 P约8.3929秒、V约9.840924秒是其他opening工作，不是“纯序列化”。Oneblock对应1.9794/2.990117秒。

Knowledge：P创建n个fresh mask scalar，wideMSM(g,d)+Hrv、scalarCommit(dy,ry)，生成挑战后编码n+3 responses；P和V都Reader解码responses；V执行wideMSM(g,z)和两条group equation。每term独立guard没有删除。Full12 guard payload155,212,928字节、core161,024字节；公开a系数另有148×32768×32=155,189,248字节，已包含在proof总大小内，不能再加为额外proof。

IPA P：初始 x/witness和g/aa复制、dot(x,a)；每轮新建xl/xr/al/ar/gl/gr，计算两次cross dot与两个wideMSM，生成挑战及inverse；随后每个i执行`g[i]*inv + g[i+half]*c`，并折叠x/aa与blind。最终构造Schnorr响应。宽度依次32768→1，总half=32767，因此每term生成元折叠65,534次直接G1标量乘、32,767次加法；148项合计9,699,032/4,849,516。左右cross MSM每term30 calls、65,534 terms，合计4440 calls、9,699,032 terms。上述是确切源码路径计数（不含表达式优化或群库内部基域运算），不是耗时分配。

IPA V：不做P的逐步generator fold。重建15个challenge，对n个weights做每轮乘法，wideMSM(bases,weights)一次，再dot(a,weights)，执行最终方程；也进行U+=L*c²+R*c^-²等直接群运算。n×log2(n)权重乘法结构重复，但challenge各term不同，不能缓存最终weights跨term。

generator 已以n为key缓存；`generators()`返回vector副本；公共基相同但每项进入prooflog又复制一次，且V目前也建立g/aa副本（后续独立权重路径使用bases/a）。公共immutable基/unused副本、切片视图、scratch容量复用是低语义风险候选，但MCL mulVec暴露mutable base参数，修改前必须确认借用/别名安全。相同right mask或low公共系数可比较后共享immutable表示，不能共享证明挑战、mask或guard。

Writer::point调用group并encode，Reader::point调用decodePoint，后者再次curve/group/order检查。P也通过exchange读取自己发送的公共点；decodeHandle结束又encode，存在反复编码/校验路径。不能称其“无意义”或自动删去：这些是输入边界约束，改变时要证明可信边界等价，并保留恶意point拒绝。

513.015秒尚无内部采样或子计时。生成元折叠是强结构热点候选，但当前不能声称它占core的某个百分比。每项耗时、inner-product耗时、allocations、group validation独占成本仍NOT_MEASURED。没有新增仪器开销校准数据。
