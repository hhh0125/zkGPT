# Correctness and negative-test results

Full Release builds and CTest are recorded in *_BUILD_RESULT.json. Current core suite has 110 tests; the new Native fixture is separately exercised by independent process validation. Exact Proof/FS metadata comparisons precede formal samples.

A2: validation PASS, 7 explicitly expected-reject process cases; all returned 1. Cases: A2-negative-root, A2-negative-leaf, A2-negative-allocation, A2-negative-session, A2-negative-truncated, A2-negative-appended, A2-negative-mutated.
B2: validation PASS, 7 explicitly expected-reject process cases; all returned 1. Cases: B2-negative-root, B2-negative-leaf, B2-negative-allocation, B2-negative-session, B2-negative-truncated, B2-negative-appended, B2-negative-mutated.
C2: validation PASS, 7 explicitly expected-reject process cases; all returned 1. Cases: C2-negative-root, C2-negative-leaf, C2-negative-allocation, C2-negative-session, C2-negative-truncated, C2-negative-appended, C2-negative-mutated.
NATIVE: validation PASS, 7 explicitly expected-reject process cases; all returned 1. Cases: native-negative-session, native-negative-omit-auth, native-negative-wrong-shape, native-negative-private-read-counter, native-negative-truncated, native-negative-appended, native-negative-mutated.

Root/leaf/allocation injection can stop the self-checking Prover; those are labeled Prover guard failures, not misrepresented as independent Verifier algebraic rejection. Session, EOF/trailing bytes and serialized Proof mutation are independent witness-free Verifier rejections. r7_oneblock_lookup_tests additionally executes real A3 native source, recurrence, tail and algebraic response negatives. Native wrong read counter is a Prover guard; native omit-auth exercises the mandatory authentication-barrier guard.

Native correctness tested all four option modes at 512/8192/32768 with exact bytes; independent Verifier accepts nine deferred claims only after unified-input/single-Hyrax authentication. No private Full12 witness was loaded, exported or used.

No actual Full12 proof was generated. Security state is unchanged: FS_NIZK_CANDIDATE / BLOCKED_CRYPTOGRAPHIC_DESIGN / may_enter_stage2=false.
