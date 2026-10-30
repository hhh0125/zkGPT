Real Full12 FS v2 completed with exact OPT3-B kernel; baseline is current successful OPT3-A uncommitted source.

{
  "status": "FULL12_OPT3B_INDEPENDENT_VERIFIED",
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
    "session": "cc180c48155db4e17027e861f28c390d7887f3f92426cd7929fa3dd9aba96332",
    "complete": true,
    "proof_bytes": 263710426,
    "proof_sha256": "8de9903f6fa11511e14bc4b5c8a5c269817b0e562cbf44c53060c4630ee6d9b3",
    "transcript_digest": "da7d61084268f692b8334e5d7fbe517d1cb2a24bcf3eb1135cc2d03ffa0e4315",
    "challenge_digest": "c1fa129edae16e4de370dede7daa08f59f9b685f4b7b2db8478767a6daf4a937",
    "challenges": 167794,
    "prover_messages": 508492,
    "events": 676288,
    "field_rejections": 988637,
    "hash_seconds": 1.12338,
    "file_io_seconds": 0.109421,
    "file_syscalls": 4025,
    "file_buffer_bytes": 65536,
    "max_message_payload_bytes": 2679174,
    "production_gate": "BLOCKED_CRYPTOGRAPHIC_DESIGN",
    "may_enter_stage2": false
  },
  "proof_sha256": "8de9903f6fa11511e14bc4b5c8a5c269817b0e562cbf44c53060c4630ee6d9b3",
  "proof_bytes": 263710426,
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
  "finished_utc": "2026-10-30T08:46:05.551710+00:00"
}

Actual performance comparison:

{
  "prover_wall_seconds": {
    "before": 5749.672307198867,
    "after": 3330.973576589953,
    "absolute_change": -2418.698730608914,
    "percent_change": -42.06672313447475,
    "before_over_after": 1.7261236617448734
  },
  "verifier_wall_seconds": {
    "before": 3251.4675149610266,
    "after": 1018.4756479156204,
    "absolute_change": -2232.9918670454063,
    "percent_change": -68.6764316964788,
    "before_over_after": 3.1924842990751676
  },
  "prover_peak_rss_kib": {
    "before": 52079148,
    "after": 52077412,
    "absolute_change": -1736,
    "percent_change": -0.0033333878657154682,
    "before_over_after": 1.0000333349898416
  },
  "verifier_peak_rss_kib": {
    "before": 6493924,
    "after": 6312208,
    "absolute_change": -181716,
    "percent_change": -2.798246483944068,
    "before_over_after": 1.0287880247292231
  },
  "proof_bytes": {
    "before": 263710425,
    "after": 263710426,
    "absolute_change": 1,
    "percent_change": 3.792038179757209e-07,
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
