# R7 category layout implementation plan
Goal: one C_U authenticates X/W/Aux/Query/Helper, each padded once, consecutive, no alignment gaps.
Scope: fixed work directory; original X; unchanged checkpoint/Exp/Statement/history.
production_gate = BLOCKED_CRYPTOGRAPHIC_DESIGN
may_enter_stage2 = false
Ruling: preserve uncommitted PERF6 changes over GitHub main 43662cd; no source checkout/reset.
Ruling: storage Chunk IDs remain internal; aligned computation tiles are views only, without PCs or claims.
Review focus: empty categories; unaligned boundaries; nonzero helper-local padding; X aliasing; precommit X mutation.
- [ ] A: derive X coordinates from public R6 registry; baseline SHA and archive.
- [ ] B: test 3/5/7/9/11, >2^24, empty categories, random mapped MLE; implement category plan/layout.
- [ ] C: borrow original sealed native/helper storage; mandatory post-seal public-X Claim.
- [ ] D: bounded streaming K over computation tiles, one Unified Sumcheck and one C_U opening.
- [ ] E: build; full CTest; source/crypto/attacks; independent Oneblock and resource-qualified Full12; exact baseline comparison.
- [ ] F: fresh review; archive source/patch/config/parameters/proof/logs; Windows SHA and restore-read.
