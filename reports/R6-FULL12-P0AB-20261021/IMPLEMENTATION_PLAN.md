# R6 full12 P0-A/P0-B execution preparation
Goal: actual 12-block Prover plus separate strict Verifier ACCEPT, after exact identity authorization.
Spec: user request 2026-10-21; fixed workspace AGENTS.md.
Constraints: no numerical/protocol/source optimization; P/V 96GiB, 4 logical CPUs, P 28800s / V 14400s (ceilings), 32GiB effective reserve, 64GiB disk reserve; one attempt, no automatic retry; preserve all history.
- [ ] Freeze current source, dependencies and history; isolated rebuild.
- [ ] R6 smoke proof and separate verification; metadata/ownership/group/mapped regressions.
- [ ] New full12 actual-builder plan; compare frozen identities and refresh budget (estimate only).
- [ ] Copy existing gate to this stage; test scoped other-user WARN policy before changing it; retain hard deficits and own proof-job refusal.
- [ ] Bind fresh source/binaries/input/Profile/anchor/manifest/run options into Scope draft. Keep A/B/C/D approval boundaries. Permit content preview only until A is explicitly signed.
- [ ] Archive source, dependencies, parameters, smoke evidence, gate and drafts; verify Windows SHA and restore-read.
- [ ] If exact A/C signatures are absent, stop with minimal exact-SHA approval request. Otherwise one serial full12 execution with measurements and archive.
Ruling: supplied user spec authorizes preparation and scoped resource policy; no additional plan review is needed. Signature mechanism remains mandatory.
Review focus: source drift; frozen Profile/W/X; historical report overwrite by test; resource-policy scope leak; draft mistaken for approval.
