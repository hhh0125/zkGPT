# Task0-C execution ledger

Approved scope: bounded research-only committed Sumcheck/GKR; no Stage2, production verifier, real model, oneblock/full12 or large MSM.

| Task | Status | Evidence |
|---|---|---|
| Baseline/migration/gate check | complete | remote `AGENTS.md`, migration success, Task0-B final status |
| Exact design and plan | complete | `design/EXACT_DESIGN.md`, `design/IMPLEMENTATION_PLAN.md` |
| Trusted interactive transcript | complete | RED/GREEN 01 |
| Shared linear/product relations | complete | RED/GREEN 02 |
| Degree-2/3 committed Sumcheck | complete | RED/GREEN 03 |
| Task0-B opening bridge | complete | RED/GREEN 04 |
| One/two-layer GKR and matrix | complete after one canonicalization bug fix | RED/GREEN/DEBUG 05 |
| Bounded resource measurements | complete | `delivery/R7_C_RESOURCE_MEASUREMENTS.json` |
| Independent reviews | complete with corrections | global-c claims narrowed; matrix/schema bugs fixed; executable receipt boundary marked BLOCKED |
| Six reports | complete, pending final verification | `delivery/` |
| Final SHA/archive/restore validation | pending | archive receipts |

Design rulings:

- Hidden-output `S0` is fixed in statement; external authentication remains out of scope.
- Public-output `S0` is `s0*G` with blind zero.
- Scalar handles share one final `c`; derived `S_i` is expanded, not separately opened.
- Only final PC endpoints invoke Task0-B H-PC; no per-round row audit.
- Resource grid is N={1,16,64,256,1024}, degrees {2,3}.
