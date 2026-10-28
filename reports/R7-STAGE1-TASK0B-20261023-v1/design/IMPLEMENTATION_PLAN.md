# Task0-B Implementation Plan
> For agentic workers: execute inline using executing-plans and TDD; one final review.
Goal: bounded independent hidden-PC prototype with exact fixed-PC simulation.
Architecture: blinded rows + A.2 Sigma masked-response IPA + random row knowledge audit.
Stack: Python3 stdlib, BN254 prime-order group, only toy random data.
Spec: EXACT_DESIGN.md. User boundary overrides worktree/commit defaults; no git mutations.
Review focus: fixed PC without secrets; degenerate weights; reused handle; RNG/session;
entire vector vs one reduced row.
- [x] Review adapted compact core and extraction before implementation.
- [x] Add failing interface/boundary/positive tests; retain RED logs.
- [x] Implement curve/entropy/core/open batch in research_only; run full local toy suite.
- [x] Add failing corruption/zero/nonce/session/simulator tests; implement and rerun.
- [x] Measure three trials at N=1,16,64,256,1024 with CPU/RSS and wire logs.
- [x] Complete six reports, independent review, preserve failures.
- [ ] Verify source hashes/protected metadata, archive and Windows restore-read.
No source production code, build, parameter reads, or real proof commands.

Ruling: final EXACT/FIXED/MULTI reports supersede provisional design details. Algebraic extraction uses 3 square classes; stock GSS proof supplies 5 distinct actual fold coins. Search budget Q is not final tree size. Final archive/Windows task closes through its external SHA/restore receipt, created after packaging.
