# Task 10 independent review and disposition

Design review: standard linear-map generalized Schnorr, completeness,
special soundness, special HVZK, b=0 behavior and conditional ROM design
accepted with no important unresolved design question.

Fresh code review found:
- Critical: inherited MCL setByCSPRNG/setArrayMask is nonuniform.
- Important: inherited read-result pointer check can process unread bytes.
Both were independently confirmed against remote fp.hpp:395-408,505-532.
The implementer replaced only the new primitive's samplers with raw Linux
getrandom and canonical Fr rejection, checked reads before decode. This was
verified by sampler RED (2 PASS/3 FAIL) then full suite GREEN (43/43).
Shared MCL and legacy crypto were not modified. No second review is claimed.

Deferred minors:
- Additional n2 b0, explicit zero C/nonce and maximum-domain/buffer boundary
  tests; modulus/zero/FF candidate tests were required for the sampler fix
  and are now covered.
- Hardening against modifications of MCL global map/serialization settings.
  Default BN254 settings are explicit API preconditions.

Declined-to-judge items and disposition:
- Full GPT/hidden-y integration: intentionally not implemented, NOT CLAIMED.
- Timing/cache/memory zeroization: not audited; no constant-time guarantee.
- Runtime and legacy preservation: verified by root with actual execution,
  final results and baseline comparison; not attributed to code reviewer.
- Remote dependency mismatch: root confirmed the remote sampling finding.

Build warnings from MCL CMake compatibility and gmp_util.hpp remain noted;
no new primitive warning appears in the final incremental build.
