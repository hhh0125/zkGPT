# R8 build/test and experiment evidence

Run from /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work.

```bash
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -DZKGPT_BUILD_R7_FRAMEWORK=ON -DCNPY_INCLUDE_DIR="$PWD/deps/cnpy/include" -DCNPY_LIBRARY="$PWD/deps/cnpy/lib/libcnpy.a"
cmake --build build/release -j8
ctest --test-dir build/release --output-on-failure -R 'r8_u_immediate|r8_u_transcript|r7_perf4_u_state|r7_category_cache|r7_u_measurement|r7_perf3_u_equivalence|r7_category_mle_fold'
```

Additional related tests and the blocks12 rejection test were run; see ctest-final.log and ctest-blocks12-guard.log in the R8 logs directory.

Default configuration remains deferred. Experiments used ZKGPT_CATEGORY_U_FOLD_MODE=deferred or immediate, with ZKGPT_CATEGORY_U_RAM_BYTES=536870912 and ZKGPT_CATEGORY_U_IMMEDIATE_RAM_BYTES=17179869184. Auto uses the same guarded immediate admission policy. Both explicit immediate and auto fall back to deferred on insufficient budget/headroom.

run_experiments.py contains the exact serial A1/B1/A2/B2/A3/B3 and one Full12 P/V commands, statement FD transport, CPU affinity, environment and checks. It refuses to overwrite/restart the completed experiment directory. ENVIRONMENT.json and P.json/V.json under each run contain actual environment, binary hashes, command, elapsed time and resource data. analyze.py creates tables from RESULT.json without rerunning proofs.

R8-U-FOLD.patch is a snapshot diff against baseline/; ancestor-git-diff.txt is empty because the workspace is untracked in the ancestor Git repository. The patch passed git apply --check --reverse against the completed workspace. Do not use the ancestor HEAD as source identity.

FULL12_BUDGET_CALCULATION.json derives U1 sizes from actual Round-0 root telemetry. U_SEGMENT_SHAPES_CALCULATED.tsv is calculated from those public padded lengths and checked against runtime aggregate effective-element counters at every round; its per-Segment shape rows are not separate physical-memory measurements.

Source/parameters/binaries were hash-checked before/after runs. Individual memory-preflight snapshots were not logged; memory.current is used internally for admission, and the runner records environment snapshots and a shared cgroup sampled peak. memory.peak was unavailable. /proc/io observations may be early partial samples and are not final physical-I/O measurements.

production_gate=BLOCKED_CRYPTOGRAPHIC_DESIGN
may_enter_stage2=false
