# Task0-D independent review (DESIGNED / CONDITIONAL)
Read-only reviewer /root/d_final_review examined new memory schedule, independent V transport, inherited primitives and security text.
No critical acceptance flaw found in normal memory mode under documented commitment/extraction assumptions.
- Eight legacy modules and verifier_components.py byte-identical to C1.
- Root handles open fixed PCs; every layer authenticates parent and children; leaf/padding constraints present.
- Normal mode coins V-owned; K<=4 per batch; ideal fixed-PC sequential HVZK argument consistent.
Findings addressed:
1. Correlated memory-zero diagnostic excluded from soundness. Added explicit invalid-lookup ACCEPT control and normal-mode REJECT comparison.
2. Original endpoint_product test actually mutated root product. Added targeted tree/0/layer/1/product response and challenge tests; both independently REJECTED.
3. N<=512 is shape ceiling, not operational claim. Event2048/wall60s budgets can abort otherwise mathematically valid large or padding-heavy inputs. Tested completing N4/8/16 only.
Global knowledge PARTIAL; production BLOCKED. Reviewer made no edits and ran no proofs/tests.
