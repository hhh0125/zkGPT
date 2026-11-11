# Reused model GKR / indexed Lasso / common input

The arithmetic path reuses `../gkr_proof_builder.cpp` and `../gkr_verifier.cpp`,
including both FCONN matrix points. The generic R1 GKR is a foundation test
implementation, not a second production model path. No LogUp proof is called
by `proveR2` or `verifyR2`.

The v3 composition fixes the public circuit, model/input/output identities and
all lookup/master commitments, proves and verifies local Range/indexed Exp
Lasso, then derives a nonzero integration context from that transcript.
The existing GKR algorithm runs under a new domain with that context. A
completed legacy GKR proof cannot be imported with its old challenges.

All authenticated GKR input terminals are reconstructed from verifier-owned
`ori_id_u/v` mappings, including ordinary input prefixes and both FCONN points.
They enter the final mapped-input Sumcheck directly, alongside authenticated
lookup children, signed bias, approved W, public X/Y, constants and zero tails.
One final new-master opening authenticates its endpoint. The existing GKR input
fold and a two-PC random-point bridge also remain checked: generator domains
and layouts are opened separately, never compared by subtracting commitments.

The caller selects the approved public statement. `model_queries.cpp` rebuilds
all model source mappings and mandatory obligations; it does not trust a layout
chosen in the proof. The statement fixes the complete externally selected X/Y
and compact W commitment. Publisher provenance is an additional trust input,
not established by accepting a prover-supplied statement.

Build and test from the repository root:

```sh
cmake -S . -B phase_b3_r2_gkr_lasso_integration_20261019/build-v3 -DCMAKE_BUILD_TYPE=Release
cmake --build phase_b3_r2_gkr_lasso_integration_20261019/build-v3 -j 8
ctest --test-dir phase_b3_r2_gkr_lasso_integration_20261019/build-v3 --output-on-failure
```

Executables in `build-v3/src/paper_r2/`:

- `r2_model_integrated NEW_DIRECTORY`: uses all real one-block queries and
  existing local quantized parameters to generate `model.statement/model.proof`.
- `r2_model_verify APPROVED_STATEMENT PROOF`: independently rebuilds the public
  one-block circuit and checks the whole composition without private files.
- `r2_verify --fixture APPROVED_STATEMENT PROOF`: small four-FCONN regression.
- `r2_model_source_probe/verify`: earlier source-only diagnostics, not a complete
  model proof. Keep their acceptance scope separate.

The model executable explicitly asserts the existing **one-block public
quantized constraint system**, with 32x768 padded activations, 12 heads,
head size 64, MLP size 3072, all 52 registered Range regions and 6336 Exp pairs.
Its root GKR residual is zero; the actual activation output Y is separately
fixed at every output coordinate through the final consistency check.
It does not assert full GPT-2 inference or equivalence to the zkGPT paper.

The real graph uses `max(1,round(2^20*exp(-t/512)))`, t in u32. Its explicit
`LegacyClampOne` profile uses the certified R1 table transformed by max(1,.) and
tail value 1. The default R1 `Unclamped` profile remains a separate regression
instance, with tail 0. Profile and table identity enter the transcript and wire
statement; substituting the profile fails even where the active values agree.
There is no change to model computation or witness to conceal this distinction.
The old standalone Exp codec rejects clamp profiles; the lookup codec carries
them explicitly.

`hostile_integration.hpp` and `lookup_attack_fixture.hpp` are test-only dishonest
provers. They generate valid local proofs and authentic openings but an
inconsistent aggregate input claim, so the full verifier must reject. They are
not linked into verifier libraries.

The deterministic PC and unmasked sumchecks provide no full zero knowledge.
Attention scaling, maximum/rounding/fused numerical obligations, publisher
authentication, a complete knowledge/FS composition argument and full12
verification remain separate acceptance gaps. See the round's reports before
treating this implementation as a production cryptographic system.
