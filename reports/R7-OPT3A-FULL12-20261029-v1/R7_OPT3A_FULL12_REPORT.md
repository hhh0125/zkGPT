Real Full12 FS v2 completed with exact OPT3-A kernel; baseline is current No-FullBase source.

{
  "status": "FULL12_OPT3A_INDEPENDENT_VERIFIED",
  "success": true,
  "prover_exit_code": 0,
  "verifier_exit_code": 0,
  "physical_roots": 45,
  "model_chunks": 10,
  "cross_block_connections": 11,
  "verified_count": 8066,
  "required_count": 8066,
  "required_family_counts": {
    "MAPPED_INPUT": 16,
    "PUBLIC_BINDING": 3,
    "MODEL_AUTHORITY": 1,
    "MODEL_RERANDOMIZATION": 1,
    "MODEL_MASTER_LINK": 1,
    "MODEL_PARAMETER_LINK": 144,
    "CROSS_BLOCK_STATE": 11,
    "GKR_OUTPUT": 1,
    "GKR_PREDICATE": 276,
    "GKR_SUMCHECK": 420,
    "GKR_ZERO_REGION": 216,
    "GKR_INTERNAL_AGGREGATION": 276,
    "GKR_MASTER_TERMINAL": 612,
    "FC_MATRIX": 96,
    "GKR_INPUT_AGGREGATION": 1,
    "GKR_AGGREGATION_TERMINAL": 1,
    "RANGE_RECONSTRUCTION": 17,
    "RANGE_TAIL": 17,
    "RANGE_DIGIT": 103,
    "MEMORY_MULTISET": 104,
    "PRODUCT_ROOT": 312,
    "PRODUCT_LAYER": 4314,
    "MEMORY_LEAF": 728,
    "EXP_SUMCHECK": 2,
    "EXP_VALUE_BINDING": 2,
    "CLASSIFIED_COMMITMENT": 45,
    "CLASSIFIED_TRUSTED_ANCHOR_TAIL": 10,
    "CLASSIFIED_ZERO_TAIL": 4,
    "CLASSIFIED_VIEW": 331,
    "CLASSIFIED_AUTHENTICATION": 1
  },
  "statement_sha256": "ce1f1c7253802da5ceb90355c8c34b0547f4febe710fe6ef2691e14271fcb76b",
  "fs": {
    "security_status": "FS_NIZK_CANDIDATE",
    "suite": "R7-FULL12-CLASSIFIED-HYRAX-LOOKUP-FS-ROWMAJOR-CANDIDATE-v2",
    "session": "9974f2189745c24b3063c6f1ee9d4ccf9580a97af65fcc0a6a39f14a2d1f26f3",
    "complete": true,
    "proof_bytes": 263710425,
    "proof_sha256": "324d57c39a5bb3c6e4b4f1e25b70c9ba0979abc1d904c666fd28b440fcf4e287",
    "transcript_digest": "dee48d804303da703e66dafa019d400f5232e0378a01ab67cbeaaf9e46d49253",
    "challenge_digest": "8d942ab4cd01f42c53d9ff788e24414f0ce959e7637a0e0e727760449a19a160",
    "challenges": 167794,
    "prover_messages": 508492,
    "events": 676288,
    "field_rejections": 986556,
    "hash_seconds": 1.12413,
    "file_io_seconds": 0.0999352,
    "file_syscalls": 4025,
    "file_buffer_bytes": 65536,
    "max_message_payload_bytes": 2679174,
    "production_gate": "BLOCKED_CRYPTOGRAPHIC_DESIGN",
    "may_enter_stage2": false
  },
  "proof_sha256": "324d57c39a5bb3c6e4b4f1e25b70c9ba0979abc1d904c666fd28b440fcf4e287",
  "proof_bytes": 263710425,
  "full_base_registration_rows": 0,
  "full_base_registration_response_bytes": 0,
  "tail_rows": 54,
  "tail_response_bytes": 15040,
  "model_binding_retained": true,
  "source_unchanged": true,
  "parameters_unchanged": true,
  "historical_files_unchanged": true,
  "independent_verifier_after_prover_exit": true,
  "eof_verified": true,
  "security_status": "FS_NIZK_CANDIDATE",
  "production_gate": "BLOCKED_CRYPTOGRAPHIC_DESIGN",
  "may_enter_stage2": false,
  "finished_utc": "2026-10-29T15:30:03.824870+00:00"
}

Actual performance comparison:

{
  "prover_wall_seconds": {
    "before": 5935.546122,
    "after": 5749.672307198867,
    "absolute_change": -185.87381480113254,
    "percent_change": -3.1315368625002247,
    "before_over_after": 1.0323277231936174
  },
  "verifier_wall_seconds": {
    "before": 3492.154318,
    "after": 3251.4675149610266,
    "absolute_change": -240.68680303897327,
    "percent_change": -6.8922155529718285,
    "before_over_after": 1.074024052810461
  },
  "prover_peak_rss_kib": {
    "before": 52071416,
    "after": 52079148,
    "absolute_change": 7732,
    "percent_change": 0.014848837604108942,
    "before_over_after": 0.9998515336694832
  },
  "verifier_peak_rss_kib": {
    "before": 6500324,
    "after": 6493924,
    "absolute_change": -6400,
    "percent_change": -0.09845663077717356,
    "before_over_after": 1.0009855366339366
  },
  "proof_bytes": {
    "before": 263710424,
    "after": 263710425,
    "absolute_change": 1,
    "percent_change": 3.7920381941367627e-07,
    "before_over_after": 0.9999999962079619
  },
  "full_registration_response_bytes": {
    "before": 0,
    "after": 0,
    "absolute_change": 0,
    "percent_change": null,
    "before_over_after": null
  }
}

Raw P/V logs, separate local coverage, layout, all phase metrics, cgroup/system resources and message categories accompany this report. No native Oneblock run, kernel-only computation optimization; no protocol edits, no historical tampering. Joint knowledge soundness/FS security remains blocked.
