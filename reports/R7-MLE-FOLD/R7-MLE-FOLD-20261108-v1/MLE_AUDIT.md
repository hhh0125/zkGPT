# Arbitrary-offset mathematical audit
For z in [0,2^d), chi_z(r)=product_j (r_j if bit_j(z)=1 else 1-r_j).
Each category contributes sum_{z=b_c}^{b_c+n_c-1} U[z] chi_z(r); local category MLE alone is not this expression.

categoryTiles takes each storage root actual interval, picks the largest aligned dyadic interval of size at most 2^20 fitting the remaining suffix, and advances by its positive length. Thus induction on begin proves disjoint complete coverage, no category padding included. Chunk boundaries only split intervals, never add cells. All endpoints are public and independently rebuilt. Nonempty intervals of length one guarantee termination. The exact preexisting order/descriptors are unchanged by extraction to category_tiles.hpp.

For tile [a,a+2^ell), a divisible by 2^ell, no carry occurs in a+j for 0<=j<2^ell. Hence chi_{a+j}(r)=selector(a,ell,r_high)*chi_j(r_low). A tile prefix fold is exactly contraction of the low bits. Once t>=ell, tile scalars sharing the suffix coordinate a>>t must be added first for BOTH U and K, before forming a quadratic product. u_streaming.cpp's collapsed map does so. This is crucial: summing tile products instead would omit cross terms. Gaps and category/global tails have no tile and hence zero value.

PublicK folds each local coefficient vector with the same low-bit challenges. Its endpoint is sum_tile selector(tile,r)*local_K_fold. categoryKernel's coefficient definition remains K[z]=sum_i rho_i sum_{j:phi_i(j)=z} beta(r_i,j). Positive/negative/zero strides are separately exercised by the retained clipping probe and will be rechecked after K changes.

single_hyrax.cpp Basis uses GLOBAL logical coordinates. mappedQueryTerms splits partial physical rows explicitly; full rows use low bits (j+shift)%columns and high bits high+row+carry, where carry=1 iff j+shift>=columns. This is exactly chi at the logical coordinate corresponding to each compact physical cell. Category tails are omitted from physical storage; matrix tail coefficients are zero. No category is treated as a Boolean subcube in this path.

Independent tests: category_mle_fold_tests.cpp denseMLE uses direct bit products, dense fold uses explicit adjacent pairs, and round polynomial uses evaluations at 0/1/2. 220 seeded cases and 1528 round comparisons passed, including callback streaming, tiny chunk limits, empty categories, unit lengths, power/nonpower lengths, zero/one/negative challenges, adjacent categories, local/global zero tails, physical row edges, and collapsed scalar collisions. Each K local state and final production endpoint are checked. Existing crypto tests exercise actual Single opening and mandatory X authentication, to be rerun in final CTest.

No mathematical discrepancy found in the current supported mapping. This audit does not establish the inherited Hyrax/Sigma knowledge-soundness proof; production gate remains blocked.
