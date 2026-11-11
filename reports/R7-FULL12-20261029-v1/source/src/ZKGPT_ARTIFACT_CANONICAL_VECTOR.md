# zkGPT Artifact v2 canonical serialization/binding test vector

> **Serialization/Binding Test Vector Only.** This public synthetic fixture is
> structurally serializable and has a correctly derived Artifact binding, but
> it is not a valid Range + GKR proof and is intentionally rejected by
> production `verifyZkGPT()`. It must not be cited as a verified zkGPT proof.

The vector contains no model parameter, private witness, real input, or secret
randomness. The Release test constructs the fields below, calls the production
serializers and `bindZkGPTProof`, and compares their outputs with independently
fixed constants in `main_range_tests.cpp::testZkGPTArtifactSerialization`.

## Protocol identifiers

| Item | Value |
|---|---|
| Top-level mathematical protocol | `protocol_version = 2` |
| ZKGS envelope/schema | version 2 |
| ZKPF envelope/schema | version 2 |
| Nested GKR proof schema | ZKGP version 3 |
| Nested Range Statement schema | ZKRS version 1 |
| Nested Range Proof schema | ZKRP version 2 |
| Artifact Transcript domain | `zkGPT-artifact-v2` |
| GKR Transcript domain | `zkGPT-proof-v2-chain-sha256` |
| Circuit fingerprint algorithm domain | `zkGPT-circuit-v2` |

## Public synthetic fixture

### ZkGPTPublicStatement

```text
protocol_version = 2
circuit_id = "gpt2-quantized-v1"
model_shape = {32, 12, 12, 64, 768, 3072}
circuit_fingerprint = 42 || 00 * 31
output_claim.mle_evaluation = Fr(19)

range_statement.val0_log_size = 2
range_statement.val0_commitment = []
range_statement.val0_generator_domain = {"zkGPT/main", 2}
range_statement.range_generator_domain = {"zkGPT/range", 2}
range_statement.shape = {32, 12, 12, 64, 768, 3072}
range_statement.regions = []
range_statement.queries = []
```

### ZkGPTProof before top-level binding

```text
gkr_proof.output_evaluation = Fr(19)
gkr_proof.layers = []
gkr_proof.lasso.rounds = []
gkr_proof.lasso.input_evaluation = Fr(0)
gkr_proof.lasso.mapping_evaluation = Fr(0)
gkr_proof.lasso.input_opening.ipa.rounds = []
gkr_proof.lasso.input_opening.ipa.final_witness = Fr(0)
gkr_proof.transcript_binding = Fr(20)

range_proof.chunk_commitments = []
range_proof.reconstruction_proofs = []
range_proof.val0_openings = []
range_proof.membership_proofs = []
range_proof.transcript_binding = Fr(21)
```

`bindZkGPTProof(statement, proof)` then replaces only the top-level
`proof.transcript_binding`.

## Fixed outputs

These are three different objects and must not be called by one ambiguous
"hash" name:

| Object | Encoding/result | Fixed value |
|---|---|---|
| Canonical ZKGS bytes | SHA-256 digest of `serializeZkGPTPublicStatement` bytes | `c0c8d6b91ce76dc473f7eb61a94a59081182734bce69feec06a89db2ec242a8c` |
| Canonical ZKPF bytes | SHA-256 digest of `serializeZkGPTProof` bytes after binding | `b8d53a19576b04f6ac879a5b7e9539b4e92a55db2d87b2e9f058423632ee5bb7` |
| Artifact Transcript binding | canonical 32-byte BN254 `Fr::serialize` output | `619960f6e9d542df95d7e91ebb2e12136d9b2b87feff3ce7bf10074cb0911b10` |

The Artifact binding is derived by the production code as:

```text
Transcript("zkGPT-artifact-v2")
  .appendBytes("statement", canonical ZKGS bytes)
  .appendBytes("gkr_proof", canonical ZKGP v3 bytes)
  .appendBytes("range_proof", canonical ZKRP v2 bytes)
  .challenge("zkGPT.artifact.final")
```

The outer ZKPF serialization itself is not absorbed into the binding, avoiding
self-reference: the nested GKR and Range proof bytes are absorbed first, and
the resulting binding is then serialized as the last ZKPF field.

## Expected verification boundary

The canonical ZKGS and ZKPF values round-trip byte-for-byte, and the stored
binding equals the production binder output. Nevertheless, production
`verifyZkGPT()` rejects the fixture at Range verification because it contains
no Range queries or valid Range proof. This rejection is fixed by the Release
test and is expected.

Passing this vector establishes serializer and binder stability only. It does
not establish GKR, Range, model, output, or commitment correctness.
