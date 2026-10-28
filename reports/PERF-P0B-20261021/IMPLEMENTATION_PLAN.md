# P0-B Mapped-input Kernel Implementation Plan
> For agentic workers: use executing-plans inline; one fresh-context whole-diff review after implementation. No implementation delegation.

Goal: exact-field acceleration of verifier kernelValue with unchanged proof/transcript/relation.
Architecture: private immutable-by-interface equality split tables,one master per invocation andone currentchild/claim; <=2MiB tables total for dimensions<=28 plus O(d) local state. Singleton product lookup; short contiguous enumeration; bounded aligned dyadic decomposition; original DP for all unsupported/invalid internal ranges. No global cache or persisted validated state. Bias and tail code remain unchanged.
Tech:C++17,existingMCL/CMake,Python bounded supervisor.
Spec:SPEC.md. Global constraints:fixedzkGPT-work,4CPU/test8GiB/32mem/64disk,realoneblockNOTAUTHORIZED,full12/P0B-beyond-kernel/permit/history/G1 changes forbidden. No sibling/worktree/git mutations; report stage and source snapshots are content-identity evidence.
Review focus: dimension0/28 and shiftbounds; zero/one/random points with no division; repeated indices and full/padded domains; cache ownership/mutation/lifetime; count/offset/ordering preservation; fallback anti-degeneration.

## Task1 Freeze and baseline
- [x] VerifyP0A2961... identity,source/binary/deps/parameter/history manifests and archive.
- [x] Exportonlypublic51-terminal layout preservingFCONNduplicates,hash orderedspans.
- [x] Measureunchanged P0AkernelValue alone on frozenstatement+layout with deterministic full-fieldpoints; no completeprover/verifier/witness. SaveCPU/wall/RSS/result.
## Task2 RED -> minimal optimization
Files:src/paper_r1/mapped_input.cpp,tests_mapped_kernel.cpp,CMakeLists.txt only.
- [x] Small DP/dense differential,full kernel+bias/padding/zero-tail,range rejection/cachelifecycle/randomdimensions.
- [x] Baseline mathematical tests pass; fastpath-hit assertion RED before implementing.
- [x] PreserveDP body; implement scoped Eq tables and singleton/contiguous/fallback; same loops/claims/weights/order/final equation.
- [x] GREEN exact equality; on incompatibility stop and retain evidence.
## Task3 bounded validation and independent review
- [x] Release focused and relevant existing regression excludingfull12/multiblockschema.
- [x] TargetedASan/UBSan on modified TU plus small protocol/codec fixtures.
- [x] Frozen/newsmall normal+malicious proof compatibility, byte equality and FS equality.
- [x] Sameisolated benchmark,samepoints/layout;actual dispatch hits/memory cap; not E2E.
- [x] Onefreshread-onlyreview,resolve findings without relaxedconstraints.
## Task4 identities/report/archive
- [x] Rehash fullsource/binaries,proveG1/params/claims/wire/historyunchanged; oldscope notinherited.
- [x] Mark newrealoneblock wall/CPU/RSS/crossverify/bytes pending explicit authorization,notPASS/notmeasured.
- [x] Archive sourcepatch/tests/rawmeasurements/deps/config/proof/statement toWindows,SHA+restore-read. StopbeforeP1.

Ruling:user supplied detailed approved implementation scope; internalalgorithm choices are routine authorized decisions, not grounds to ask for another design approval. Separate realoneblock authorization remains required. No plan commit/worktree/delete actions because fixed-workspace/history rules take precedence.

Task4 archive prepared; final Windows SHA and restore-read receipt remains outside the archive. Realoneblock acceptance explicitly pending authorization, not marked complete.
