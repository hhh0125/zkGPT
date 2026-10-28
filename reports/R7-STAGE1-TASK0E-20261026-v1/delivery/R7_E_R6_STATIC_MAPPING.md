# Task0-E actual R6 source correspondence
Status: DESIGNED_STATIC_SOURCE_MAPPING. Source/metadata only; no real graph/proof/parameter/anchor execution.

## Complete structural lists
evidence/R6_FULL12_TERMINAL_PLANS.json has612 ordered records from the sealed FULL12_PLAN. Each preserves layer, role, coordinate count, u32le coordinate-array SHA, first/last index and exact runtime resolver. Roles:276 input-u0,240 input-v0,48 matrix-u,48 matrix-v. First FC pair is part of input-u0/v0; second FC pair is matrix-u/v. All source coordinates, including repeats, are ordered, not deduplicated.
evidence/R6_FULL12_FC_ENDPOINTS.json lists all48 FC layers and their4 terminal roles /2 independent matrix SCs (96 SCs total). The verifier derives both parent points, SC recurrence and X/W endpoints independently; one parent pair cannot stand in for both.
The plan contains coordinate identities/counts, not every integer coordinate array. This task does not reconstruct the enormous full graph. The exact mathematical resolver is mu(i)=circuit[layer].ori_id_u[i] for u ports and ori_id_v[i] for v ports, restricted to q.actual_size, padded with zero to2^|q.point|. Full coordinate bytes must be regenerated/checked against these SHA identities by a future authorized metadata-build step before production. No claim of byte-enumerating those arrays is made.

## Sixteen mapped children
evidence/R6_FULL12_CHILDREN.json contains all16 complete ordered span/bias lists:13 Range groups, Exp-t, Exp-E, W. For child j and the ordered concatenation of spans (offset_h,count_h,bias_h), if i=prefix_h+z, then mu_j(i)=offset_h+z and beta_j(i)=bias_h. Padding is zero, including child W padding. Range groups are sorted (bits,signed), not P-chosen: widths1,11,13,24,32,56,64u,64s,80s,96u,96s,112u,112s. Signed bias is2^(bits-1); reserve/positive-helper -1 biases are retained in exact spans.
Source model_queries.cpp:19-69 constructs plan;26-53 Range groups;55-61 Exp/W;71-91 public and operator-approved W identity;94-97 gathers. No numeric Profile changes.

## Public positions, zero residual and tail
X: all32768 positions m_base[i]=X[i]. Y: all32768 positions m_base[p.output[i]]=Y[i], where verifier reconstructs p.output from final RELU zero-residual wiring (model_queries.cpp:62-68); it is not an assumed contiguous offset or P list. On each1024-wide activation row, columns768..1023 of X/Y must be zero (:77-78).
Constants p.constants map to1 and p.reserved_zero to0 (:83-84); they are reconstructed from registry offsets/reserves (:45-52). Final GKR output is a public ZERO RESIDUAL, separately checked in r6_model_verify.cpp and gkr_verifier.cpp, not permission to publish other endpoint scalars.
Original master actual199348640 / padded268435456. Base padded rows are zero. Future extended-master base view must preserve this virtual zero tail independently of its appended auxiliary slices.

## Internal aggregation and bridge remain separate
gkr_verifier.cpp:601-647 forms all input-port/FC-second-point sigmas and degree2 internal aggregation. :652-738 recomputes public mapping evaluation including second FC terms and checks K(r)*W0(r)=terminal; :744-752 authenticates old input opening; :753-777 exports checked terminals ONLY afterward.
paper_r2/protocol.cpp:38-50 reconstructs checked spans from those trusted claims and ordered circuit coordinates; :84-102 runs local Range/Exp, full GKR (including internal aggregation), new/old bridge, then final mapped-input. mapped_input.cpp:200-292 /337-347 supplies lambdas, bias correction, complete tail kernel, SC and final master opening.
R6 old GKR row-major deterministic PC and newer paper_r1 row/column PC use separate generator/Transcript domains. They are BOTH nonhiding. E's proposal uses one NEW hiding master and typed gather views; no old R6 commitment is made hiding by this design.

## All auxiliary witnesses and future flat-master ceiling
evidence/R6_AUXILIARY_MASTER_EXTENSION.json gives an explicit append-only namespace/offset list of315 fixed base-witness auxiliaries:103 digit arrays,208 read/final arrays and4 Exp helpers. Address/value digit aliasing is preserved; Exp indexed query is a view of x/nativeE, not a replacement table. Challenge-dependent tree/SC/masking messages remain authenticated execution derived from those base entries.
This literal design has714217488 logical entries and2^30 padded entries:34359738368 bytes (32GiB) for ONE Fr array alone. It exceeds old R6 max vector2^28. No allocation or proof was attempted. The naive flat layout is BLOCKED under existing R6 limits and is not an approved production resource plan. This is static arithmetic, not performance optimization or a Profile change.
Base view indices below199348640 remain unchanged; old padding is a virtual-zero view, not a source of appended auxiliary data. Changing master dimension requires typed reductions rather than sending an old28-bit point unchanged to the new30-bit PC.

## Range/Exp future interface
Base512 digits use t_h=min(9,bits-9h), including smaller highest table; reconstruct sum512^h*digit and zero-tail. Signed encoding uses verifier-owned active-mask/bias mapping. Exp LegacyClampOne remains x=min(t,8191), exact8192-entry profile table, selector/delta branch identities, nativeE=E+2^20*(1-active), and public table identity. D/E table4 never authenticates actual512/8192 tables.
The full12 structural mapping and new auxiliary offset manifest are future engineering interfaces. E toy success does not implement any of612 real terminals,96 real matrix SCs,13 true Range groups or real Exp/anchor binding.
