# zkGPT R7: Single Hyrax + Unified Input + Fiat-Shamir

Windows editing workspace: `C:/Users/tanyt/Desktop/program/zkGPT-R7-dev`.
Linux build/run workspace: `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work` on `4090_sub2api`.

The supported complete FS entries are:

- `r7_fs_oneblock_prover` and `r7_fs_oneblock_verifier`
- `r7_fs_full12_prover` and `r7_fs_full12_verifier`

All four require `R7_CLASSIFIED_V2`, `R7_FS_ONEBLOCK`, `R7_UNIFIED_INPUT1`, and `R7_SINGLE_HYRAX`. Full12 adds `R7_FS_FULL12`; verifiers add `R7_COMPLETE_VERIFIER`.

The controller fixes source-opening handles, calls `authenticateUnifiedInput(s.x)`, and verifies all coverage relations. The production route is Category Input -> Unified Sumcheck -> one Single Hyrax global opening -> Hyrax A.3. The old authentication barrier, Oneblock V1, multi-root production variants and obsolete classified/FS probes are retired.

Configure and build in the fixed Linux workspace:

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -DZKGPT_BUILD_R7_FRAMEWORK=ON -DCNPY_INCLUDE_DIR="$PWD/deps/cnpy/include" -DCNPY_LIBRARY="$PWD/deps/cnpy/lib/libcnpy.a"
cmake --build build/release -j8
ctest --test-dir build/release --output-on-failure -j2
```

The FS executables retain `prove|verify PROOF_PATH CONTEXT` and independent public-statement/coverage file descriptors. Existing model data, parameter identity, proof format and transcript are retained. Component research targets have distinct bounded testing roles; they are not alternative complete production suites. The existing `BLOCKED_CRYPTOGRAPHIC_DESIGN` / `may_enter_stage2=false` gate is unchanged.

Cleanup specification, complete Windows process evidence and validation commands are under `reports/R7-LEGACY-CLEANUP-20261116-v1/`. The final report records exact outcomes, including any pre-existing regression failures. Historical launchers and original source bytes are retained there instead of remaining active tools. Earlier stage documents are historical evidence.
