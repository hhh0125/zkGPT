# R7 single Hyrax: local implementation, remote validation pending

Status: **LOCAL_SOURCE_ONLY / NOT_COMPILED / NOT_PROTOCOL_ACCEPTED**.
The server was unreachable on 2026-11-03. The user explicitly authorized Windows
source changes first and upload later. No new proof, verifier result or timing is
claimed. Historical 45-root results are baseline evidence only.

## Implementation

The new FS production path commits the existing Protocol1 virtual domain as one
RowMajor matrix. `single_hyrax.{hpp,cpp}` implements the descriptor, row Pedersen
commitments, bounded parallel CPU MSM, public-X/padding Sigma constraints, chunked
PC exchange, and one folded hidden-evaluation A.3 opening. Existing A.3 separate
knowledge proofs, masked Sigma and compact IPA are retained without a plaintext
evaluation or folded-vector fallback.

`single_commitments.cpp` packs native base and prepared lookup bytes into sealed
source storage, including W in its original logical order. Existing source
partitions remain memory/oracle segments only: their PCs, generator vectors,
blinds and opening functions are unused and empty in the new mode. There is one
global `SingleHyrax/sealed` commitment identity. Native GKR/Lasso maps, biases,
constants, Y claims, aggregation challenges and kernel checks are unchanged.

`authenticateUnifiedInput()` reuses the existing hidden degree-two Sumcheck over
the same virtual domain and authenticates its input endpoint directly against
`C_U`. The old loop of per-root openings and hidden stitching is bypassed only
in the explicitly selected new mode.

The address space is the existing aligned X/W/aux/L packing, with explicit zero
gaps and source tails. This preserves all stored helper padding values that
belong to the original logical witness. The new fixed-cell Sigma proves the
global X segment equals public X and every cell outside the private actual
source intervals is zero. Previously anchor-enforced W tails are included.
No witness value is changed to fit the new matrix.

Low MLE bits index columns, high bits index rows:
`U[i * columns + j]`; `q = (q_columns, q_rows)`. Column count is the smaller of
the near-square choice and the existing A.3 limit of 4096. Domain metadata is
bounded by 2^32; public rows are bounded by 2^20. The production frontend retains
the frozen X maximum of 32768 scalars. The reusable backend binds larger public
X in bounded chunks. No full U or full folded private input is allocated.
Source storage, public row points and row blinds remain in CPU memory; worker
scalar scratch is bounded by `workers * columns * sizeof(Fr)`. This path uses
CPU MCL and requires no GPU allocation.

Generators retain the exact existing BN254 A.3 column derivation and Pedersen H.
The descriptor binds that basis choice, the new suite, global layout, native
source-map identity and model scope; the FS public instance also pins the
existing generator identity. No trusted-R6-role basis is substituted into IPA.

## Protocol selection and model scope

`r7_fs_oneblock_{prover,verifier}` and `r7_fs_full12_{prover,verifier}` now compile
with `R7_SINGLE_HYRAX=1` and `R7_UNIFIED_INPUT1=1`. Their proof types are
`OneblockSingleHyrax` and `Full12SingleHyrax`; old protocols cannot be imported
under those headers. Explicit `r7_fs_legacy45_*` reference targets retain the
historical FS versions. Older interactive/probe targets keep their old suites.

The model scope is explicitly `PROVER_SELECTED_WEIGHTS_BOUND_IN_C_U-v1`. The
new path skips trusted model anchor loading, rerandomization and separate model
commitments. It retains native manifest/profile metadata, W coordinate mapping,
parameter-region relations, GKR, Range/Exp, Lasso, X/Y binding and independent
verification. This is not official checkpoint certification. Frozen honest
witness generation still uses the existing model data and numerical profile.

## Tests written, not executed

- `r7_single_hyrax`: literal/dense-reference MLE at Boolean and non-Boolean points,
  repeated openings, row/handle/query/witness/identity corruption, physical tail
  and gap rejection, large PC codec and public-X payload boundaries.
- `r7_single_hyrax_integration`: real unified-input verification with dense and
  compressed native maps, non-native W span order, split helper pieces with
  nonzero logical padding, wrong GKR/Lasso maps, wrong Y/X, reordered/truncated
  frames and legacy-root messages. V has no witness.
- `r7_single_hyrax_fs`: actual file-backed independent FS verification; old
  45-root and unified proof types, wrong statement/context, transcript order,
  truncation and trailing bytes are rejected by the production verifier flow.

Negative fixtures invoke actual Sigma/IPA/FS verifiers. They have not yet run;
their expected outcomes are requirements, not reported results.

## Remote validation after connection is restored

Do not run from the original `zkGPT` repository. Use only
`/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`. First read its current AGENTS.md
and verify `reports/migration-20261020/MIGRATION_RESULT.json` has success=true.
Check remote Git status and every task-start source SHA before synchronizing.
The prepared deployment manifest and exact before/after bytes are in the Windows
task archive. A changed remote baseline is a conflict; do not overwrite it.

Use a new build directory under the fixed workspace, e.g.:

```sh
cmake -S . -B build/single-hyrax-20261103-v1 \
  -DCMAKE_BUILD_TYPE=Release -DZKGPT_BUILD_R7_FRAMEWORK=ON \
  -DCNPY_INCLUDE_DIR="$PWD/deps/cnpy/include" \
  -DCNPY_LIBRARY="$PWD/deps/cnpy/lib/libcnpy.a"
cmake --build build/single-hyrax-20261103-v1 -j4
ctest --test-dir build/single-hyrax-20261103-v1 \
  -R 'r7_single_hyrax' --output-on-failure
ctest --test-dir build/single-hyrax-20261103-v1 --output-on-failure -j4
```

These commands are prepared, not executed. Capture all output and exit codes
in a new `reports/R7-SINGLE-HYRAX-20261103-v1` stage without overwriting history.
Then run Oneblock P, reap P, run independent V, inspect footer/EOF, transcript and
challenge digests, all verifier-local coverage, root_count=1 and the actual single
layout. Only after that run Full12 and required malicious cases. Reuse the exact
validated public statement/parameter bytes, not an invented fixture. A source
SHA mismatch, failed unit, missing native obligation, or verifier rejection
stops advancement. Do not bypass checks to obtain acceptance.

Measure row/column/padding counts, commit/open/V time, proof bytes, CPU peak RSS
and sampled per-process GPU memory. Compare against archived 45-root measurements
only when scope/profile/environment are compatible. All new values are currently
**NOT_MEASURED**.

## Security obligations

This is still FS_NIZK_CANDIDATE / BLOCKED_CRYPTOGRAPHIC_DESIGN;
may_enter_stage2=false. Engineering tests do not prove joint knowledge soundness,
extractability of all committed rows, multi-opening zero knowledge, or the
Fiat–Shamir composition. Full private-row registration remains absent as in the
baseline; A.3 guards prove their existing separate folded-input relations only.
The new fixed-cell Sigma retains the old tail constraints' role and extends it
to public X, W tails and the global gaps; it is not a full witness extractor.
See the [Hyrax paper](https://www.pepper-project.org/hyrax-oakland18.pdf) and the
existing project security reports for the protocol reference and open obligations.

The task archive contains the task-only patch plus exact source bytes. No Git
commit has been made: the checkout already contains substantial older uncommitted
work, and Linux validation is deferred. Do not stage those older changes as if
they were produced by this task.
