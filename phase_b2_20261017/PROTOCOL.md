# B2 independent direct-query relation, version 1

Engineering adaptation, not a protocol claimed to be specified by the Lasso or zkGPT papers. No new commitment bridge. Non-hiding prototype only.

## Audit conclusion before implementation

Reuse B1 vector IPA for CQ and the existing production generic Hyrax IPA for C0 rows. Both use the project MCL BN254 scalar field, but different generators; never subtract their commitments. C0 consists of R=2^floor(l/2) row commitments C_a=sum_c W[a*K+c]G_c, K=2^ceil(l/2), N=2^l. W is the logical val[0] prefix followed by zero padding, in row-major order. No witness permutation occurs in commitInput.

Trusted input consists of a verifier-approved WitnessLayoutDescriptor, selected lookup identifier, approved B1 instance, and exact C0 row commitments. A prover artifact provides none of these trust anchors. A CLI instance path must be provisioned by the verifier, like the B1 trusted-instance path. A descriptor hash freezes this input but does not certify its model origin. Test profiles are synthetic approved profiles; no full-model endorsement is inferred.

Descriptor declares version, field/curve/backend/padding IDs, logical witness length, circuit/profile identifiers, disjoint named regions, and multiple lookup specifications with exact logical index-to-region mappings and explicit alias policy. Logical query indices are list order. Check bounded lengths, checked region bounds, overlaps, unique identifiers, mapping bounds, permitted alias and B1 query length. A registry adapter selects unsigned direct range regions (not limbs) or direct Exp t/E regions from a caller-supplied independently rebuilt registry. This adapter must never be invoked on a prover-supplied registry as a trust anchor.

## Relation and transcript

For selected query j: Q[i]=W[pi_j(i)] for 0<=i<m. B1 has p=next_pow2(m), with Q padding T[0]. New B2 domain: zkGPT.route-b.witness-binding.v1. Absorb canonical descriptor/selected-instance/C0 digest, B1 instance digest, CQ. Sample fresh low-bit-first r of length log2(p), AFTER both commitments are fixed. The B1 lookup proof is fully verified against the approved instance; its CQ must be exactly the commitment opened here. B1's earlier point is NOT reused since C0 was not absorbed into its challenge.

Define b[a,c]=sum_{i<m:pi(i)=a*K+c} eq(r,i), and delta=T[0]*sum_{m<=i<p}eq(r,i). Open CQ with eq(r,.) in its own generator domain, obtaining q. For each C0 row a, open with public coefficients b[a,.] using the existing generic IPA, obtaining v_a. Verify q=delta+sum_a v_a. Zero coefficients are allowed; all rows and their exact IPA round counts are checked. No witness or query vector is sent.

This works because each v_a authenticates a linear functional of the same row vector bound by C0. CQ independently authenticates Q. Their generators need not match. All IPA messages/evaluations are absorbed in fixed Query-then-row order. Verifier reconstructs b and replays transcript independently. Artifact hashes do not define challenges. Mapping is reconstructed from approved descriptor, never from artifact-supplied indices.

For fixed unequal vectors, the discrepancy is a nonzero multilinear polynomial of total degree at most log2(p). In the interactive uniform challenge model, Schwartz-Zippel gives at most log2(p)/|F| collision (zero for p=1), plus underlying commitment/opening soundness. The actual Transcript samples nonzero challenges: denominator is |F|-1 under an ideal uniform-nonzero model. No full Fiat-Shamir/random-oracle composition theorem or concrete security bound for the existing challenge sampler is claimed.

## Costs and alternatives

Selected: separate openings, no new commitment. R existing C0 points + 1 existing CQ point; 1+R extra IPA openings, log2(p)+R*log2(K) IPA rounds, zero added Sumcheck rounds. Communication for adapter openings: (1+R)*68 + 64*(log2(p)+R*log2(K)) bytes with current fixed Fr/G1 encoding and u32 round counts, plus statement digests and nested B1 artifact. P/V arithmetic O(p+R*K) with existing IPA verifier MSM; transcript challenge count for B2 is log2(p)+IPA rounds, excluding B1. Opening each row is deliberately conservative, not a production performance proposal.

Existing sparse-pattern IPA is an optimization candidate for contiguous approved mappings. It opens grouped linear combinations of C0 rows with public pattern coefficients, subtracting signed bias where relevant. B1 CQ still needs a separate fresh-point opening. Not selected for initial adapter because arbitrary aliases/regions require general mapping semantics. Direct joint opening of CQ and C0 is incompatible with their different generators/reshape; a new bridge is unnecessary and is not implemented. Publishing full vectors and comparing arrays is not a proof and is not used.

## Boundaries

Binding unsigned raw Range values does not establish limb range membership/reconstruction. Signed Range requires affine bias and chunk reconstruction relations; these remain in existing production Range. Exp raw t and E are eligible direct queries; x=min(t,6891), selector s=[t<6891], d=s*(6890-t)+(1-s)*(t-6891), u8 limbs of d, pair x+alpha*E and canonical membership still require existing Exp constraints. B1 small explicit tables cannot represent the full production Exp table (B1 table cap 256). No production model proof, GKR integration, unified aggregation, hiding/Task10/11 changes, or B3/B4.
