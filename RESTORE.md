# zkGPT historical archives and restore procedure

Archive root: `C:\Users\tanyt\Desktop\program\zkGPT-backups\migration-20261020`.
Use `LOCAL_VERIFICATION.json` for completed checks; absence of a stage means not verified.

Each `archives/B3-R*.tar.gz` contains a complete `source/` tree and the entire corresponding `evidence/` tree, including build configuration, binaries, original proof/statement, logs, reports and parameter identity records. R3/R4 data links and all library links are materialized into ordinary file bytes. Hardlinks are not required for restoration. `.git` directories/pointers are intentionally excluded: source recovery does not require Git. The original Git repository was never reset, cleaned, staged or committed. Git identities/status and the original source diff are in `GIT_STATE_BEFORE.json` and `MAIN_UNCOMMITTED_SOURCE.patch`. Actual stage differences are in `R3-to-R4.patch`, `R4-to-R5A.patch`, plus the original stage evidence patches. These are supplemental; full source bytes are authoritative.

`DEPENDENCIES.tar.gz` contains the external cnpy install, including headers, static and shared libraries. MCL source is included separately in every stage's `source/3rd`. System/toolchain package versions are recorded in `TOOLCHAIN.json`; install matching Linux C++/CMake/GMP/zlib dependencies on a replacement host. This is source/project recovery, not an OS image. The target OS/toolchain and package installation are prerequisites, not references to old snapshot directories.

For Linux recovery, choose a NEW empty destination (never overwrite existing work), verify the archive SHA against `ARCHIVES.json`, then:

```bash
mkdir -p /chosen/new-empty-restore
tar -xzf B3-R5A.tar.gz -C /chosen/new-empty-restore
tar -xzf DEPENDENCIES.tar.gz -C /chosen/new-empty-restore
cmake -S /chosen/new-empty-restore/source -B /chosen/new-empty-restore/build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCNPY_INCLUDE_DIR=/chosen/new-empty-restore/cnpy/include \
  -DCNPY_LIBRARY=/chosen/new-empty-restore/cnpy/lib/libcnpy.a
cmake --build /chosen/new-empty-restore/build -j8
ctest --test-dir /chosen/new-empty-restore/build --output-on-failure -j4
```

Do not reuse archived CMake caches or old binaries as the new build. Old absolute paths inside historical logs and build metadata are provenance; they do not form dependencies for a fresh build. Use the selected stage's `source/data` for model parameters; do not substitute checkpoints or regenerate Exp tables. R3, R4 and R5A are individually complete and do not need a previous stage to restore. R4/R5A preserve their original partial numerical profiles; archived R3 full12 artifacts are historical evidence, never R5A validation.

All regular files are also restored locally into `restore-cas/<sha256>`, with their exact Linux path, mode and timestamps recorded in each `*.manifest.json`. This avoids Windows filename/encoding limits and deduplicates identical data. The tar archives preserve Linux names and modes for native restoration. Local verification compares every decompressed member with the remote file manifest, verifies restored CAS bytes, enforces complete membership, rejects archive links, and consumes the gzip stream through its CRC footer.

Never archive SSH configs, SSH private keys, API tokens or environment credential files. Included bytes were screened for private-key/provider-token signatures and secret filenames; see `SECRET_SCREENING.json` for scope and limitations. SSH connection configuration is intentionally outside the archives.
