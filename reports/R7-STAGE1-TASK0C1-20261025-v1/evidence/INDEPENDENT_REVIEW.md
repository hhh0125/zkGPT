# Independent read-only reviews
Date: 2026-10-25.
c1_call_audit read all five prior reports and all inherited source; confirmed mutable P-created receipts, H-PC preselected simulator path, exact challenge schedule and width8 table violation. Findings incorporated in design and audit.
c1_design_review reviewed exact design then server/components/client source. Confirmed eight legacy SHA identities, online child reconstruction, no P state/seed/receipt path, correct H-PC R reconstruction, and final complete comparison.
Important finding: total deadline checked only while waiting for newline; buffered messages/math/output could exceed wall time. Exact failing test retained RED_02_BUFFERED_DEADLINE.log. Fixed with process wall alarm, nonblocking bounded writes, deadline at receive/coin/ACCEPT. GREEN_03_ALL_DEADLINES.log proves buffered/late/nonreading cases, plus full suite actual wall-alarm case.
Launch integration issue missing -I corrected before first passing integration; client stdout ResourceWarning also corrected.
Review did not execute pending adversarial suite; final suite was subsequently executed by primary agent:39/39, deadline3/3. Review claims are not a cryptographic theorem. No further review iterations were used as substitute for tests.
