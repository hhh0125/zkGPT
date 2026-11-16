# R6/R7 Public Build Implementation Plan

> Implement inline using executing-plans. User explicitly authorized local edits, synchronization, builds and tests; no commits, extra remote source trees or approval round.

**Goal:** Build and audit each process's same-purpose R6 public circuit once, sharing its immutable artifacts with all R7 planning consumers and the verifier.

**Architecture:** `r6::PublicCircuit` owns a noncopyable LLM, graph and ModelQueryPlan. Its constructor configures, builds, audits and validates regions once; const accessors and an audited serialization overload avoid mutable audit tokens. R7 builds Context and Coverage from these artifacts; standalone wrappers construct their own safe owner. Entrypoints retain their own owner through graph use. Private witness construction remains separate and untouched.

**Tech Stack:** Existing C++17/CMake/CTest, Python SSH orchestration, fixed Linux directory.

**Spec:** User attachment preserved as `reports/R7-PUBLIC-BUILD-20261113-v1/REQUEST.zh-CN.md`.

## Global Constraints

- Windows edits first, SHA-guarded deployment only to `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`.
- Preserve existing local and remote bytes, no reset/clean/commit/deletion or protocol/model/numeric/CLI changes.
- Keep standalone `writePlan` audit and independently constructed verifier trust.
- Only actual R7 supported configurations 1 and 12; retain unsupported-target rejection.
- Archive stage source/patches/config/dependencies/parameters/proofs/raw logs/measurements to Windows with SHA and restore-read verification.

## Review Focus

- No mutable graph/plan can bypass standalone writer audit.
- Artifact lifetime and noncopyability prevent dangling references or deep graph copies.
- Oneblock and Full12 retain exact plan, objects and layout identities.
- Existing layout/trust standalone overloads retain independent local construction.
- Witness generation and verifier ownership remain independent; no peer imports planning artifacts.

### Task 1: Baseline and regression evidence

- [x] Preserve both original source trees, Git states and SHA manifests; resolve local/remote drift by retaining local original bytes and using active remote source as actual baseline.
- [x] Add temporary function trace and public planning snapshot probe; show regression on duplicate counts for blocks1/12.
- [x] Record three baseline runs for build/audit/context/coverage/startup/RSS and snapshot Native Plan, Coverage, query/object/layout identities.
- [x] Run baseline CTest and retain raw failures unchanged.

### Task 2: Immutable shared public artifacts

Files: `paper_r2/r6_model.hpp/.cpp`, `paper_r7/r6_adapter.hpp/.cpp`, `oneblock_plan.hpp/.cpp`.

- [x] Introduce `r6::PublicCircuit(unsigned)` owning `LLM`, graph and plan; delete copy/move, expose const references; audit and validate once.
- [x] Extract internal serialization; existing `writePlan(o,g,n,p)` still audits, `writePlan(o,PublicCircuit)` uses constructor validation.
- [x] Add `buildR6PublicArtifacts`, Context-from-artifacts and discover-from-artifacts overloads; retain independent wrappers.
- [x] Verify structural trace 1/1/1/1/1 and exact snapshots for both targets; reject malformed pin, unsupported blocks and tampered standalone writer input.

### Task 3: Remaining consumers and entrypoints

Files: `master_layout.hpp/.cpp`, `category_layout.cpp`, `classified_layout.hpp`, `oneblock_model.hpp/.cpp`, `oneblock_main.cpp`, `classified_controller.inc`, `full12_preprocess.cpp`, `perf4_public_layout.cpp`.

- [x] Add artifact-taking X spans, trusted layout, category layout and oneblock trust overloads; retain independent wrappers.
- [x] Entrypoints build artifacts once, discover/context/layout/trust from them, retain graph for verifier; retain private witness execution graph separately.
- [x] Remove duplicate public construction from Full12 preprocessing and public census.
- [x] Test plan/layout/trust compatibility and runtime counts; keep unrelated experimental fixtures unchanged and report their distinct construction purpose.

### Task 4: Full validation and delivery

- [x] Repeat three matched planning measurements and compare exact snapshots.
- [x] Run relevant R6/R7, Full12 integration, independent P/V, invalid proof rejection and complete CTest; distinguish baseline failures.
- [x] Run a new Full12 proof and independent verifier; record public vs witness counts separately.
- [x] Remove temporary source trace, rebuild final production binaries and rerun targeted checks.
- [x] Obtain fresh whole-change review via requesting-code-review, fix material issues, archive and verify all evidence locally.
- [x] Deliver Chinese report with call chains, files/lifetimes/counts/performance/test totals and protocol-boundary statement.

## Ledger

- Ruling: user requires this working tree and fixed remote directory, so do not create a worktree; preserve original bytes instead.
- Ruling: user's direct-execution request overrides skill approval/commit steps; retain a written plan and evidence without a Git commit.
- Baseline migration success exists at `reports/WORKSPACE/migration-20261020/MIGRATION_RESULT.json`; scoped remote Git is ancestor main dd66a53, not a source identity.

- Baseline CTest: 121/122; green candidate CTest: 123/124, same existing r7_zero_suffix_storage failure. Full12 plan and trust passed. Both target stdout regressions RED?GREEN.
- Independent review Important finding (C printf escaping FD1) fixed via QuietStdout; no other material findings or deferred minors.
- New Full12 P/V and oneblock P/V are running serially; final source trace removal/build/CTest/replay are SHA-guarded and queued after completion.

- Final: Full12 P/V accepted 8011/8011; oneblock P/V accepted; 2 malformed proofs rejected. Trace removed; final Release build and CTest 123/124 (same baseline failure), final independent replay both accepted with identical canonical events.
- Ruling: repair pre-existing oneblock caller manifest path via executionTarget(1), after baseline reproduced missing reports/R6A artifact; pinned statement SHA unchanged and private WitnessStore bytes untouched.
- Final archive: d672f7dbcd83315c8f45d8a438cdad567dd743c8a17259b99695ad5941072b7e; 1189 regular files, source468/parameters120/final binaries5 matched validated identities, gzip CRC and Windows CAS restore-read verified.
