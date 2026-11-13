# 内存与 I/O

Full12矩阵32768×32768，physical actual714184720、padding359557104。该shape是commitment寻址域，不是源码实际分配了完整2^30个Fr的dense矩阵；commit/readRow按行拼装。逻辑域2^31也未materialize。sizeof(Fr)=32由运行指标及归档类型配置确认。

|指标|Full12 P|Full12 V|性质|
|---|---:|---:|---|
|peak RSS KiB|52054100|6228680|getrusage/进程监测实测|
|约GiB|49.643|5.940|RSS单位换算，不是新测量|
|seal阶段结束RSS KiB|35983848|5538932|阶段末statm|
|U source字节累计|708536253952|无U访问|代码在每round按span.stored×32累计，非PMU实际读流量|
|K API read bytes|36775662720|无K cache|pread成功返回字节累计|
|K API write bytes|18387827424|无K cache|pwrite成功返回字节累计|
|K I/O累计wall秒|55.3904|不适用|transfer/evict/advice计时子集|
|FS proof file I/O秒|0.640318|0.106977|FS buffer API计时，嵌套于process/phases|

P峰值出现在all-classified-roots-prechallenge-seal阶段：其累计peak从29444156涨到52054100KiB，阶段末降为35983848KiB。源码single_commitments.cpp把prepared helper重新pack为source values，然后清空prepared；witness/model/packed source/临时buffer可能共存，但没有allocation timeline，不能把峰值全部归因到某一对象。RSS包含resident而非虚拟地址数，file page cache通常不等于进程RSS。

single_hyrax.cpp:52 readRow每行out.assign(columns,0)，扫描runs并拼接私有值；commit worker循环每个物理row（含未触及tail行）都有该清零，只有touched row做分块MSM。4workers scalar scratch bound4194304字节，G1scratch另计；这是源码边界而非整个commit额外内存实测。fold每term重新建witness列buffer和row scratch，并按left遍历相同source；carry/edge可能重叠读同物理行。preparation63.682秒包含这种工作以及rowCombination MSM，不能单独算memory copy耗时。

u_streaming.cpp:56-109每round建一个≤4worker pool、shared低位weights和tile，直接读sealed spans；每round重新访问整个U，而不是把庞大private folded input复制进cache。31round记录P U_scan652.3055 wall/1190.8485 CPU秒。日志U_bytes_read是span公式计数，scan包含Fr乘加、线程调度join和padding处理，不可将708.536GB/652.3055直接标为实测DRAM bandwidth；cache命中、Fr临时读写、低位weights流量都没有硬件测量。

PublicK缓存512MiB/4GiB和mapping-plan spill782724040字节为不同系统。K初建向量最大512MiB；K累计kernel cpp_allocated_bytes=60733587456（115个kernel traces，P），V23152623616（45个），这是tracing thread内ordinary new请求的累计字节，不是峰值存活量或malloc实际承诺量。K_RAM_peak1010259296含builder临时量，K_file_peak4294967296；U_TOTAL_EXTRA_RAM_BYTES是源码预检上界，不是RSS分配测量。

hyrax_a3.cpp:37复制bases/aa/x，每round新建左右标量和point向量，形成分配/复制候选；当前无字节实际copy profiler。MSM allocation bins为0仅覆盖trace生命周期ordinary C++ new，排除caller construction、worker、C malloc、aligned allocation，不表示全程零分配。

环境固定affinity=[0,1,2,3]、ZKGPT_THREADS=4、U_THREADS=4、SINGLE_COMMIT_THREADS=4，Full12启动load约97，Oneblock环境原件另存。没有perworker NUMA placement、page locality、cache miss或DRAM计数。当前没有证据判断NUMA主因、swap主因或memory-bandwidth上限。I/O bytes是API成功transfer，可能经page cache，不能叫NVMe/磁盘物理字节。FS file time小也不能排除K/cache或mapping I/O开销。
