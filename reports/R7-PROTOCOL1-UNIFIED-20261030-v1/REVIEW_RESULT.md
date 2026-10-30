# Candidate implementation review

2026-10-30. Fresh read-only whole-change review by protocol1_implementation_review; no source edits or remote execution by reviewer. Compared frozen394-file OPT3-B baseline to current397-file candidate manifest CURRENT_SOURCE_SHA_oneblock.json: exactly12 changed/new R7 files; all397 local SHA match the manifest.

Verdict: no critical or important implementation findings. Checked true single global quadratic recurrence; mixed-size block folding and crossroot/crosspartition terms; public X with zero Boolean K; full map validation before aggregation challenges; independently evaluated K(r); hidden common-point Hyrax opening of every physical root and correct selector/public-X stitching; explicit suite/type/instance downgrade rejection; no full private/folded witness copy; authentication/coverage only completed after successful checks.

This review does not replace real Oneblock/Full12 execution. Production switch and Full12 remain gated. Knowledge soundness, zero knowledge and Fiat-Shamir composition are not formally established. FS_NIZK_CANDIDATE / BLOCKED_CRYPTOGRAPHIC_DESIGN / may_enter_stage2=false.

Earlier builder check was resolved in DESIGN.md: canonical input packing binding before rho; separate offset selectors for both U and K; public X participates in folds; new streaming driver rather than raising old materialization bound; all committed root dimensions/openings; mapping validation before challenge draw. Repeated public kernel construction may cost time and must be measured.
