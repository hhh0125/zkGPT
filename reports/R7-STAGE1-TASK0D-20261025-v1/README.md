# R7 Stage1 Task0-D — research only

Classification: DESIGNED / LOCALLY_PROVEN / CONDITIONAL / TOY_TESTED. Joint knowledge PARTIAL; production BLOCKED_CRYPTOGRAPHIC_DESIGN; may_enter_stage2=false.

- [Exact protocol](delivery/R7_D_ZK_MEMORY_PROTOCOL.md)
- [Product tree security](delivery/R7_D_PRODUCT_TREE_SECURITY.md)
- [Multi-opening composition and Range/Exp mapping](delivery/R7_D_MULTI_OPENING_COMPOSITION.md)
- [Implementation, all attack outcomes and commands](delivery/R7_D_TOY_IMPLEMENTATION_REPORT.md)
- [Resource measurements](delivery/R7_D_RESOURCE_MEASUREMENTS.json)
- [Research and production gate](delivery/R7_TASK0D_GATE_REVIEW.md)

Normal memory mode uses independently sampled Verifier challenges. The separately launched memory-zero diagnostic deliberately conditions tau on a zero factor; it can ACCEPT an invalid lookup and is excluded from soundness. Its logs are a boundary demonstration, not protocol acceptance evidence.

53 final selected test outcomes (including diagnostic controls and local K-bound test), plus two final resource-guard regressions and six fresh-process resource measurements. Original failures are retained. No oneblock/full12, real parameters/proofs/anchors, large MSM or production changes.

All executable work occurred under the fixed remote workspace. The archive contains research source, byte-identical B/C primitives, public toy statement/proof evidence, logs, source patches, dependency identity, prior security reports, SHA manifest and recovery script. Public transcripts are audit evidence; they cannot act as trusted Verifier receipts.

Restoration on Windows is read-only evidence verification, not proof verification. verify_restore.py checks safe regular archive members, complete membership, SHA, gzip CRC, and restored bytes. Archive receipt/Windows restore receipt are outside the sealed manifest to avoid self-reference. No cleanup or deletion authorized.
