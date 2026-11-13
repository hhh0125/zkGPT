# A2 Prover-only kernel dispatch

A2 is explicit `ZKGPT_PERF6_A2=1`; absent/0 keeps the generic arithmetic unless a preserved legacy explicit experiment is selected. No historical PERF5 default-on status is restored. Verifier returns through expressionSumcheck before reading either candidate variable. The first-round and folded coefficient kernel is the retained, already independently checked three-factor formula; the fresh acceptance uses current Proof and current samples.

The runtime candidate branch remains inside the Prover-only coefficient computation. No Verifier coefficient calculation, message work, authentication or challenge computation is added. Shared statement and verification flow are independently inspectable in oneblock_sumcheck.cpp. Test-only getenv instrumentation fails on historical Verifier (5 reads at n=32) and passes on A2 (0 reads).

Full Release build, full CTest 108/108, independent P/V, native canonical digest, boundary proof bytes and negative tests passed before formal A/B. A2_AB_RESULT.json determines performance status and default choice; partial sample improvements do not establish acceptance. Machine-code/build configuration comparison and source/binary identity accompany the stage archive.
