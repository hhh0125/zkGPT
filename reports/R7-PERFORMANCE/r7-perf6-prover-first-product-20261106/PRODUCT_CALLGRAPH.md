# Product call graph and measurement boundaries

Real Full12 classified_controller.inc -> hiddenRange/hiddenExp (prepared=true) -> hiddenMemory -> hiddenProduct -> productSumcheck -> expressionSumcheckImpl -> Prover polynomial, shared exchangeHidden/Sigma recurrence, FS coin/combine/binding, Prover fold -> shared endpoint Sigma -> equality authentication -> carry -> native leaf authenticator -> pcs.view.open -> deferred classified claims -> authenticateUnifiedInput -> single Hyrax opening.

The old bounded perf5_product_fs_probe enters the same hiddenProduct and Sumcheck/Sigma/FS implementation, but uses public structured fingerprints and immediate O(n) public leaf authentication. Its whole timing includes root computation and setup, and is not a full model. It cannot independently establish true Full12 speedup.

Prover polynomial computes four coefficients of sum_j (a0+d0 X)(a1+d1 X)(a2+d2 X). Verifier does not call either polynomial implementation. Fold consumes the same challenge and updates each factor pair in place. All message/authentication checks remain shared; no Sigma, handle, statement or FS input may be dropped for optimization.

PERF5 internal Scope uses thread-local optional allocation tracing and nested wall subtraction. It includes callbacks, arithmetic and accumulation in Polynomial/Fold; allocation counts cover ordinary C++ new on the tracing thread, not C malloc/aligned/worker allocation. Separate test-only malloc sampling must not be used as formal timing.
