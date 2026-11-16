# zkGPT Full12: Single Hyrax + Unified Input + Fiat-Shamir

Fixed build/run workspace: `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`.

`src/` is flat and contains only Full12 sources, headers, included tables, the
manifest-pinned parameter exporter and its CMake configuration. The retained
mainline entry points are:

- `r7_full12_witness`: construct the private witness checkpoint.
- `r7_full12_preprocess`: generate the trusted weight anchor bundle.
- `r7_full12_statement`: generate the public statement.
- `r7_fs_full12_prover`: generate the Full12 Fiat-Shamir proof.
- `r7_fs_full12_verifier`: independently verify that proof.

Build from the fixed workspace:

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release \
  -DCNPY_INCLUDE_DIR="$PWD/deps/cnpy/include" \
  -DCNPY_LIBRARY="$PWD/deps/cnpy/lib/libcnpy.a"
cmake --build build/release --target r7_full12_preprocess r7_full12_witness \
  r7_full12_statement \
  r7_fs_full12_prover r7_fs_full12_verifier -j4
```

The executables are directly under `build/release/src/`. Existing binaries under
`build/release/src/paper_r7/` belong to the previous layout and must not be used
as validation of the flat sources.

Run from the fixed workspace so relative data and manifest paths resolve.
Keep `data/`, `reports/`, `deps/` and `3rd/` available. In particular,
`src/export_gpt2_complete_params.py` is checked against the model manifest and
must retain its original bytes.

The prover/verifier retain the existing `prove|verify PROOF_PATH CONTEXT`
interface, independent public-statement/coverage file descriptors, parameter
identity and transcript format. Production definitions remain
`R7_CLASSIFIED_V2`, `R7_FS_ONEBLOCK`, `R7_FS_FULL12`, `R7_UNIFIED_INPUT1`, and
`R7_SINGLE_HYRAX`; the verifier also defines `R7_COMPLETE_VERIFIER`.

Directory cleanup validation covers dependency selection, backup recovery,
source equivalence, compilation and argument validation. It does not create a
new Full12 proof or change the existing protocol acceptance status.

The complete supported mainline is preprocessing -> witness/statement ->
Full12 prover -> independent verifier. Standalone plan and test entry points
are retired. Shared Full12 implementations retain their historical filenames.

All mainline libraries use function/data sections and all five entry points
link with `--gc-sections`. Use the supplied CMake configuration: discarded
legacy functions can reference implementations removed from this mainline.

The exact cleanup list, original source backup, source hashes, bounded runtime
checks and build logs are in `reports/SRC-FULL12-MAINLINE-20261116-v1/`.
The prior flattening evidence remains in `reports/SRC-FULL12-FLAT-20261116-v1/`.
