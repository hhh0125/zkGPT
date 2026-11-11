# PERF6 archive and restore map

Windows authority remains C:/Users/tanyt/Desktop/program/zkGPT-R7-dev. Linux sole project remains /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work. No historical source/evidence or original repository was deleted/reset. Recovery must not overwrite an existing worktree automatically.

DELIVERY_SOURCE.zip contains the current 521-member Windows source tree, including preexisting uncommitted source and PERF6 changes. DELIVERY_SOURCE_SHA256.json defines every SHA and canonical tree hash. DELIVERY_TASK_ONLY.patch isolates this task from BASELINE_SOURCE.zip; PREEXISTING_DIFF.patch retains the earlier changes. All source members were physically restored and reread in DELIVERY_SOURCE-restored. FINAL_SOURCE.zip and FINAL_STAGE.zip are retained earlier snapshots before the test-only exact-byte recorder, not the current delivery source.

A2_STAGE.zip, B2_STAGE.zip and C2_STAGE.zip are independent chronological stage snapshots. Each includes source/build-input bytes, dependencies, binary/configuration identities, proofs, profiles and negative logs. Each matching *_ARCHIVE_VERIFICATION.json records SHA, CRC and actual Windows restore-read. C2 includes the supplemental 126-call distribution. Failed builds/checkers/samples are retained.

CLOSURE_STAGE.zip is the final Linux source/build-input and evidence snapshot, including actual event/Challenge binary streams, raw stage data, synthetic proof input, runtime MCL libraries, production/test binaries, build configuration and windows-closure report snapshot. CLOSURE_ARCHIVE.json defines container/member hashes. CLOSURE_ARCHIVE_VERIFICATION.json is the external SHA/CRC/every-member/physical-restore receipt. The reports inside this evidence snapshot may still say pending closure: the current Windows FINAL_RESULT.json and final DELIVERY.zip reports record the subsequently verified closure without depending on a self-contained claim about their own container.

DELIVERY.zip packages current reports/scripts/patches/manifests, source and all stage archive containers. DELIVERY_MANIFEST.json defines its members; DELIVERY_VERIFICATION.json is the external container/CRC/SHA/physical-restore receipt. Its extracted archive containers can be restored independently with restore_archive.py. Existing *-restored directories are verification products and are excluded from DELIVERY.zip.

All 120 unchanged model/checkpoint/Exp parameter identities are retained in stage manifests and existing validated retention storage. Their large bytes are not duplicated here, so this is not a standalone Full12 model restore. Native uses an explicit synthetic input seed; no private Full12 witness is present. Public deterministic test masks provide no confidentiality for these synthetic proofs.

Verify archive SHA, membership and CRC before restoring. Rebuild using the recorded GCC/CMake/Release flags and archived dependencies; old CMake caches are not relocatable. Source sync must originate from Windows and check prior hashes. All three new performance options and legacy B remain OFF.

SSH configuration, credentials and private keys are outside every archive. Remote copies are retained; no deletion is authorized. Actual Full12 requires a future passing admission and separate explicit authorization.
