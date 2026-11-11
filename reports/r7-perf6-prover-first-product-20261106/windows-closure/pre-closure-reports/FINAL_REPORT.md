# PERF6 final result

1. A historical ~18% Verifier regression: true physical root cause UNRESOLVED. Same-code/same-Proof isolation supports measurement variability; polynomial arithmetic is absent from V. Removing candidate getenv reads does not by itself prove no regression.
2. B initial/final: every raw pairing and old gate reproduces; final 2^23 V median ratio 1.107659 and weighted 1.012769 fail old gates. Source/profile changes are reconstructed; physical scheduling/frequency cause remains unknown.
3. Removing unused V weights truly removes 23 constructions and ~268MB traced allocation at 2^23, ~0.5s in the old measured region; equality authentication remains. Legacy B stays OFF.
4. New A2 and contiguous C2 preserve equivalent coefficients/Proof/FS and show fresh bounded Prover gains; acceptable retention is decided by the complete new gates, not kernel timing alone.
5. New Product/Whole P/V measurements and failed checks are listed in BOUNDED_AB_RESULTS.md and FINAL_RESULT.json.
6. q=1,2,5,10 sensitivities use T_P+q*T_V on bounded data; net gains cannot override mandatory per-size gates.
7. Selected default bundle: NONE. Only a bundle passing both independent surrogate and native gates may be enabled.
8. Actual Full12 ready to run: NO. Performance admission: False; separate authorization remains absent.

All requested optimization experiments, source/tests, raw logs, failures and SHA/restore evidence are retained. Final source/default/build identities are checked separately during archive closure.

FS_NIZK_CANDIDATE / BLOCKED_CRYPTOGRAPHIC_DESIGN / may_enter_stage2=false.
