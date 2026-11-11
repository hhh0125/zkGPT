# R7 five-category MLE audit and bounded U/K/source optimization

Candidate: `5e1a413d84094bc0187f7b6850410965bf799f68`; baseline: `622fa03324bce696c024ab96b5679d3881b67553`.
`production_gate=BLOCKED_CRYPTOGRAPHIC_DESIGN`; `may_enter_stage2=false`. This is an implementation/performance candidate, not cryptographic production acceptance.
Fixed verified workspace: `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`. The shorter path in the task attachment was absent. Candidate/source/parameter/dependency bytes matched the prior accepted snapshot; PERF6 and the original main repository were preserved. See BASELINE_IDENTITY.json and BASELINE_GIT_RESULT.json.

## 1. MLE mathematical audit

For global z, chi_z(r)=product_b (r_b if bit_b(z)=1 else 1-r_b). The contribution of category c is sum over z in [b_c,b_c+n_c) of U[z] chi_z(r). A category-local MLE is insufficient at an arbitrary offset.
Each storage actual interval is greedily partitioned into positive, globally aligned dyadic tiles of size <=2^20. Advancing the end of each tile proves coverage and disjointness; tile selection never crosses a storage actual boundary or adds padded cells. Chunk/category/global zero gaps remain implicit. For tile a divisible by 2^ell, chi_(a+j)=selector(a,ell,r_high)*chi_j(r_low).
Local folds contract low bits with the same challenges. Once tiles collapse to the same global suffix coordinate, BOTH U and K scalars are summed before their quadratic product; this preserves cross terms across chunks/tiles/categories. Source maps use absolute global coordinates. Compact physical matrix coefficients in single_hyrax.cpp mappedQueryTerms preserve row carry; category and matrix tails are zero. See MLE_AUDIT.md.
The new verifier K endpoint computes sum_i rho_i sum_nonconstant_j chi_j(source_point_i)*chi_(phi_i(j))(global_point). Aligned simple blocks reuse TerminalBasis. For power-of-two logical steps and positive/negative power strides, source quotient-aligned blocks use a two-state carry/borrow recurrence for exact joint bit products at arbitrarily unaligned global coordinates. Zero stride sums free source weights to one. Non-power steps/strides and short blocks enumerate exact terms. See CARRY_MLE_AUDIT.md and2200 mixed-dimension,Boolean/signed,32-bit boundary and invalid-range tests. No division, no witness-dependent admission, no changes to bias removal, claims, messages or challenge order. Callback fallback hashes the exact consumed map and rejects mutation.
Tests: 220 seeded arbitrary-offset cases, 1,528 round comparisons, Dense==Streaming==Cached every round; local U/K folds, endpoint and physical mapped coefficients; empty/one/power/nonpower categories, small chunks, zero tails and 0/1/negative points. A further 220 K endpoint cases cover positive/negative/zero stride, step omissions, repeats, bias, cross-category and changed maps. A real bounded larger proof is replayed by dense and direct witness-free V paths with identical frame count and transcript prefix.
Full Release CTest: **119/119 PASS** (ctest-carry-final.log). Actual bounded X algebraic negatives, omitted claim/modified handle/PC/map probes passed. Full Oneblock/Full12 malicious CLI proof attacks: **NOT_RUN**; bounded attacks are not represented as full-scale attacks.

## 2. U Scan attribution

