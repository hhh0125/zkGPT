# Bounded native fixture design and disclosure

The PERF6 fixture must exercise the production prepared Native Memory/Lasso path, three hidden Products, actual Sigma/FS, deferred leaf/source claims through classified Commitments and authenticateUnifiedInput/single Hyrax. No public O(n) leaf expected-value loop may replace that barrier.

Inputs are a bounded synthetic address/value witness generated only by the Prover from a saved test seed, plus consistent read/final counters. They are not a private Full12 witness or a GPT2 checkpoint. Verifier uses only public layout/table and the Proof, never the seed or witness arrays. Full12 GKR, model authority registration, actual 8011 coverage/1502 claims and private Full12 input are absent. No Full12 witness is exported. Any measured speedup is a native synthetic fixture observation, not a complete Full12 result.

Sealed roots: one minimal model-class block, one auxiliary block holding address/value arrays, prepared lookup/read and lookup/final blocks. Source views and factory ids/shapes are fixed before commitments. Real hiddenMemory(...,prepared=true) consumes those views; their open callbacks queue the original handles. A required final unified-input authentication barrier discharges them. Independent FsVerifierIO and finish() enforce strict EOF.

Deterministic test-only masking/nonces allow exact proof, transcript and challenge comparison across implementation switches. Production entropy code is unchanged. The fixture's deterministic stream cost differs from production OSRandom/SessionRandom, so entropy timing cannot be transferred. An additional live-entropy correctness run may be reported separately if executed.

Those test masks are public/deterministic: fixture Proofs do not provide witness confidentiality. “Prover-only synthetic seed” describes file/data access in this fixture, not a claim that its Proof hides the synthetic values. It exercises actual private-data code paths and authentication equations, without loading real Full12 witness bytes. No live-entropy Native run is claimed by the deterministic experiment.

The source was implemented in perf6_native_probe.cpp. The first build omitted paper_r1/wire.hpp for readFile; the missing include was corrected before independent proof generation. Full Release build and 110/110 CTest then passed. NATIVE_VALIDATION.json records all four modes at the three scales with exact Proof/FS equality, witness-free independent Verifier and seven expected rejection processes. The seed uses exclusive creation, so a repeated validator cannot overwrite an earlier input before failing on existing logs.

Pre-frozen native scales [512,8192,32768], six alternating independent-process pairs per scale, unit weights and 1800s total budget are in PERFORMANCE_GATE.json. These deliberately bounded scales test the real authentication path but do not represent the largest Full12 calls. All validity/security and P/V/RSS requirements apply; failure is not replaced with the surrogate outcome.

Implementation and executed status are recorded in NATIVE_VALIDATION.json/NATIVE_AB_RESULT.json when available. Absence means NOT_RUN; this design document alone is not evidence of acceptance.
