# R7 category roots and whole-row MSM validation

Base: GitHub `hhh0125/zkGPT` main, `d02ec98a00ba24b3e867984064d7fc0623211eb6` (confirmed again before delivery on 2026-11-24).

Each nonempty category now owns exactly one storage root. Category actual counts, logical padding, category order and global C_U semantics are preserved. Empty categories do not allocate roots. The category API no longer accepts a storage cap. Each storage root uses `category_offset=0`; base runs and helper pieces refer directly to that root. Helpers retain all original stored padding, including nonzero values.

`Coordinate.index` remains uint32_t. Appending storage checks both the size_t addition and the inclusive uint32_t index range before any cast. Coordinate offset calculations also validate the root, root extent and uint32_t addition. The index representation can address 2^32 elements (indices 0 through UINT32_MAX); the existing master, global domain and padding bounds still apply independently. No category storage cap of 2^24 remains. Legacy non-category slab/partition APIs retain their separate behavior.

Every Hyrax row, including zero and padding rows, executes one `G1::mulVec(value, bases.data(), row.data(), p.columns)`. Every row retains an independent blinding scalar and the same Pedersen formula. Bases are normalized once before launching workers: the bundled MCL fallback can normalize its input in place, but skips writes to points already normalized. No per-worker generator copy or 1024-term application loop remains. MCL's internal algorithms are unchanged. MSM counters record actual whole-row calls and terms.

`categoryTiles()` and `categoryKernelInto()` are unchanged: the computation tile limit remains 2^20 elements. Opening, IPA, Sumcheck and proof encoding are unchanged.

## Compatibility

Storage root metadata and coordinate pieces are already included in `categoryIdentity()`. Layouts previously split by the old cap therefore have a different identity, which is also bound by the Single Hyrax descriptor and existing model/proof bindings. Such artifacts must be regenerated using the new canonical layout; accepting an old identity is not enabled. Layouts that previously needed no splitting retain the same root identifiers and metadata structure. Row MSM evaluation alone does not change any commitment point for fixed inputs and blinds.

## Build and tests

GCC 12.3.0, CMake 4.1.2, Release (`-O3 -DNDEBUG`), local independent cnpy installation. Fresh `build/release` in the isolated Git worktree. All default production targets and enabled research/test targets compiled successfully, including `r7_fs_full12_prover` and `r7_fs_full12_verifier`.

Configuration enabled `ZKGPT_SINGLE_IDENTITY_TESTS`, `ZKGPT_CATEGORY_FROZEN_TESTS`, `ZKGPT_LAYOUT_VALIDATION_TESTS`, `ZKGPT_KIND01_TESTS`, `ZKGPT_MASKED_TESTS`, `ZKGPT_NATIVE_MASKED_TESTS` and `ZKGPT_PUBLIC_K_TESTS`.

Validation command: `ctest --test-dir build/release --output-on-failure -j 4`.

- New layout regression covers category sizes 8, 2^24 and 2^24+3, including five categories simultaneously, unsorted W source spans, category boundaries, helper offsets, virtual padding, root/index overflow rejection and empty categories. Large cases construct metadata without allocating a full witness. Computation tiles are checked for exact coverage and the preserved 2^20 limit.
- New MSM regression uses 2048 columns and deterministic field-valued witness data. Every row equals an independent copy of the previous 1024-term algorithm with the same generators and blinding scalar. Serial/four-worker equality, zero source rows, padding rows, counter values and exception propagation are checked.
- New independent P/V fixture constructs the layout through the new category API, reads two helpers (including nonzero stored padding), authenticates a mapped view spanning X/W, completes the existing unified input protocol and rejects a corrupted proof. Existing identity, layout, frozen mapping and masked protocol suites also ran.
- Final suite: 26/27 passed. The extra `r7_public_k_mmap` test fails with `FAIL expected rejection did not happen`. Rebuilding that test and all its dependencies from an exact archive of unmodified main reproduces the identical failure (exit 1). Its source and checks were not changed or disabled. This remains an existing baseline failure, not a claimed pass.

No full model Full12 witness/proof generation was run. Production executables were built; protocol acceptance evidence here is from independent synthetic P/V fixtures. The repository's existing cryptographic design/production gate is unchanged.

## Single-run resource comparison

Both executables use the same benchmark object, fixed RNG, witness, bundled MCL and Release flags. The baseline executable links the unmodified main `single_hyrax.cpp` object before the current library; the updated executable uses the new implementation. Witness scalars are deterministically hashed field elements. Each run is a separate process measured with `/usr/bin/time -v`.

Matrix: 2048 rows, 4096 columns, domain 8,388,608; actual values 2,097,156. This fixture has substantial logical/padding space. One measurement per configuration, without statistical or production performance claims.

| Workers | Old commit seconds | New commit seconds | Old peak RSS KiB | New peak RSS KiB |
| --- | ---: | ---: | ---: | ---: |
| 1 | 22.7073 | 21.6469 | 9072 | 9864 |
| 4 | 6.81238 | 5.91567 | 10880 | 13796 |

Old counters: 2052 MSM calls, 2,101,248 terms, maximum MSM 1024. New counters: 2048 MSM calls, 8,388,608 terms, maximum MSM 4096. The increase in counted terms is expected because padding-only rows now also execute exactly one whole-row MSM. Both worker configurations and both implementations produce the same SHA256 of all serialized row points:

`3bf11c53ef6eadc75496cd4a64f24ca1dfba99bf105178eaf8b19d5cff42a06a`

`SINGLE_HYRAX_COMMIT` reports a row scalar scratch bound of 131072 bytes with one worker and 524288 bytes with four workers in this fixture. This field only bounds row scalar buffers, not MCL's internal workspace or total process RSS. The measured RSS above includes the whole process. Production-size peak RSS was not measured.

## Local evidence

The isolated worktree is under the fixed workspace at `r7-single-root-main`. Original build/test logs are in `logs/r7-single-root-20261124/`. Evidence is in `reports/r7-single-root-20261124/`: `before-{1,4}.log`, `after-{1,4}.log`, matching `.time` files, `baseline-public-k.log`, `generated-protocol/` (proof and original P/V logs), and `SHA256.json` (source, binary and proof hashes). Baseline source/build artifacts are retained locally for reproducibility and are not part of the code commit.
