# OPT2 Windows archive / restore

Final source HEAD: bce054ddb719982e18cbb2a726f08cc239720c91.
Start: 2026-10-28; completed: 2026-10-29 Asia/Shanghai.

evidence.tar.gz contains 400 regular members including ARCHIVE_MANIFEST.json.
LOCAL_VERIFICATION.json records container SHA, 399 data-member SHA checks,
gzip CRC, full restore-read checks, and all 371 final source-byte checks.
Every data member is restored as restore-cas/<sha256>; ARCHIVE_MANIFEST.json
maps its original relative name, bytes and Linux mode to that SHA.

Convenient independently hash-checked copies are full-opt2.fsproof,
PUBLIC_STATEMENT.bin, final-source.tar.gz and WINDOWS_SOURCE_CHANGE.patch.
The proof SHA is f65c7d28d7266f67630bed816d14314b33b7ece09312b8c6d91430596854f758.
final-source.tar.gz contains all 371 frozen source/plan files without links.
Recover into a new empty destination for inspection, never overwrite existing
source or the protected original zkGPT repository. To rebuild, use the fixed
Linux workspace and archived build/link configuration with compatible tools;
MCL dependency source remains in the verified historical migration/R5A archive.
The exact linked dependency bytes and their original paths are in this stage's
BUILD_DEPENDENCY_IDENTITY.json and dependency-bytes members. Restore Linux
executable permissions from the archive/manifest when using saved binaries.

Parameter identity covers all 120 data/gpt2_int files plus the unchanged
RowMajor anchor, Manifest and exporter. Historical validated parameter bytes
remain in Windows migration/stage archives; this task changed none of them.
No private witness, SSH configuration or private key is included.

The three final report files and final-report-generator.py were generated
after the frozen source commit and after archive verification. Their SHA
values are covered by DELIVERY_MANIFEST.json, separately from evidence.tar.gz.
Raw build/test/proof/negative logs and the complete stage runner are preserved.
Remote artifacts remain retained. This delivery authorizes no deletion and
does not request another proof run. FS_NIZK_CANDIDATE /
BLOCKED_CRYPTOGRAPHIC_DESIGN / may_enter_stage2=false remains unchanged.
