# PERF6 bounded A/B results

All medians and weighted ratios follow the pre-frozen formula. No samples were dropped; no PERF5 threshold was changed. Bundles A2, A2+B2, A2+B2+C2 are separate fresh experiments against the generic baseline. Native unit-weight aggregates are separate from 186-call surrogate weights.

| Experiment | Product P | Whole P | Product V | Whole V | Gate |
|---|---:|---:|---:|---:|---|
| Surrogate A2 | 0.912864 | 0.914631 | 1.052338 | 1.052261 | FAIL: V_Product, V_whole, each_size_V |
| Surrogate B2 | 0.916618 | 0.926829 | 0.964828 | 0.964799 | FAIL: each_size_V |
| Surrogate C2 | 0.899571 | 0.907438 | 0.984061 | 0.984222 | PASS |
| Native A2 | 1.028652 | 1.050748 | 0.748064 | 0.757365 | FAIL: P_Product, P_whole |
| Native B2 | 1.049774 | 1.045833 | 1.016668 | 1.012588 | FAIL: P_Product, P_whole, each_size_V |
| Native C2 | 0.974489 | 0.969548 | 1.081798 | 1.073679 | FAIL: P_Product, V_Product, V_whole, each_size_V |

Product is inline hiddenProduct timing. Native Whole includes actual deferred source authentication; that barrier is not charged into an individual Product timer. Surrogate Whole is the complete bounded public-fingerprint harness, not a GPT2 proof.

| Experiment | q=1 Product ratio | q=2 | q=5 | q=10 |
|---|---:|---:|---:|---:|
| Surrogate A2 | 0.953597 | 0.975916 | 1.006797 | 1.025125 |
| Surrogate B2 | 0.931225 | 0.939039 | 0.949637 | 0.955812 |
| Surrogate C2 | 0.925017 | 0.938684 | 0.957280 | 0.968149 |
| Native A2 | 0.904475 | 0.856491 | 0.804526 | 0.779453 |
| Native B2 | 1.037420 | 1.031781 | 1.024993 | 1.021429 |
| Native C2 | 1.014309 | 1.032575 | 1.054632 | 1.066247 |

q is an analysis parameter, not an asserted user verification workload. The q=1 net savings do not override a failed per-size/security/RSS gate. See raw result JSON for absolute representative totals, Whole sensitivity, every per-size median and all failed checks.

A2 representative V failures: `{"8388608": {"Product": 1.0940119925379073, "whole": 1.0939464647927057}}`.
B2 representative V failures: `{"2097152": {"Product": 1.0693116780886347, "whole": 1.0693645972474761}}`.
C2 representative V failures: `{}`.

Remaining 126-call distribution: 30 fresh pairs over all ten omitted sizes. Weighted diagnostic ratios: `{"P": {"Product": 0.9850047585570539, "whole": 0.9850835556860552}, "V": {"Product": 1.006737117034403, "whole": 1.007152104371905}}`. These do not become a full-model speedup.

Default selection after both gates: NONE; all performance experiments OFF. Historical PERF4 2937.058658s P/828.128557s V remains a single background observation only.
