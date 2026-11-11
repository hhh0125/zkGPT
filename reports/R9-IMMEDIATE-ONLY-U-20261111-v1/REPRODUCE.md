# R9 复现与证据

固定工作目录 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`。

构建命令：

```bash
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -DZKGPT_BUILD_R7_FRAMEWORK=ON -DCNPY_INCLUDE_DIR="$PWD/deps/cnpy/include" -DCNPY_LIBRARY="$PWD/deps/cnpy/lib/libcnpy.a" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/release -j8
ctest --test-dir build/release --output-on-failure -R 'r7_(perf4_u_state|perf3_u_equivalence|category_mle_fold|category_cache|u_measurement|single_hyrax|category_layout|category_crypto|category_k_|unified_input|opt3d_streaming|opt3c_folding|carry_terminal|source_mle)|r5b_blocks12_guard'
python3 reports/R9-IMMEDIATE-ONLY-U-20261111-v1/compile_lifecycle.py
```

run_experiments.py 保存完整串行 P/V 命令、FD 传递、环境、参数和源码验证、CPU affinity、wait4 RSS/CPU/I/O。已经执行过的目录禁止重跑覆盖。复现实验时需要新的唯一报告和日志目录并保存实际 R8 对照二进制身份。R9 进程不设置已移除的 U 模式和预算环境变量；R8 对照显式 Immediate。ENVIRONMENT.json 已按 run() 实际设置校正（原元数据误将 R8/R9 侧标签写成 runtime mode，该问题只影响记录，不影响执行）。

RAW_METRICS 与 AB_STATISTICS 由 analyze.py 从原始 RESULT/日志生成。U 专项 = scan+weights+capture+fold；fold 含 materialization 和 shrink/unmap。unmap_wall_seconds 含 shrinkTo 的析构和计量开销，并非纯内核 syscall 计时。

FrBuffer 内部类直接测试放在报告目录，不增加生产入口；包含 move、自 move、溢出、零长度和 mincore 对已 unmap 尾部/最终 mapping 的 ENOMEM 检查，代码和构建输出均保留。perf4 链接器 wrap mmap/munmap 验证真实调用、失败注入和所有状态映射清理。固定 RNG 的 bounded-R8/R9 proof 逐字节相同，两个独立 Verifier 接受。

SOURCE_BEFORE/SOURCE_AFTER/FROZEN_SOURCE/PARAMETERS/BINARY_SHA256 保存内容身份。祖先 Git 中此目录未跟踪，所以 GIT_DIFF.txt 为空；可审查差异为 R9-IMMEDIATE-ONLY-U.patch。未创建工作分支、commit、push、reset、clean 或 stash。remote-baseline.git 是未下载对象的空证据仓库（无分支 refs），下载许可被拒绝；不作为基线文件内容证据。

production_gate=BLOCKED_CRYPTOGRAPHIC_DESIGN
may_enter_stage2=false
