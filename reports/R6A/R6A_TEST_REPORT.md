# Test results

Final Release64/64 passes after independent-review fix; all57 retained R5B/legacy regressions included. Combined ASan+UBSan29/29 directed tests pass with leak detection/halt-on-error, including original host/numerical/parser cases, R6oneblock metadata, full-anchor metadata, effective resource arithmetic and actual3-block uncommitted witness. ASan virtual shadow is allowed while real process-group RSS is capped16GiB and deadline600s; no unbounded physical test allocation.

Actual full12 graph/terminal/query audit, all163,749,888 W scalar production mappings, strict external anchor/schema negatives and thirteen CLI/resource cases pass. Wrong-block LN gamma audit was tested RED before adding the missing audit check; the production builder was already correct. New permit/circuit/pin failures are schema/authorization checks, not malformed-proof rejection evidence. Timeout and disk-reserve stop fixtures terminate the complete process group; no model proof is run.

| Run | Seconds | GNUtime peak RSS KiB | Exit |
|---|---:|---:|---:|
| preprocess-full12-v1 | 360.07 | 13926756 | 0 |
| preprocess-oneblock-R6-v1 | 41.94 | 1846656 | 0 |
| final-full12-metadata | 21.10 | 4649800 | 0 |
| limited-threeblock-witness | 44.45 | 4353588 | 0 |
| release-ctest-post-review | 228.41 | 4662700 | 0 |
| sanitize-directed | 94.72 | 4627928 | 0 |
| review-red-inherited-hard-cap | 5.03 | 436340 | 8 |
| review-green-inherited-hard-cap | 3.52 | 816628 | 0 |

Full raw successful and negative outputs are RELEASE64_RAW_LASTTEST.log / SANITIZER29_RAW_LASTTEST.log and named logs; exact argv,cwd,limits,environment,binarySHA,exits and timings are retained in run JSON. Earlier failures are retained: initial missing CMake target configuration/test include/field spelling; secondFC auditor offset fixture; default hardware-thread fingerprint resource error under8GiB resolved using4workers/2arenas; entry usage expectation; one Important inherited-RLIMIT mismatch reproduced and fixed. None is counted as a passing validation. No full12 numeric witness/proof or full independent verifier run occurred.
