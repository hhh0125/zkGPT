# PERF6 archive and restore map

Windows authority remains C:/Users/tanyt/Desktop/program/zkGPT-R7-dev. Linux sole project remains /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work. No historical source/evidence or original repository was deleted/reset. Do not automatically overwrite an existing worktree during recovery.

FINAL_SOURCE.zip contains the 520-member manifest-defined Windows source tree, including existing uncommitted source plus PERF6 changes. FINAL_SOURCE_SHA256.json defines each file and canonical tree hash. TASK_ONLY.patch isolates PERF6 from the preserved baseline; BASELINE_SOURCE.zip and PREEXISTING_DIFF.patch retain the earlier bytes. All source members were physically restored and reread in FINAL_SOURCE-restored.

A2_STAGE.zip, B2_STAGE.zip and C2_STAGE.zip are independent chronological stage snapshots, each with source/build-input bytes, dependency bytes, binary/configuration identities, raw proofs/profiles/negative logs and a per-member archive manifest. Each has a matching *_ARCHIVE_VERIFICATION.json and actual *-restored directory. C2 includes the additional 126-call distribution test. Failed samples/builds are retained.

FINAL_STAGE.zip contains final Linux build-input bytes, archived dependencies and runtime MCL libraries, key binaries, build configuration, proof/statement/FS data and complete raw stage evidence. FINAL_ARCHIVE.json supplies container/member hashes; FINAL_ARCHIVE_VERIFICATION.json records CRC, every SHA and physical Windows restore-read. Final closure reports and the verification receipt are also packaged separately so validation of a container does not depend on a claim embedded inside that same container.

Unchanged model/checkpoint/Exp parameter files are identified by all 120 hashes in the stage identity and remain under the existing validated retention storage. This stage does not duplicate their large bytes and is not represented as a standalone Full12 model restore. Bounds/native fixtures use their own explicit synthetic input seed (NATIVE_SYNTHETIC_INPUT.bin); no private Full12 witness is present. The deterministic fixture masks provide no confidentiality for those synthetic proofs.

Recover files only after verifying archive SHA, membership and CRC; use the included restore_archive.py for the stage containers. Rebuild from source with the recorded GCC/CMake/Release flags and archived dependencies, never treat an old CMake cache as relocatable. Source synchronization must originate in the Windows authority and be guarded by expected previous hashes. The live default remains generic; all three new options and legacy B remain OFF unless a future separately accepted profile explicitly selects them.

SSH configuration, credentials and private keys are outside every archive. Remote copies are retained; no disposal or cleanup is authorized. Actual Full12 still requires a new passing admission and separate explicit authorization.
