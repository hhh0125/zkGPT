# Protocol1 mathematical and code correspondence (candidate)

2026-10-30. Source: https://www.usenix.org/system/files/usenixsecurity25-qu-zkgpt.pdf Appendix A, Protocol1(6),(7), printed pages2061-2062. This is a minimal implementation correspondence, not a renewed full R7 audit. The printed final selector expression repeats a category coefficient; the code uses independently tested little-endian four-way multilinear selectors rather than copying that expression.

| Relation / responsibility | Actual candidate path |
|---|---|
| Native arithmetic/FC terminals (both ports and matrix points) | `oneblock_gkr.cpp::runHiddenGKR`, `classified_controller.inc` loop over `pending.terminals`, verifier-derived `ori_id_u/ori_id_v`; exact equality with required GKR_MASTER_TERMINAL IDs |
| Actual Range/Exp Lasso plus Memory/Product provenance | `oneblock_lookup.cpp::hiddenRange/hiddenExp`, `VectorOracle::open`, `Commitments::view/defer`; all sources and helper objects constrained by existing native plan |
| Native master coordinate to existing StageC physical root | `classified_layout.cpp::locate`, `partitioned_layout.cpp::baseCoordinate`; original trusted layout/digest preserved |
| Unified virtual X/W/aux/L layout | `unified_input.cpp::pack/validate`; `classified_commitments.cpp::unifiedInputIdentity`; explicit statement X, trusted root category/order, dimensions and aligned offsets; hash bound in FS instance and live authentication |
| Original mapped-claim identity and affine/constant terms | `Commitments::defer/mapDigest/prepareClaim`; all prepare passes before rho; original OPT3B RAM/file map budgets; frozen maps callback-free, exhausted rebuilds digest-checked |
| Preparation of the input sum target and public wiring polynomial | `Commitments::authenticateUnifiedInput`: A=sum_i rho_i(h_i-B_i), K(z)=sum_ij rho_i eq(q_i,j)1[z=offset(root)+index]; multiplicities accumulate; biases/virtual constants removed from target |
| Protocol1(6), actual single global hidden input Sumcheck | `unified_input.cpp::quadraticRound/reduce`; sum_{Boolean z}U(z)K(z)=A, degree2, one common r across complete4N virtual domain; coefficient commitments + recurrence Sigma before each challenge |
| Input Sumcheck final product | `reduce` verifies hidden U(r) times independently computed public K(r) equals the final hidden carry, using existing Sigma products |
| Protocol1(7), classified hidden PC openings | `authenticateUnifiedInput` opens every original physical root at r[:log2(root.padded)] via `rootOpening`, existing `openVector` / trusted `model::opening`, RowMajor Hyrax A.3 |
| Logical concatenation and model chunk stitching | Hidden relation U(r)=selector_X*X_MLE(r[:dX])+sum_k selector_k*root_k_MLE(r[:dk]); all roots included even with zero selector/kernel. No new physical master commitment. W root order remains trusted model chunk order. |
| Original model identity and base tails | Existing constructor `registerModel`/Full12 chunk trust; existing `constrainBaseTails`; no full base-row knowledge registration restored |
| Transcript/no downgrade/independent verification | Explicit OneblockUnified / Full12Unified FS proof types and new suites; `FsVerifierIO` reconstructs challenges from new disk Proof and separately loaded Statement; `finish()` validates footer and EOF |
| Verifier-local coverage | Candidate `oneblock_main.cpp` required inventory and `classified_controller.inc` completion only after all new checks; no peer coverage receipt accepted |

R6 reference `gkr_proof_builder.cpp::buildLassoProof` is input-layer wiring reduction, not the actual Range/Exp Lasso. Its plain input evaluation/opening path is never used as fallback by this candidate.

Storage semantics: zero virtual alignment holes and category tails are implicit and allocated nowhere. Physical root blocks use the complete committed padded vector, with original registered-base/model-tail constraints. Existing lookup helper logical padded values retain their original native constraints; unused padding inside an unregistered helper commitment is not newly asserted to be zero by the virtual layout. Every committed block opens with its original padded dimension. The existing public-X/Y/constants claim also binds native copies of public data, while X is an explicit public logical partition.

No 2^28 private input copy, root folded witness copy, or new mixed val[0] commitment is built. One public K physical-root buffer is used at a time. Streamed coefficients re-read sealed root values; repeated public kernel construction is included in timing. Split equality tables and a sparse suffix map have bounded size. The last scalar-block merging includes crossroot/crosspartition terms for BOTH factors, including public X even where Boolean K_X=0.

Security status: FS_NIZK_CANDIDATE / BLOCKED_CRYPTOGRAPHIC_DESIGN / may_enter_stage2=false. Exact algebra/real P-V testing is engineering evidence; there is no new formal proof of joint knowledge soundness, zero-knowledge simulation or Fiat-Shamir/Sigma/Hyrax composition.
