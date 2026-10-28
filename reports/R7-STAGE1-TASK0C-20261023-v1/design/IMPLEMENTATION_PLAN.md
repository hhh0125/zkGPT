# Task0-C Committed Sumcheck and toy GKR Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build and audit a bounded interactive research prototype for exact committed degree-2/3 Sumcheck, final H-PC opening, and one/two-layer toy GKR without changing production code.

**Architecture:** Incrementally fixed scalar coefficient commitments feed derived hidden claims. One end-of-session random linear compression and one shared Sigma challenge authenticate every coefficient, recurrence, endpoint product, and layer aggregation; only final master endpoints invoke one bounded Task0-B PC batch.

**Tech Stack:** Python 3 standard library; byte-identical Task0-B toy curve, entropy, and H-PC modules; unittest; canonical JSON; remote fixed zkGPT-work directory.

**Spec:** `reports/R7-STAGE1-TASK0C-20261023-v1/design/EXACT_DESIGN.md`

## Global Constraints

- Research-only directory `reports/R7-STAGE1-TASK0C-20261023-v1`; no worktree or ancestor Git mutation because project AGENTS.md defines file hashes, not ancestor Git, as content identity.
- N<=1024; degree only 2 or 3; at most two toy GKR layers; H-PC batch K<=4; MSM<=32; CPU<=120 s/process; address space<=512 MiB; wire<=2 MiB.
- Fresh toy data only. Do not read real model parameters, R6 proof, or true W anchor; do not run oneblock/full12 or production binaries.
- Preserve prior R6/R7 bytes and original `may_enter_stage2=false`; no deletion.
- Every implementation behavior follows RED -> observed expected failure -> minimal GREEN; preserve raw logs.

## Review Focus

- A coefficient or endpoint changed after its challenge must be rejected; covered in Task 2 and Task 4 mutation tests.
- A prover-supplied coin receipt or cross-session transcript must not authenticate. Prefix/session replay is tested, but same-type state provenance is not enforced by the current single-process API; external Verifier ownership remains follow-up work.
- Zero claim, repeated point/coin, zero leading coefficient, and identity handle must avoid division/abort; covered in Task 3 boundary tests.
- Two bottom endpoints must reuse the exact same master PC and exact endpoint handles; covered in Task 5 matrix/GKR integration tests.
- Opening extraction remains K<=4 and is not multiplied per round/layer; checked in Task 6 static call/count evidence and security report.

---

### Task 1: Stage baseline, inherited primitive, and transcript skeleton

**Files:** Create stage directories, baseline evidence, byte-identical `research_only/curve.py`, `entropy.py`, `hpc_protocol.py`, and new `research_only/transcript.py`; test in `test_transcript.py`.

**Interfaces:** Produces canonical statement/Flow helpers and trusted verifier receipt replay consumed by all later tasks.

- [ ] Write tests for prefix-bound challenge order, wrong prefix, unused coin, zero challenge rejection, repeated nonzero coin acceptance, and cross-session rejection.
- [ ] Run tests and preserve RED output showing the missing transcript module.
- [ ] Implement the minimum transcript wrapper and exact Task0-B source import/copy identity checks.
- [ ] Run the task tests and full research-only suite; preserve GREEN output.
- [ ] Record baseline source/protected-file identities without reading real payloads.

### Task 2: Scalar relation batch and Hyrax product Sigma

**Files:** Create `research_only/relations.py`; test in `test_relations.py`.

**Interfaces:** Consumes Flow and curve helpers; produces scalar handles/openings, derived handles, linear relation finalizer, product first/response/verify, and fixed-statement simulators.

- [ ] Write hand-derived tests for representation, equality/public-constant linear relations, two simultaneous relations, product, wrong value/blinding/product, changed order, wrong session, and fixed-point simulation without witnesses.
- [ ] Run and preserve RED output.
- [ ] Implement exact equations from the design with one shared Sigma challenge and independent nonzero relation-compression coins.
- [ ] Run tests and full suite; preserve GREEN output.

### Task 3: Degree-2/3 committed Sumcheck

**Files:** Create `research_only/sumcheck.py`; test in `test_sumcheck.py` and algebra fixtures in `test_helpers.py`.