Historical 13.7849s (PERF4) and 536.0176s (five-category) are not controlled A/B: the former also had a separately timed 29.4731s retained-state fold, different rounds/configuration, and 11,576,840,712 retained capacity bytes. The latter bypassed retained state for callback inputs and repeatedly contracted original U in u_streaming.cpp quadraticRoundStreaming/Weights::dot. category_auth.cpp provides scalar callback Inputs; legacy pointer-backed FoldedU did not retain them.
Historical five-category counters: 30 rounds, 21,426,524,640 U coordinates, 685,648,788,480 scalar payload bytes, 20,712,447,656 contraction Fr multiplications and 21,426,690,816 additions. Historical scan CPU1173.4177s differs from wall536.0176s because worker work overlaps. These bytes are scalar payload demand, not DRAM/disk traffic. See HISTORICAL_U_ATTRIBUTION.json and original raw log references.
Final FoldedU admits only a derived tile level whose actual lengths plus metadata fit 512MiB. It captures contractions already computed by the admission-round scan, then folds retained tile vectors in place after each challenge. No extra original scan, full U/val0 copy, global padding vector or private U file. PERF4_U_STATE.original_read_coordinates covers state advance only; deferred admission reuses scan reads, reported by U_SCAN_DETAIL instead. Immutable storage token/offset/bounds bind each borrowed reader; retained descriptors do not own allocating callbacks. Failed lifecycle/admission poisons state.
Tile geometry is enumerated once per authentication. Weight tables are shared by prefix width within each scan round, not reused across distinct sampled prefixes. The new cache removes later original contractions and their weight reconstruction. Source claims and authentication identities remain distinct.
All strict runs have read-profile0. Separate Full12 D1 uses baseline U/K plus read-profile1; it is excluded from medians. Optional <=1024-Fr profiling buffers measure reading+mapping jointly without changing original callback call counts. Small contractions are untimed; CPU residual includes their reading, Fr operations and dispatch. Physical chunk read/cache traffic is **NOT_MEASURED_NO_BULK_CHUNK_API**, never zero. Current U readers are resident scalar readers; U explicit file IO is zero. Exact original/retained reads, descriptor counts, coverage and operations are reported.
One early diagnostic prefetch bug amplified reads; it was stopped, retained as failed Oneblock A1, reproduced RED and corrected with exact callback-count GREEN tests. The strict final Oneblock v2 A samples are A1/A2/A3 in ab-v2. All v1 ab/* runs are superseded diagnostics after measured endpoint eligibility22.3%; final ab-v2 samples use a fresh single-binary freeze with carry/borrow endpoint, whose old whole Oneblock proof replay admitted98.4% and passed all5346 coverage relations. No failed or superseded run is counted in final medians.

Separate Full12 D1 read/map diagnostic: original reads 21426524640, timed 14284328960 (66.667% coverage); aggregate profiled worker wall 278.859300s, worker CPU 279.008900s; enclosing scan wall 502.595550s, CPU 1125.155200s. Worker wall sums overlap and are not scan wall. CPU residual 846.146400s contains untimed reads/Fr/dispatch. Do not extrapolate measured read CPU to untimed coverage or treat it as pure mapping CPU. This diagnostic is excluded from all strict A/B medians.

## 3. Algorithms, reads, cache and memory

Times below are seconds, displayed as median [min,max]; spread. Counts and bytes use the same format. Complete per-run/per-round counters are in PERFORMANCE_SUMMARY.json. U scan is inside Unified Sumcheck; state capture, advance folds, K access and products are separate subphases inside that same enclosing timer. Do not add nested totals twice.

### oneblock U and memory

| Metric | A | B |
|---|---:|---:|
| Original scalar reads | 1800820728.000 [1800820728.000, 1800820728.000]; spread 0.000 | 200091192.000 [200091192.000, 200091192.000]; spread 0.000 |
| Original scalar payload bytes | 57626263296.000 [57626263296.000, 57626263296.000]; spread 0.000 | 6402918144.000 [6402918144.000, 6402918144.000]; spread 0.000 |
| Retained scalar reads | 0.000 [0.000, 0.000]; spread 0.000 | 16675593.000 [16675593.000, 16675593.000]; spread 0.000 |
| U scan wall | 45.394 [44.333, 45.663]; spread 1.329 | 10.999 [10.819, 11.426]; spread 0.607 |
| U scan CPU | 98.967 [94.235, 99.056]; spread 4.821 | 10.996 [10.817, 11.423]; spread 0.606 |
| Weight construction wall | 0.082 [0.081, 0.084]; spread 0.003 | 0.000 [0.000, 0.000]; spread 0.000 |
| State capture wall | 0.000 [0.000, 0.000]; spread 0.000 | 0.162 [0.161, 0.174]; spread 0.013 |
| State capture CPU | 0.000 [0.000, 0.000]; spread 0.000 | 0.163 [0.162, 0.175]; spread 0.013 |
| Contraction Fr multiplications | 1734133724.000 [1734133724.000, 1734133724.000]; spread 0.000 | 133394128.000 [133394128.000, 133394128.000]; spread 0.000 |
| Contraction Fr additions | 1800833021.000 [1800833021.000, 1800833021.000]; spread 0.000 | 183416926.000 [183416926.000, 183416926.000]; spread 0.000 |
| State hit tile visits | 0.000 [0.000, 0.000]; spread 0.000 | 3360.000 [3360.000, 3360.000]; spread 0.000 |
| State miss tile visits | 3780.000 [3780.000, 3780.000]; spread 0.000 | 420.000 [420.000, 420.000]; spread 0.000 |
| P RSS GiB | 3.172 [3.165, 3.173]; spread 0.007 | 3.234 [3.232, 3.234]; spread 0.002 |
| V RSS GiB | 0.615 [0.613, 0.616]; spread 0.003 | 0.615 [0.613, 0.616]; spread 0.003 |

Per-round original/retained reads and scan wall, oneblock (median of three):
| Round | A original | B original | B retained | A wall | B wall |
|---:|---:|---:|---:|---:|---:|
| 0 | 66697064 | 66697064 | 0 | 2.748820 | 2.678450 |
| 1 | 66697064 | 66697064 | 0 | 4.263380 | 4.193280 |
| 2 | 66697064 | 66697064 | 0 | 3.833970 | 3.804620 |
| 3 | 66697064 | 0 | 8337133 | 3.659300 | 0.140455 |
| 4 | 66697064 | 0 | 4168567 | 3.477350 | 0.067736 |
| 5 | 66697064 | 0 | 2084285 | 3.396620 | 0.035974 |
| 6 | 66697064 | 0 | 1042145 | 3.394310 | 0.016516 |
| 7 | 66697064 | 0 | 521076 | 1.544650 | 0.008145 |
| 8 | 66697064 | 0 | 260545 | 1.463320 | 0.004248 |
| 9 | 66697064 | 0 | 130282 | 0.984766 | 0.002153 |
| 10 | 66697064 | 0 | 65153 | 0.938728 | 0.001152 |
| 11 | 66697064 | 0 | 32591 | 0.940612 | 0.000590 |
| 12 | 66697064 | 0 | 16313 | 0.933478 | 0.000320 |
| 13 | 66697064 | 0 | 8177 | 0.933821 | 0.000192 |
| 14 | 66697064 | 0 | 4112 | 0.933787 | 0.000134 |
| 15 | 66697064 | 0 | 2082 | 0.958409 | 0.000084 |
| 16 | 66697064 | 0 | 1070 | 0.940110 | 0.000071 |
| 17 | 66697064 | 0 | 567 | 0.992544 | 0.000062 |
| 18 | 66697064 | 0 | 318 | 0.954954 | 0.000055 |
| 19 | 66697064 | 0 | 197 | 0.946241 | 0.000054 |
| 20 | 66697064 | 0 | 140 | 0.937711 | 0.000054 |
| 21 | 66697064 | 0 | 140 | 0.959089 | 0.000069 |
| 22 | 66697064 | 0 | 140 | 0.961527 | 0.000063 |
| 23 | 66697064 | 0 | 140 | 0.962471 | 0.000059 |
| 24 | 66697064 | 0 | 140 | 0.975481 | 0.000060 |
| 25 | 66697064 | 0 | 140 | 0.963045 | 0.000061 |
| 26 | 66697064 | 0 | 140 | 0.978011 | 0.000059 |

Combined oneblock memory: witness snapshot `[{"native_master_size": 16642424, "native_master_capacity_bytes": 532557568, "helper_materialized_elements": 50054640, "helper_capacity_bytes": 1601748480, "other_native_layer_capacity_bytes": 110434304, "snapshot": "all-witness-and-helpers-ready", "capacity_not_RSS": true}]`; U retained capacity `[{"enabled": true, "failed": false, "budget_bytes": 536870912, "capacity_peak_bytes": 533595616, "live_bytes": 4480, "roots_materialized": 140, "advances": 27, "original_read_coordinates": 0, "cache_fold_read_coordinates": 33348252, "fold_input_coordinates": 33348252, "fold_write_coordinates": 33348392, "Fr_multiplications": 16674126, "Fr_additions": 33348252, "fold_wall_seconds": 0.665542, "fold_cpu_seconds": 0.665404, "deferred_callback_mode": true, "checkpoint_round": 2, "private_files": 0}]`. K total tracked peak `571059696` bytes and builder `33554432` bytes; source table max `262144` bytes. These capacities are separate, nested/transient lifetimes differ, and they must not be added to predict RSS. Other native-layer/helper capacities are snapshot values, not an exhaustive lifetime peak. Whole process P/V RSS comes from wait4.

### full12 U and memory

| Metric | A | B |
|---|---:|---:|
| Original scalar reads | 21426524640.000 [21426524640.000, 21426524640.000]; spread 0.000 | 4999522416.000 [4999522416.000, 4999522416.000]; spread 0.000 |
| Original scalar payload bytes | 685648788480.000 [685648788480.000, 685648788480.000]; spread 0.000 | 159984717312.000 [159984717312.000, 159984717312.000]; spread 0.000 |
| Retained scalar reads | 0.000 [0.000, 0.000]; spread 0.000 | 11170271.000 [11170271.000, 11170271.000]; spread 0.000 |
| U scan wall | 488.595 [485.948, 496.603]; spread 10.655 | 272.719 [270.609, 273.957]; spread 3.348 |
| U scan CPU | 1074.306 [1074.006, 1105.983]; spread 31.977 | 272.640 [270.536, 273.882]; spread 3.346 |
| Weight construction wall | 0.100 [0.099, 0.102]; spread 0.003 | 0.000 [0.000, 0.000]; spread 0.000 |
| State capture wall | 0.000 [0.000, 0.000]; spread 0.000 | 0.290 [0.286, 0.296]; spread 0.010 |
| State capture CPU | 0.000 [0.000, 0.000]; spread 0.000 | 0.296 [0.290, 0.301]; spread 0.010 |
| Contraction Fr multiplications | 20712447656.000 [20712447656.000, 20712447656.000]; spread 0.000 | 4285304928.000 [4285304928.000, 4285304928.000]; spread 0.000 |
| Contraction Fr additions | 21426690816.000 [21426690816.000, 21426690816.000]; spread 0.000 | 4988362770.000 [4988362770.000, 4988362770.000]; spread 0.000 |
| State hit tile visits | 0.000 [0.000, 0.000]; spread 0.000 | 24242.000 [24242.000, 24242.000]; spread 0.000 |
| State miss tile visits | 31620.000 [31620.000, 31620.000]; spread 0.000 | 7378.000 [7378.000, 7378.000]; spread 0.000 |
| P RSS GiB | 36.034 [36.033, 36.035]; spread 0.001 | 36.034 [36.032, 36.034]; spread 0.002 |
| V RSS GiB | 5.841 [5.837, 5.844]; spread 0.007 | 5.837 [5.836, 5.881]; spread 0.046 |

Per-round original/retained reads and scan wall, full12 (median of three):
| Round | A original | B original | B retained | A wall | B wall |
|---:|---:|---:|---:|---:|---:|
| 0 | 714217488 | 714217488 | 0 | 28.739600 | 28.868500 |
| 1 | 714217488 | 714217488 | 0 | 46.088300 | 46.623700 |
| 2 | 714217488 | 714217488 | 0 | 41.557000 | 42.566000 |
| 3 | 714217488 | 714217488 | 0 | 39.950800 | 40.326600 |
| 4 | 714217488 | 714217488 | 0 | 38.673400 | 39.116300 |
| 5 | 714217488 | 714217488 | 0 | 36.442900 | 37.698000 |
| 6 | 714217488 | 714217488 | 0 | 36.098900 | 37.150100 |
| 7 | 714217488 | 0 | 5579827 | 11.001100 | 0.130176 |
| 8 | 714217488 | 0 | 2789916 | 11.212900 | 0.043771 |
| 9 | 714217488 | 0 | 1394961 | 9.578750 | 0.021950 |
| 10 | 714217488 | 0 | 697484 | 9.654170 | 0.011352 |
| 11 | 714217488 | 0 | 348747 | 9.381390 | 0.005967 |
| 12 | 714217488 | 0 | 174412 | 9.299090 | 0.003317 |
| 13 | 714217488 | 0 | 87262 | 9.445120 | 0.001986 |
| 14 | 714217488 | 0 | 43705 | 9.342560 | 0.001240 |
| 15 | 714217488 | 0 | 21944 | 9.377320 | 0.000962 |
| 16 | 714217488 | 0 | 11090 | 9.577470 | 0.000738 |
| 17 | 714217488 | 0 | 5686 | 9.506530 | 0.000594 |
| 18 | 714217488 | 0 | 3007 | 9.416600 | 0.000516 |
| 19 | 714217488 | 0 | 1690 | 9.582520 | 0.000485 |
| 20 | 714217488 | 0 | 1054 | 9.416980 | 0.000487 |
| 21 | 714217488 | 0 | 1054 | 9.461950 | 0.000501 |
| 22 | 714217488 | 0 | 1054 | 9.634720 | 0.000499 |
| 23 | 714217488 | 0 | 1054 | 9.316440 | 0.000496 |
| 24 | 714217488 | 0 | 1054 | 9.310750 | 0.000497 |
| 25 | 714217488 | 0 | 1054 | 9.618220 | 0.000496 |
| 26 | 714217488 | 0 | 1054 | 9.325000 | 0.000519 |
| 27 | 714217488 | 0 | 1054 | 9.433080 | 0.000528 |
| 28 | 714217488 | 0 | 1054 | 9.454090 | 0.000534 |
| 29 | 714217488 | 0 | 1054 | 9.329720 | 0.000562 |

Combined full12 memory: witness snapshot `[{"native_master_size": 199348640, "native_master_capacity_bytes": 6379156480, "helper_materialized_elements": 514868848, "helper_capacity_bytes": 16475803136, "other_native_layer_capacity_bytes": 1325211648, "snapshot": "all-witness-and-helpers-ready", "capacity_not_RSS": true}]`; U retained capacity `[{"enabled": true, "failed": false, "budget_bytes": 536870912, "capacity_peak_bytes": 357244992, "live_bytes": 33728, "roots_materialized": 1054, "advances": 30, "original_read_coordinates": 0, "cache_fold_read_coordinates": 22317192, "fold_input_coordinates": 22317192, "fold_write_coordinates": 22318246, "Fr_multiplications": 11158596, "Fr_additions": 22317192, "fold_wall_seconds": 0.368486, "fold_cpu_seconds": 0.368463, "deferred_callback_mode": true, "checkpoint_round": 6, "private_files": 0}]`. K total tracked peak `573034176` bytes and builder `33554432` bytes; source table max `1048576` bytes. These capacities are separate, nested/transient lifetimes differ, and they must not be added to predict RSS. Other native-layer/helper capacities are snapshot values, not an exhaustive lifetime peak. Whole process P/V RSS comes from wait4.

## 4. K optimization

Initial category K is generated per tile from rho-fixed prepared runs. Frozen map plans are prepared once, but each tile visits relevant runs and generates its own coefficient range; rebuild fallback repeats that work when retention budgets cannot admit a root. Per-claim split weight objects fit a shared budget (Oneblock32MiB, Full12256MiB); over-budget claims use scratch tables. Claims with different identities/points are not merged. No value-based sharing or new cross-claim cache was introduced.
The prior RAM vectors shrank size while preserving capacity, preventing later admissions. B compacts their live ranges and releases capacity; transient old+new is included in K RAM peak. Copy time is inside K fold. Category copy temporary<=16MiB; K builder remains32MiB, retained RAM budget512MiB, file4GiB plus metadata/scratch. K file reads/writes/IO are reported separately, with IO nested in fold/access time. U and K reuse the exact public tile descriptors and challenge prefix, but retain different states; no ownership coupling was introduced.
Verifier direct endpoint avoids dense tile coefficient generation/folding when public geometry has >=1024 eligible coordinates and >=75% eligibility. Otherwise it retains the exact previous endpoint. See CATEGORY_K_ENDPOINT for admission/time/additional table peak; old PERF2_K_TOTAL is incomplete for this new path and is not its time/memory authority.
| Scope / Metric | A | B |
|---|---:|---:|
| oneblock K_initial_build_seconds | 7.114 [7.017, 7.120]; spread 0.104 | 6.984 [6.975, 7.118]; spread 0.143 |
| oneblock K_fold_wall_seconds | 9.832 [9.764, 9.855]; spread 0.091 | 10.670 [9.794, 11.076]; spread 1.283 |
| oneblock K_fold_cpu_seconds | 7.385 [7.367, 7.522]; spread 0.155 | 8.708 [7.459, 9.098]; spread 1.639 |
| oneblock K_initial_builds | 140.000 [140.000, 140.000]; spread 0.000 | 140.000 [140.000, 140.000]; spread 0.000 |
| oneblock fallback_rebuild_calls | 0.000 [0.000, 0.000]; spread 0.000 | 0.000 [0.000, 0.000]; spread 0.000 |
| oneblock K_RAM_peak_bytes | 571059696.000 [571059696.000, 571059696.000]; spread 0.000 | 571059696.000 [571059696.000, 571059696.000]; spread 0.000 |
| oneblock K_builder_peak_bytes | 33554432.000 [33554432.000, 33554432.000]; spread 0.000 | 33554432.000 [33554432.000, 33554432.000]; spread 0.000 |
| oneblock K_file_read_bytes | 6389772672.000 [6389772672.000, 6389772672.000]; spread 0.000 | 6389772672.000 [6389772672.000, 6389772672.000]; spread 0.000 |
| oneblock K_file_write_bytes | 3194877792.000 [3194877792.000, 3194877792.000]; spread 0.000 | 3194877792.000 [3194877792.000, 3194877792.000]; spread 0.000 |
| oneblock K_io_wall_seconds | 6.918 [6.694, 6.936]; spread 0.242 | 6.726 [6.602, 6.744]; spread 0.142 |
| full12 K_initial_build_seconds | 216.809 [214.652, 218.448]; spread 3.796 | 191.679 [190.675, 194.056]; spread 3.381 |
| full12 K_fold_wall_seconds | 88.521 [87.608, 89.309]; spread 1.701 | 81.582 [79.536, 83.215]; spread 3.679 |
| full12 K_fold_cpu_seconds | 73.426 [72.622, 74.241]; spread 1.619 | 67.723 [64.537, 69.570]; spread 5.034 |
| full12 K_initial_builds | 3277.000 [3277.000, 3277.000]; spread 0.000 | 2709.000 [2709.000, 2709.000]; spread 0.000 |
| full12 fallback_rebuild_calls | 2223.000 [2223.000, 2223.000]; spread 0.000 | 1655.000 [1655.000, 1655.000]; spread 0.000 |
| full12 K_RAM_peak_bytes | 573059008.000 [573059008.000, 573059008.000]; spread 0.000 | 573034176.000 [573034176.000, 573034176.000]; spread 0.000 |
| full12 K_builder_peak_bytes | 33554432.000 [33554432.000, 33554432.000]; spread 0.000 | 33554432.000 [33554432.000, 33554432.000]; spread 0.000 |
| full12 K_file_read_bytes | 36744429216.000 [36744429216.000, 36744429216.000]; spread 0.000 | 36371296896.000 [36371296896.000, 36371296896.000]; spread 0.000 |
| full12 K_file_write_bytes | 18372034912.000 [18372034912.000, 18372034912.000]; spread 0.000 | 18185471072.000 [18185471072.000, 18185471072.000]; spread 0.000 |
| full12 K_io_wall_seconds | 44.504 [42.548, 45.386]; spread 2.838 | 43.472 [42.743, 44.294]; spread 1.550 |

## 5. Source Handles memory

The independent q28 benchmark uses the same synthetic resident scalar oracle, point and result in fresh fixed-affinity processes, alternating reference/stream three times. It is separate from strict U/K A/B (both source stream0). Source stream1 splits low/high weight tables, contracts one low block at a time and scales by the high weight. At most1MiB weight tables for <=28 dimensions replace the full8GiB table. Every padded source value is read, including nonzero materialized helper padding; point, blind, handle protocol and transcript are unchanged.
| Mode | Oracle wall median [min,max] s | Process RSS median KiB | Weight table bytes |
|---|---:|---:|---:|
| reference | 31.618300 [30.611100,31.625800] | 8393124 | 8589934592 |
| stream | 15.294600 [14.221100,15.605600] | 8952 | 1048576 |

All six outputs equal `21735611750723472264`. This benchmark RSS is not Full12 RSS. Combined C1 runs validate streamed source handles inside each actual protocol; their process/phase metrics are reported below.

## 6. Strict PERF6 A/B and combined validation

Same final executable/source/toolchain/parameter bytes, Statement, five-category layout, map identity, relation set, compiler options, PERF6 configuration, four threads and CPUs0-3. A disables category U cache/K compaction/direct K endpoint, B enables them; both source stream0 and profile0. Fresh independent P and witness-free V; original Statement copied read-only into each run. Alternating Oneblock v2 A1 B1 A2 B2 A3 B3, then Full12 B1 A1 B2 A2 B3 A3. See AB_DESIGN.md and per-run ENVIRONMENT.json.
Each accepted run checks P/V complete FS/proof digest agreement, one global commitment, exact five categories, no root padding/alignment gap, unchanged required relation IDs/families and authenticated X. Blinding and sessions are fresh; equality of independent randomized proof digests is neither expected nor required.
| Scope / Metric | A median [min,max]; spread | B median [min,max]; spread |
|---|---:|---:|
| oneblock P wall_seconds | 504.878 [503.906, 512.052]; spread 8.146 | 470.808 [469.977, 474.280]; spread 4.302 |
| oneblock P peak_rss_kib | 3326352.000 [3319240.000, 3326940.000]; spread 7700.000 | 3390580.000 [3389076.000, 3390832.000]; spread 1756.000 |
| oneblock V wall_seconds | 208.250 [208.233, 209.669]; spread 1.435 | 200.493 [198.404, 200.981]; spread 2.576 |
| oneblock V peak_rss_kib | 645208.000 [642952.000, 646040.000]; spread 3088.000 | 644876.000 [642896.000, 646076.000]; spread 3180.000 |
| oneblock Proof bytes | 143266980.000 [143266980.000, 143266980.000]; spread 0.000 | 143266980.000 [143266980.000, 143266980.000]; spread 0.000 |
oneblock P median B/A elapsed change: **-6.748%**.
oneblock V median B/A elapsed change: **-3.725%**.

### oneblock phase times (nested timers must not be summed)

| Role / phase | A | B |
|---|---:|---:|
| A3_TIMING.P[0].inner_product_wall_seconds | 7.047 [6.950, 7.158]; spread 0.209 | 7.024 [6.960, 7.029]; spread 0.069 |
| A3_TIMING.P[0].knowledge_wall_seconds | 1.265 [1.258, 1.334]; spread 0.076 | 1.258 [1.256, 1.273]; spread 0.016 |
| A3_TIMING.V[0].inner_product_wall_seconds | 0.810 [0.809, 0.839]; spread 0.029 | 0.823 [0.815, 0.829]; spread 0.014 |
| A3_TIMING.V[0].knowledge_wall_seconds | 0.752 [0.743, 0.762]; spread 0.019 | 0.742 [0.739, 0.743]; spread 0.004 |
| COMPACT_QUERY.P[0].preparation_seconds | 6.398 [6.386, 6.437]; spread 0.051 | 6.394 [6.344, 6.435]; spread 0.090 |
| COMPACT_QUERY.P[0].total_seconds | 15.561 [15.477, 15.809]; spread 0.332 | 15.602 [15.539, 15.618]; spread 0.079 |
| COMPACT_QUERY.V[0].preparation_seconds | 0.146 [0.145, 0.150]; spread 0.005 | 0.145 [0.145, 0.147]; spread 0.003 |
| COMPACT_QUERY.V[0].total_seconds | 3.859 [3.806, 4.045]; spread 0.239 | 3.828 [3.825, 3.978]; spread 0.154 |
| OPT3A_AUTH.P[0].kernel_wall_seconds | 0.000 [0.000, 0.000]; spread 0.000 | 0.000 [0.000, 0.000]; spread 0.000 |
| OPT3A_AUTH.P[0].partial_wall_seconds | 0.000 [0.000, 0.000]; spread 0.000 | 0.000 [0.000, 0.000]; spread 0.000 |
| OPT3A_AUTH.P[0].prepare_wall_seconds | 14.677 [14.499, 14.764]; spread 0.265 | 14.568 [14.449, 14.715]; spread 0.266 |
| OPT3A_AUTH.V[0].kernel_wall_seconds | 0.000 [0.000, 0.000]; spread 0.000 | 0.000 [0.000, 0.000]; spread 0.000 |
| OPT3A_AUTH.V[0].partial_wall_seconds | 0.000 [0.000, 0.000]; spread 0.000 | 0.000 [0.000, 0.000]; spread 0.000 |
| OPT3A_AUTH.V[0].prepare_wall_seconds | 14.665 [14.643, 14.798]; spread 0.155 | 14.834 [14.830, 14.945]; spread 0.115 |
| P.phase.all-classified-roots-prechallenge-seal | 25.116 [24.805, 28.134]; spread 3.329 | 25.077 [24.831, 25.191]; spread 0.361 |
| P.phase.all-native-GKR | 16.813 [15.920, 20.379]; spread 4.458 | 15.784 [15.247, 16.492]; spread 1.246 |
| P.phase.all-source-original-handles | 3.808 [3.769, 3.862]; spread 0.093 | 3.792 [3.786, 3.805]; spread 0.019 |
| P.phase.classified-physical-zero-tails | 0.000 [0.000, 0.000]; spread 0.000 | 0.000 [0.000, 0.000]; spread 0.000 |
| P.phase.classified-public-Y-constants-and-input-definition | 0.018 [0.018, 0.019]; spread 0.001 | 0.018 [0.017, 0.018]; spread 0.001 |
| P.phase.frozen-witness-and-prechallenge-lookup | 28.617 [28.525, 28.746]; spread 0.222 | 28.695 [28.593, 28.797]; spread 0.204 |
| P.phase.native-Range-Exp-Lasso-Memory-Product | 297.164 [296.900, 298.045]; spread 1.145 | 297.250 [296.999, 298.653]; spread 1.654 |
| P.phase.protocol1-unified-input-sumcheck-and-opening | 109.084 [108.251, 109.818]; spread 1.567 | 76.516 [75.061, 77.349]; spread 2.287 |
| SINGLE_HYRAX_COMMIT.P[0].seconds | 22.341 [22.120, 25.448]; spread 3.328 | 22.341 [22.150, 22.502]; spread 0.353 |
| UNIFIED_SUMCHECK.P[0].cpu_seconds | 127.145 [122.058, 127.151]; spread 5.093 | 41.076 [39.600, 41.883]; spread 2.282 |
| UNIFIED_SUMCHECK.P[0].wall_seconds | 76.527 [74.984, 76.805]; spread 1.820 | 43.617 [42.482, 44.432]; spread 1.950 |
| UNIFIED_SUMCHECK.V[0].cpu_seconds | 9.210 [9.146, 9.245]; spread 0.099 | 0.487 [0.486, 0.560]; spread 0.074 |
| UNIFIED_SUMCHECK.V[0].wall_seconds | 9.212 [9.148, 9.248]; spread 0.100 | 0.487 [0.487, 0.560]; spread 0.073 |
| V.phase.all-classified-roots-prechallenge-seal | 1.845 [1.832, 1.857]; spread 0.025 | 1.833 [1.827, 1.841]; spread 0.014 |
| V.phase.all-native-GKR | 4.534 [4.493, 4.558]; spread 0.065 | 4.538 [4.505, 4.604]; spread 0.099 |
| V.phase.all-source-original-handles | 1.131 [1.121, 1.196]; spread 0.075 | 1.144 [1.137, 1.144]; spread 0.008 |
| V.phase.classified-physical-zero-tails | 0.000 [0.000, 0.000]; spread 0.000 | 0.000 [0.000, 0.000]; spread 0.000 |
| V.phase.classified-public-Y-constants-and-input-definition | 0.013 [0.013, 0.014]; spread 0.000 | 0.013 [0.013, 0.014]; spread 0.000 |
| V.phase.frozen-witness-and-prechallenge-lookup | 0.533 [0.520, 0.535]; spread 0.015 | 0.546 [0.545, 0.553]; spread 0.008 |
| V.phase.native-Range-Exp-Lasso-Memory-Product | 143.528 [142.959, 144.584]; spread 1.625 | 143.623 [141.998, 144.065]; spread 2.067 |
| V.phase.protocol1-unified-input-sumcheck-and-opening | 30.413 [30.248, 30.477]; spread 0.229 | 21.811 [21.743, 21.941]; spread 0.198 |

Combined C1 oneblock: P `473.714500s`, V `200.836870s`, P RSS `3388808KiB`, V RSS `642480KiB`, proof `143266980` bytes. Single combined run, not a three-repeat performance median.

| full12 P wall_seconds | 3235.185 [3226.197, 3283.671]; spread 57.474 | 3016.461 [2984.174, 3019.852]; spread 35.678 |
| full12 P peak_rss_kib | 37784256.000 [37783756.000, 37785136.000]; spread 1380.000 | 37784112.000 [37782220.000, 37784212.000]; spread 1992.000 |
| full12 V wall_seconds | 793.213 [780.799, 795.086]; spread 14.287 | 696.409 [696.368, 701.235]; spread 4.868 |
| full12 V peak_rss_kib | 6125228.000 [6120352.000, 6127408.000]; spread 7056.000 | 6120440.000 [6119232.000, 6167076.000]; spread 47844.000 |
| full12 Proof bytes | 238413515.000 [238413515.000, 238413515.000]; spread 0.000 | 238413515.000 [238413515.000, 238413515.000]; spread 0.000 |
full12 P median B/A elapsed change: **-6.761%**.
full12 V median B/A elapsed change: **-12.204%**.

### full12 phase times (nested timers must not be summed)

| Role / phase | A | B |
|---|---:|---:|
| A3_TIMING.P[0].inner_product_wall_seconds | 26.959 [26.951, 27.132]; spread 0.180 | 27.149 [26.961, 27.611]; spread 0.650 |
| A3_TIMING.P[0].knowledge_wall_seconds | 4.590 [4.467, 4.612]; spread 0.145 | 4.561 [4.526, 4.717]; spread 0.190 |
| A3_TIMING.V[0].inner_product_wall_seconds | 2.671 [2.664, 2.801]; spread 0.137 | 2.671 [2.658, 2.682]; spread 0.024 |
| A3_TIMING.V[0].knowledge_wall_seconds | 2.586 [2.518, 2.643]; spread 0.125 | 2.522 [2.506, 2.524]; spread 0.019 |
| COMPACT_QUERY.P[0].preparation_seconds | 63.406 [62.491, 63.560]; spread 1.069 | 64.314 [64.011, 64.565]; spread 0.554 |
| COMPACT_QUERY.P[0].total_seconds | 98.480 [97.324, 98.785]; spread 1.461 | 99.706 [98.921, 100.422]; spread 1.501 |
| COMPACT_QUERY.V[0].preparation_seconds | 0.376 [0.374, 0.384]; spread 0.010 | 0.375 [0.375, 0.376]; spread 0.002 |
| COMPACT_QUERY.V[0].total_seconds | 14.218 [13.938, 14.244]; spread 0.306 | 13.989 [13.957, 14.186]; spread 0.229 |
| OPT3A_AUTH.P[0].kernel_wall_seconds | 0.000 [0.000, 0.000]; spread 0.000 | 0.000 [0.000, 0.000]; spread 0.000 |
| OPT3A_AUTH.P[0].partial_wall_seconds | 0.000 [0.000, 0.000]; spread 0.000 | 0.000 [0.000, 0.000]; spread 0.000 |
| OPT3A_AUTH.P[0].prepare_wall_seconds | 158.229 [158.117, 161.351]; spread 3.234 | 158.183 [157.533, 158.745]; spread 1.212 |
| OPT3A_AUTH.V[0].kernel_wall_seconds | 0.000 [0.000, 0.000]; spread 0.000 | 0.000 [0.000, 0.000]; spread 0.000 |
| OPT3A_AUTH.V[0].partial_wall_seconds | 0.000 [0.000, 0.000]; spread 0.000 | 0.000 [0.000, 0.000]; spread 0.000 |
| OPT3A_AUTH.V[0].prepare_wall_seconds | 159.766 [159.480, 159.849]; spread 0.369 | 160.630 [159.232, 161.494]; spread 2.262 |
| P.phase.all-classified-roots-prechallenge-seal | 249.819 [246.539, 250.636]; spread 4.097 | 248.361 [247.286, 249.837]; spread 2.551 |
| P.phase.all-native-GKR | 210.101 [204.448, 241.585]; spread 37.137 | 233.767 [210.295, 233.849]; spread 23.554 |
| P.phase.all-source-original-handles | 54.591 [54.444, 55.278]; spread 0.834 | 54.752 [54.726, 55.156]; spread 0.430 |
| P.phase.classified-physical-zero-tails | 0.000 [0.000, 0.000]; spread 0.000 | 0.000 [0.000, 0.001]; spread 0.000 |
| P.phase.classified-public-Y-constants-and-input-definition | 0.017 [0.017, 0.017]; spread 0.000 | 0.017 [0.017, 0.018]; spread 0.001 |
| P.phase.frozen-witness-and-prechallenge-lookup | 306.735 [303.732, 306.764]; spread 3.032 | 305.519 [304.446, 308.239]; spread 3.793 |
| P.phase.native-Range-Exp-Lasso-Memory-Product | 1145.480 [1141.940, 1146.080]; spread 4.140 | 1148.460 [1145.590, 1151.760]; spread 6.170 |
| P.phase.protocol1-unified-input-sumcheck-and-opening | 1178.590 [1177.880, 1192.980]; spread 15.100 | 931.976 [930.418, 933.105]; spread 2.687 |
| SINGLE_HYRAX_COMMIT.P[0].seconds | 239.031 [235.798, 239.594]; spread 3.796 | 237.493 [236.517, 239.070]; spread 2.553 |
| UNIFIED_SUMCHECK.P[0].cpu_seconds | 1479.300 [1473.410, 1514.690]; spread 41.280 | 644.611 [643.886, 644.749]; spread 0.863 |
| UNIFIED_SUMCHECK.P[0].wall_seconds | 911.559 [908.309, 925.804]; spread 17.495 | 663.684 [663.582, 663.893]; spread 0.311 |
| UNIFIED_SUMCHECK.V[0].cpu_seconds | 99.873 [95.696, 101.500]; spread 5.804 | 4.833 [4.749, 4.973]; spread 0.224 |
| UNIFIED_SUMCHECK.V[0].wall_seconds | 99.924 [95.737, 101.527]; spread 5.790 | 4.834 [4.750, 4.975]; spread 0.225 |
| V.phase.all-classified-roots-prechallenge-seal | 7.382 [7.331, 7.553]; spread 0.223 | 7.337 [7.335, 7.361]; spread 0.026 |
| V.phase.all-native-GKR | 52.998 [52.233, 53.114]; spread 0.881 | 53.226 [53.099, 53.989]; spread 0.890 |
| V.phase.all-source-original-handles | 16.986 [16.963, 17.057]; spread 0.095 | 16.943 [16.868, 17.332]; spread 0.464 |
| V.phase.classified-physical-zero-tails | 0.000 [0.000, 0.000]; spread 0.000 | 0.000 [0.000, 0.000]; spread 0.000 |
| V.phase.classified-public-Y-constants-and-input-definition | 0.014 [0.014, 0.014]; spread 0.001 | 0.014 [0.014, 0.024]; spread 0.010 |
| V.phase.frozen-witness-and-prechallenge-lookup | 6.240 [5.341, 6.411]; spread 1.070 | 6.157 [5.979, 6.345]; spread 0.367 |
| V.phase.native-Range-Exp-Lasso-Memory-Product | 325.986 [323.349, 327.087]; spread 3.738 | 325.283 [325.061, 327.840]; spread 2.779 |
| V.phase.protocol1-unified-input-sumcheck-and-opening | 284.231 [280.111, 285.652]; spread 5.541 | 189.696 [188.580, 190.696]; spread 2.116 |

Combined C1 full12: P `2902.496846s`, V `685.925569s`, P RSS `29986460KiB`, V RSS `6153416KiB`, proof `238413515` bytes. Single combined run, not a three-repeat performance median.

The physical chunk API provides scalar callbacks, so chunk IO/cache hit/miss remains null and NOT_MEASURED; storage descriptors touched and scalar payload counts are available. Timed read+map worker CPU cannot be split into pure address arithmetic vs scalar access from this API. U contraction Fr counters exclude weight-constructor internals, source callback arithmetic, K/PC/Sigma arithmetic and library internals; fold and coefficient operation counters are separate. No claim is made that32MiB bounds the entire protocol or that capacity snapshots cover every temporary.

## 7. Git and delivery

Release build and118 CTests passed. Original clipping/X binding/callback-lifetime probes passed. 15 complete independent P/V runs accepted (12 strict,2 combined,1 read diagnostic); six standalone source runs matched. Complete raw logs, proof/Statement bytes, configuration/parameter identity, patches, source, review rulings and measurements are retained.
`MLE_FOLD.patch` compares the candidate to622fa033. `GIT_RESULT.json` lists the24 modified/added files. Main functions: categoryTiles/categoryKernelEndpoint, FoldedU admission/capture/advance, quadraticRoundStreaming/scanMetricsJSON, PublicK RAM compaction, sourceMLE/evaluateOracle. Existing PERF6 remains. Snapshot Git metadata stays inside this stage; no push, ancestor reset or deletion.
Stage A/C/E archives were copied to Windows and verified by container/member SHA, gzip CRC, physical CAS restore and reread. Final delivery also contains parameter bytes, source/dependency/configuration identity, all accepted proofs/Statements and failed diagnostic evidence. `DELIVERY_ARCHIVE.json`, `DELIVERY_MANIFEST.json`, Windows `DELIVERY_LOCAL_VERIFICATION.json` and `DELIVERY_SEMANTIC_VERIFICATION.json` are the final integrity receipts; do not infer Windows completion merely from creation of the remote archive.
Reviewer declined production soundness judgment; gates remain blocked. No cleanup is authorized or performed.
