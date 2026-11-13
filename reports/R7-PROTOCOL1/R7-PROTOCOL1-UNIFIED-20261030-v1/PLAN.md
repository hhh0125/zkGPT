# Protocol1 unified input implementation plan

Execution: inline with executing-plans and TDD; explicit user authorization covers implementation/testing. No commits or extra approval loop. Spec and architecture: DESIGN.md in this directory. All execution logs in this unique stage, source bytes preserved in Windows baseline archive.

- [x] Freeze current OPT3-B baseline394 bytes; remote migration/AGENTS and SHA equality; confirm no assumed mount and ancestor Git distinction.
- [x] Check design with fresh builder reviewer; resolve mathematical/identity/memory ambiguities before source implementation.
- [x] Add independent dense-reference round polynomial tests to new unified_input_tests.cpp; demonstrate expected RED on remote. Implement unified_input.hpp/.cpp logical aligned layout and streaming quadratic round computation; compare every round and endpoint on4 categories/unequal roots/gaps/repeats and reject malformed segments.
- [x] Add hidden global input reduction with existing hiding arithmetic, separate from old root reductions. Test tape replay, wrong hidden values/round messages/endpoints/FS and memory bounds; verify no secret scalars serialized. Compile new module via paper_r7 CMake.
- [x] Add Commitments::authenticateUnifiedInput(publicX), preserving prepareClaim/digests/tail/Anchor; new common-point per-root Opening and hidden global stitching; production GKR/Lasso source and terminal coverage unchanged. Test wrong roots/maps/claims/bias/padding, mutable callbacks and bounded cache exhaustion. Explicit new protocol suite and FS downgrade rejection.
- [ ] New opt-in Oneblock P/V targets; isolated project-local Release build and complete CTest; independent Oneblock positive then all specified malicious cases. Keep old path until new covers all necessary relations.
- [ ] Route production to accepted path; final source review, source/binary/model/config SHA. Real Full12 with fourCPU/shared-memory monitor; P exit0 then fresh V, coverage/EOF/audit/performance. Failed gates retain logs, no fake completion or old-proof substitution.
- [ ] Final report: paper/code table, math/maps, hidden opening, actual commands/exit codes, tests, measured baseline comparison and remaining security issues. Archive source/diff/dependencies/identity/new proof/Statement/all raw logs to Windows; SHA/member/restore-read checks; no credentials or private Witness.
