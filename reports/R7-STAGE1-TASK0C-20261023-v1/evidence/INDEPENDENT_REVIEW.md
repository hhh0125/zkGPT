# Task0-C independent review disposition

Date: 2026-10-23. Two read-only reviewers inspected the local source, tests and reports.

| Finding | Severity | Disposition |
|---|---|---|
| Reports claimed one global `c`, while implementation uses separate Sumcheck/endpoint/layer relation challenges | Critical | Reports corrected everywhere; per-batch extraction retained, global multi-stage extractor marked PARTIAL |
| Same-type challenge state can be created/modified by a hostile caller because `prove_*` returns it | Critical | Not hidden by type checks; executable verifier gate set `BLOCKED_EXTERNAL_CHALLENGE_BOUNDARY`; Task0-D code gated on external Verifier-owned session refactor |
| Matrix verifier accepted row=2 MLE extrapolation | Important | Added adversarial RED reproducer, explicit 2x2 PC/index checks in prover and verifier, GREEN regression |
| Whole fixed-PC HVZK relied on inherited H-PC theorem | Important | Wording made conditional on archived Task0-B exact theorem and byte-identical inherited implementation |
| `verify_sumcheck` does not authenticate arbitrary `instance` semantics | Important | Reports now call it a reduction verifier; semantic conclusion requires terminal predicate/opening |
| Relation/Sumcheck statement schema and session validation incomplete | Important | Added malformed-statement RED tests and strict suite/kind/session checks; GREEN |
| Reports implied X/Y/Z all directly opened | Important | Corrected: H-PC opens X/Y; Z is bound by A.1 multiplication proof and terminal relation |
| `run_all.py` could discover zero tests from another cwd | Important | Discovery rooted at `__file__`; exact 26-test assertion |
| Random response mutation could occasionally be a no-op | Minor | Mutation now parses scalar and adds one modulo q |
| Resource measurement wording omitted same-process warm state and excluded statement/PC | Minor | JSON/report wording corrected; final measurements rerun |

No reviewer found an error in the Sumcheck recurrence, Hyrax A.1 equations, per-batch rho compression bound, GKR individual-degree bound, or λ-after-endpoint ordering.

The unresolved executable challenge-state item is intentionally visible in `R7_TASK0C_GATE_REVIEW.md`; it is not treated as fixed by ordinary Python object encapsulation.
