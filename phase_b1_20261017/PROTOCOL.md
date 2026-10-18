# Route B Phase B1 protocol and implementation plan

Date: 2026-10-17 Asia/Shanghai. User specification is the supplied Phase B1 task.

## Frozen mathematical instance

P/R: L[i]=T[a(i)], M[i,y]=1[y=a(i)], row(i)=i, val(i)=1.
D: M~(r,u)=sum_i eq(r,i)eq(u,a(i)); L~(r)=sum_y M~(r,y)T[y].
R: dim_h:{0,1}^log(mpad)->F encodes integer address chunk h; this is NOT an address-bit-domain polynomial.
R: Lasso Figure 6 invokes Surge Figure 5. Choose c=1 for arbitrary explicit tables; additionally support independently checked additive SOS tables with arbitrary integer bit partitions. For additive tables g(E)=T[0]+sum_h E_h and T_h[d]=scale_h*d. c=1 explicit uses g(E)=E.
R: commit query and dim before r. Commit E_h, read_h, final_h before tau,gamma. Prove ell=sum_i eq(r,i)g(E(i)) by degree-2 sumcheck and authenticate each E endpoint.
R: each memory has I={(a,T_h[a],0)}, W={(dim(i),E(i),read(i)+1)}, R={(dim(i),E(i),read(i))}, F={(a,T_h[a],final(a))}. Check H(I)H(W)=H(R)H(F), H(S)=prod_(a,v,t in S)(a*gamma^2+v*gamma+t-tau). Characteristic BN254 Fr is larger than all capped read counts.
R: grand products use the binary multiplication-tree protocol in Thaler 2013 Proposition 2, explicitly cited by Lasso Figure 5. At each level prove V_parent(r)=sum_x eq(r,x)V_child(0,x)V_child(1,x) by degree-3 sumcheck; absorb both endpoint values then sample t and reduce to V_child(t,x). Low-bit-first is an I coordinate adaptation. Authenticate the final leaf through dim/E/read/final openings. Public I is computed by V.
I/D: Npad=next_pow2(N), mpad=next_pow2(m), m,N in [1,256]. Query padding equals T[0], dim padding equals zero. Direct masked linear IPA openings enforce padding at a fresh random row point. All padded rows participate in counters.
I/D: add a memory whose trusted contents are 1 at addresses <N and 0 elsewhere. Its read value is fixed to 1 and its address is the linear recombination of committed dims. The same R memory argument excludes table-padding addresses, including for segmented non-power-of-two tables. This costs O(Npad); no SOS performance claim is made for this adapter.
I: backend is a non-hiding one-row vector commitment C=sum_i v_i G_i with the existing hyraxInnerProductProve/Verify. It is NOT C0 and is NOT the legacy two-dimensional Hyrax API. eq coefficients use low-bit-first indexing. Dedicated generator domain and transcript domain: zkGPT.route-b.lasso-prototype.v1. No batch-opening optimization. field_id=curve_id=backend=padding_id=1 specify MCL BN254 (the production project's curve, NOT MCL BN_SNARK1), its scalar field, vector IPA and the fixed T[0]/zero padding rule. Runtime field modulus is checked. Dedicated generators are derived and cached in the new adapter because the production cache deliberately rejects new domains.
I: fixed tagged binary fields, u32 little-endian lengths, canonical MCL Fr/G1 roundtrip and subgroup checks, maximum artifact size 8 MiB, strict end-of-input. Instance includes approved table/profile/layout; artifact supplies no trusted table. A verifier accepts only against an independently supplied instance.
I: new Fiat-Shamir schedule/wire format and PC composition are engineering adaptations. 论文未明确给出，不能从论文直接推出. No claim to inherit a complete noninteractive security theorem, or to provide ZK.

Sources: local ../route_b_audit_20261016/{lasso-primary,zkgpt-author2025}.pdf and RouteB-technical-design.md. Supplement: https://arxiv.org/pdf/1304.3812 §5.3.1 Proposition 2.

## Implementation plan

Goal: independently verify small Lasso/Surge proofs without changing production behavior.
Architecture: new src/lasso subtree; only opt-in registration appended to src/CMakeLists.txt. Reuse gpt_lib's field, generators, Transcript and generic IPA functions through a dedicated adapter. No GKR/LogUp wrapper reuse.

- [x] Preserve remote HEAD/status/instructions and hash every existing src/docs/CMake file.
- [x] Add bounded instance, canonical table digest, selector, commitment and strict codecs; observe a failing build before implementation.
- [x] Add independent sumcheck and Thaler multiplication-tree reduction, ending in authenticated leaves.
- [x] Add prover and independently implemented verifier, query claim output and padding/validity checks.
- [x] Add separate verifier CLI and positive, mutation, parser/resource, complete malicious-prover regeneration fixtures.
- [x] Build opt-in target, invoke independent verifier processes, measure small proofs, run suitable existing lightweight regressions.
- [x] Compare all old file hashes, inspect diff, report A-G and mark any gaps honestly.

Global constraints: no production proofs, no 76,032-query benchmark, no hiding/Task10/11 changes, no C0 cross-binding, no GKR integration, no unified consistency sumcheck, no commits/resets/cleanup of pre-existing changes.
Review focus: non-power-of-two invalid addresses; zero-dimensional m=N=1; stale/out-of-order challenges; canonical field/subgroup decoding and resource caps; fresh dishonest read values/counters with regenerated commitments.