**Interfaces:** Consumes relation session; produces incrementally committed rounds, derived S_i handles, terminal relation hooks, and product-of-MLE toy instances.

- [ ] Write tests with literal small polynomials for coefficient interpolation, correct degree-2/3 proofs, two sequential Sumchecks under one finalizer, and one secret-vector plus public-weight terminal.
- [ ] Run and preserve RED output.
- [ ] Implement per-round commitment-before-r flow, exact recurrence relations, terminal products/mixed constants, and one final shared Sigma proof.
- [ ] Add corruption tests for wrong initial claim, coefficient recurrence, final claim, multiplication, declared degree, challenge order, and session splice; add zero/repeated/degenerate cases.
- [ ] Run the full suite and preserve GREEN and explicit negative-rejection logs.

### Task 4: Bind terminal claims to the inherited committed opening

**Files:** Create `research_only/opening_bridge.py`; extend `test_sumcheck.py`.

**Interfaces:** Consumes the exact final/endpoint handle and parent transcript digest; produces one Task0-B opening batch with one row audit.

- [ ] Write failing tests for a correct K=1 opening and rejection of wrong point, value/handle, PC identity, parent session, suite, and trusted verifier receipt.
- [ ] Run and preserve RED output.
- [ ] Implement the bounded bridge by reusing the exact handle point and deriving the child session from the authenticated parent state.
- [ ] Run tests and the full suite; preserve GREEN output and verify no raw witness/blind appears in canonical wire.

### Task 5: One/two-layer toy GKR and matrix endpoints

**Files:** Create `research_only/gkr.py`; test in `test_gkr.py`.

**Interfaces:** Consumes Sumcheck session and opening bridge; produces standard one-layer and two-layer toy GKR, internal lambda aggregation, and matrix multiplication with two endpoints in one master PC.

- [ ] Write failing tests for one layer, two layers, joint parent product, internal aggregation, modified lambda/order, wrong terminal, and both matrix endpoint points against one master PC.
- [ ] Run and preserve RED output.
- [ ] Implement the public wiring MLE polynomial, deferred shared finalizer, two-layer selector aggregation, and master layouts.
- [ ] Run tests and the full suite; preserve GREEN and negative logs.

### Task 6: Measurements, proofs, reports, independent review, and archive

**Files:** Create `measure.py`, `run_all.py`, six requested delivery files, literature/search records, code index, raw logs, manifests, archive script, and local Windows verification receipt.

**Interfaces:** Consumes all preceding artifacts; produces reviewable resource JSON, security gate, immutable archive, and restore-read evidence.

- [ ] Measure bounded degree-2/3 `N in {1,16,64,256,1024}` and toy GKR/matrix cases with proof bytes, verifier-coin bytes, prover/verifier time, RSS, MSM maximum, and call counts.
- [ ] Write exact completeness, fixed-commitment HVZK, soundness/knowledge, GKR mapping, extraction-boundary, FS-open-obligation, and gate analyses with source/paper citations in the exact files `R7_C_COMMITTED_SUMCHECK_PROTOCOL.md`, `R7_C_SUMCHECK_SECURITY.md`, `R7_C_GKR_INTEGRATION_DESIGN.md`, `R7_C_TOY_TEST_REPORT.md`, `R7_C_RESOURCE_MEASUREMENTS.json`, and `R7_TASK0C_GATE_REVIEW.md`.
- [ ] Run fresh full tests, static caps/wire scans, source SHA checks, protected-byte comparison, and requirement checklist.
- [ ] Dispatch one fresh protocol/security reviewer and one code/evidence reviewer; make one TDD fix pass for any Critical/Important findings.
- [ ] Build the stage archive, copy it to Windows, verify archive SHA, gzip integrity, every member SHA, safe paths, and restored bytes; store the Windows receipt back remotely as external evidence.
- [ ] Confirm no production artifacts ran or changed, `may_enter_stage2=false`, and production gate BLOCKED.

Ruling: the user's fixed remote directory and prohibition on sibling source copies override generic worktree creation and Git commit steps. The execution method is native inline with fresh builder/final reviewers because the six tasks share one transcript and relation registry. Toy GKR uses a statement-fixed external hidden `S_0` with opening knowledge; public-output mode instead uses verifier-constructed `s_0G`.
