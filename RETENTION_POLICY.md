# Fixed workspace and retention policy

Only active remote directory: `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`.
Do not create sibling `zkGPT-r5b-*`, `zkGPT-r6-*` or other complete project copies without a demonstrated technical need and explicit user approval. Do not work in or delete `/home/yttan/project/zkGPT/zkGPT-main/zkGPT`.

For backup, archiving, migration or directory cleanup, verify bytes/counts/structure/recovery, build configuration, symbols/links and parameter/artifact paths. Reuse existing protocol-validation evidence. Do not automatically generate new oneblock/full12 proofs, malicious proofs or perform other long cryptographic computations. Only actual changes to proof code, numerical constraints or verification flow justify corresponding scope-based verification, within the user's authorization.

For each stage, develop and validate in the fixed workspace. Save source bytes/patch, canonical source and parameter SHA manifests, toolchain/build configuration, actual commands and return codes, profile/model identity, proof/statement and required cryptographic artifacts, independent verifier logs, negative-test logs, audit JSON/reports and resource measurements under `reports/<stage-id>` and `logs/<stage-id>` in this workspace. Use unique output names; never overwrite historical artifacts.

Archive completed stages to Windows and verify both container and per-file hashes plus restore-read checks before moving to the next stage. Content-addressed parameter/dependency storage is allowed only with an explicit per-stage restore map. A Git commit alone is insufficient for uncommitted source copies.

Keep the current successful Release build and any builds needed to investigate a failure. Historical object files and temporary proof variants may be classified as rebuildable only after source/toolchain and important original evidence are locally verified. Keep final proof/statement, manifest, logs, failures and measurements long term. Never discard a unique remote artifact before its local archive is confirmed. Any deletion is a separate user-approved cleanup operation with an exact path list. This migration performs no deletions, including temporary remote archive containers.
