# Fixed zkGPT workspace

All zkGPT development, builds, witness construction, GKR/production Lasso proof generation, independent verification and resource measurements take place in this directory:
`/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`.

Before starting the next stage, check `reports/migration-20261020/MIGRATION_RESULT.json` has `success: true`. A missing/failed result means migration is incomplete; preserve old R5A and diagnose migration first.

Do not create new sibling full project snapshots for R5B/R6. Any exceptional full copy needs a concrete technical reason and explicit user approval. Never delete/reset/clean the original `/home/yttan/project/zkGPT/zkGPT-main/zkGPT` or modify historical R3/R4/R5A evidence.

Source and data are copied byte-for-byte from the validated R5A. Preserve host safety fixes, GKR, production Lasso, unified input Sumcheck, independent verifier, declared partial profile semantics, checkpoints, parameter bytes and Exp tables. This migration does not authorize B3-R5B, numerical changes or full12. The blocks12 guard remains.

The Git repository discovered in an ancestor is not a content identity for this uncommitted snapshot. Use WORK_IDENTITY and per-file source/parameter hashes; never run reset/clean against the ancestor repository. Stage archives contain full source bytes.

Build in `build/release`, using the local independent `deps/cnpy` installation. Do not use binaries/caches from old snapshot or evidence directories. Put stage results in unique `reports/<stage-id>` and `logs/<stage-id>` subdirectories, with commands, toolchain, source/binary/parameter SHA, proof/statement and original verifier/negative logs. Never overwrite a historical run.

Long-term archives are on Windows under `C:/Users/tanyt/Desktop/program/zkGPT-backups`. Verify container SHA, every file SHA, gzip integrity and restore-read results before cleaning unique remote data. Keep the current successful build and unresolved failure evidence. Full retention rules are in RETENTION_POLICY.md. Any deletion is a separate explicitly approved action; this migration deletes nothing.

Fresh build commands:

```bash
cd /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -DCNPY_INCLUDE_DIR="$PWD/deps/cnpy/include" -DCNPY_LIBRARY="$PWD/deps/cnpy/lib/libcnpy.a" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/release -j8
ctest --test-dir build/release --output-on-failure -j4
```

## Directory-only task validation scope (user instruction)

Backup, archiving, migration and directory cleanup do not authorize automatic long cryptographic computations or protocol acceptance. Check file SHA/counts/structure/recoverability, R5A source identity and build, symlinks/hardlinks, build configuration, parameter paths and runtime dependencies. Reuse existing R5A 43/43 and oneblock independent-verification evidence. Do not start new oneblock/full12 proofs, malicious proofs or other long computations for directory-only work. Only actual modifications to proof code, numerical constraints or verification flow justify corresponding verification, scaled to the change and user authorization. Stop before any deletion and await explicit approval of the exact cleanup list.

## Authorized cleanup completed

On 2026-10-20 the user explicitly approved the six historical R3/R4/R5A source and evidence directories in REMOTE_CLEANUP_PLAN.md. They have been deleted after backup/hash/path/reference checks. The original zkGPT Git main repository and its shared data remain protected. Restore history from the verified Windows archives; do not expect the old remote paths to exist. See reports/cleanup-20261020/REMOTE_CLEANUP_RESULT.json. No new crypto computation was run for cleanup.
