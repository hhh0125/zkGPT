# R7 Full12 unrestricted execution
Goal: generate one complete native 12-block FS proof and independently verify it, then run representative rejection tests.
Constraints: fixed zkGPT-work directory; Windows authority based on9f8869e/source f04c2ef; preserve Oneblock/OPT1/OPT2/RowMajor and all numerical/parameter/Exp bytes. No OPT3, global resource changes, sudo, deletions or protocol audit. Own RLIMIT_AS inherits unlimited hard cap; no arbitrary wall timeout. Four CPUs/threads and actual RSS/Vm/system/cgroup/disk monitoring. FS_NIZK_CANDIDATE / BLOCKED_CRYPTOGRAPHIC_DESIGN / may_enter_stage2=false.
- [x] Inspect physical/cgroup resources and own limits; matched376 baseline source files.
- [x] Real12-block witness succeeded; reference X/Y matched, no legacy commitment; complete fields retained in private0700/0600 checkpoint.
- [x] Generate10 chunk anchors using actual R6 preprocessing and independent production reader; recompute in a second process; byte-compare and fix independent pin.
- [x] Connect45-root runtime and formal Full12 FS P/V; retain all GKR/Range/Exp/Memory/Source/authentication relations and add explicit cross-block obligations.
- [x] Component/regression and separate-process small partition FS P/V tests passed, including tampered proof rejection.
- [ ] Run real Full12 P to exit, then separate V; compare session/challenge/transcript/proof identity and require every dynamically discovered obligation VERIFIED.
- [ ] Wrong X, wrong model and proof-message tamper rejection; restore the sole proof bytes after reversible tampering.
- [ ] Commit final source, collect actual measurements, archive public proof only once and verify Windows restore reads. Never archive private witness/blinds.
