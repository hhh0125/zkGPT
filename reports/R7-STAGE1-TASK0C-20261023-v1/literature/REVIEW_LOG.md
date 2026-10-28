# Task0-C primary-source review log

Date: 2026-10-23. Review type: targeted primary-source protocol/proof review, not a systematic literature review.

| Source | Archived file / SHA256 | Sections read for this task | Applied fact | Not imported |
|---|---|---|---|---|
| Wahby et al., *Doubly-efficient zkSNARKs without trusted setup* (Hyrax extended) | `hyrax-extended.pdf`, `6440b28eb3d692b144772c660c42607c5ef158306a33691a03921925a8d48296` | §5, §6.1, Appendix A.1, A.3–A.6 and whole-proof discussion | coefficient commitments before challenges; exact Figure 5 multiplication equations; PC/opening context | no blanket use of Hyrax whole-system theorem for Task0-C |
| Goldwasser, Kalai, Rothblum, *Delegating Computation: Interactive Proofs for Muggles* | `gkr-original.pdf`, `493bca78c42e05d07469d5adbef39997f14d63bfda0c84abee468df853e163b0` | full 10-page paper, especially document pp.5–9 | layer polynomial; reduction to two parent evaluations; random linear aggregation | no claim that original GKR hides Task0-C commitments or proves its composition |
| Lund, Fortnow, Karloff, Nisan, *Algebraic Methods for Interactive Proof Systems* | `lfkn-original.pdf`, `926388d68334bc8cdcd586c70f0b97b2b7e957fe0f360858f4574e91a36622ab` | full 10-page author PDF; Sumcheck protocol and polynomial-root lemma | per-round recursion and degree/root soundness | no committed/HVZK/FS theorem imported |

Retrieval URLs:

- https://eprint.iacr.asia/2017/1132.pdf
- https://www.microsoft.com/en-us/research/wp-content/uploads/2008/01/GoldwasserKR08a.pdf
- https://lance.fortnow.com/papers/files/ip.pdf

Hyrax ePrint was retained from the byte-identical Task0-B archive because the current official ePrint endpoint had previously returned 403; the report states this provenance and does not claim a fresh official-byte comparison. GKR and LFKN were retrieved from the author/institution primary URLs above. No abstract-only theorem claim is used.
