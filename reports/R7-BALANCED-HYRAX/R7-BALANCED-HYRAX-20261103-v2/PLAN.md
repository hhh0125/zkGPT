# R7 balanced Hyrax implementation plan

Goal: balanced ordinary RowMajor commitments based on padded length, ordinary A.3 through 4096, unchanged trusted W anchors.
Architecture: central ordinary vector shape calculation; explicitly version ordinary wire layout. Preserve logical classified packing identity because existing pinned Full12 bundle includes it. Bind ordinary commitment scheme and dimensions separately in classified and unified FS inputs.
Tech: existing C++17/MCL/Release Linux toolchain, Windows source authority.
Spec: user attachment 348caeb7-e45e-4612-a1b2-9358b2f452a7, plus subsequent approval to retain actual OPT3-D worktree.

Constraints: no GitHub, commits, index changes, deletion, source copies or Linux source editing. Linux only receives SHA-checked Windows bytes. All source/build/run paths remain those requested. Preserve 2^24 cap, mappings, witness, model parameters/anchors/pins and safety statuses.

Review focus: invalid padded/overflow before allocation; old-format downgrade even when dimensions coincide; odd log2 low-column bit split; independent high-dimensional knowledge/core verification; anchor identity independence from ordinary shape.

- [x] Archive Windows source/status/diff and compare Linux baseline. All src matches; historical tool differences excluded. Remote Git is ancestor main/dd66a53, not a dedicated R7 checkout; source bytes identify baseline.
- [ ] Add balanced behavior and A.3 2048/4096 tests, transfer only tests/CMake with hashes; build in new build/balanced-hyrax-20261103-v2. Observe old code rejects required behavior.
- [ ] Modify oneblock_vectors.hpp/.cpp (vectorShape, encode/decode, commit/open), hyrax_a3.hpp/.cpp (dimension bound, MSM bound), hyrax_a3_tests.cpp (old 2048 rejection becomes 8192), classified_layout.cpp/partitioned_layout.cpp (shared shape reports), classified_commitments.cpp/fs_io.cpp (ordinary scheme/expanded basis identity).
- [ ] Archive task-only diff and before/after SHA; stage and guard every deployment against saved current SHA; back up remote old bytes; verify deployed bytes.
- [ ] Release build, CTest and explicit R7 tests (including unregistered tests), anchor load/identity and Full12 layout/coverage census.
- [ ] New Oneblock prover then independent verifier after prover exit; capture time/RSS/proof/statement, raw logs and negative cases.
- [ ] New Full12 prover then independent verifier; confirm 45 roots, 10 W chunks, 1502 claims, 8112 coverage and requested native dimensions; report any mismatch/failure exactly.
- [ ] Independent final code review, archive SHA/restore-read verification and Chinese report.

Execution: inline, directly authorized by user. Skill commit/worktree/approval/delete suggestions yield to the user's explicit prohibitions and instruction to implement directly. No source changes from HEAD or old backup.
