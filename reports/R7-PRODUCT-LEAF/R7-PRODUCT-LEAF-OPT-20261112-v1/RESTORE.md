# R7 Product leaf-sharing restoration

The current active Linux project remains `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work` on the existing `4090_sub2api` host. No remote copy is disposable and no deletion is authorized by these archives.

Artifacts:

- `R7-PRODUCT-LEAF-OPT-20261112-v1.tar.gz`: complete current source bytes, MCL source, cnpy install, public trust files, exact statement/proof, raw success/failure/negative-test and resource logs, build/parameter/source identities, candidate and baseline binaries, overwritten-file backups.
- `R7-PRODUCT-LEAF-OPT-20261112-v1-local-evidence.tar.gz`: local patch, baseline identity, task/plan, directory relocation audit, independent-review summary, report and local helper scripts.
- `LOCAL_ARCHIVE_VERIFICATION.json`: archive SHA, member verification, gzip CRC and restoration/readback status. `REMOTE_ARCHIVE_RECEIPT.json` records the Linux container SHA and manifest identity.

Verify both container hashes first. Extract on a recovery host into a user-approved empty location; never overwrite the live workspace or historical data as an implicit restore step. On the current server, changing the fixed workspace or creating a parallel full source copy still needs explicit user approval.

The runtime project is the extracted `source/` tree. Copy the archived `runtime-public/reports/` files to that project's `reports/` with identical relative paths; check their SHA against `evidence/ARCHIVE_MANIFEST.json`. The new categorized report paths are intentional. The exact Full12 statement is `evidence/full12-B-shared-leaf-v2/STATEMENT.bin`; retain its source and parameter pins. Archived MCL shared-library bytes are under `evidence/runtime-libraries/release` and `asan`; if replaying archived binaries, select the matching library directory explicitly rather than mixing sanitized and release libraries.

Large unchanged model parameter arrays are identified by `evidence/PARAMETERS.json` and are **not duplicated** in this stage archive. Use the already retained original model data/parameter archives, restore to `data/...`, and verify every map entry before running. Do not substitute checkpoints, regenerate Exp tables, change numerical profiles, or trust filenames alone. This archive is not an offline OS or full-model-data image.

Reconfigure a fresh build under the approved fixed project rather than reusing an archived CMakeCache:

```bash
cmake -S . -B build/product-leaf-recovery \
  -DCMAKE_BUILD_TYPE=Release -DZKGPT_BUILD_R7_FRAMEWORK=ON \
  -DCNPY_INCLUDE_DIR=/approved/dependency/cnpy/include \
  -DCNPY_LIBRARY=/approved/dependency/cnpy/lib/libcnpy.a
cmake --build build/product-leaf-recovery -j4
ctest --test-dir build/product-leaf-recovery --output-on-failure -j4
```

Match the archived compiler/CMake/GMP/zlib/OpenSSL identities where needed; system shared libraries are identified, not supplied as a full OS image. MCL is rebuilt from archived source. The original build had 120/121 CTest passes: `r7_zero_suffix_storage` failed its allocation-budget assertion in both the candidate and retained Release binaries. Do not silently raise that budget or report an entirely green original suite.

Tests and measurements were authored locally and uploaded byte-for-byte. The test-only frozen `product_leaf_reference.inc` is the actual post-Weights Optimization lookup implementation, SHA256 `1ddb3eb9bc456d09e4f812216cc365c5a7c8b11e1be50eb468e35e5a32d1e49e`. It is not the earlier pre-weight GitHub baseline.

Ordinary restoration/archiving does not authorize new long cryptographic runs. Reuse the archived acceptance evidence unless proof code or verification flow is changed and the user authorizes corresponding validation. No SSH config, private key, or credential file is part of the archives.
