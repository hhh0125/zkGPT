# Actual12-block graph and connection evidence

Public integer X:32rows,768active/1024padded columns, shared unit5/512. Each block b computes LN(2b)->FC(4b)QKV->R5B25/4096 scaled causal Attention->FC(4b+1)->attention residual; LN(2b+1)->FC(4b+2)->shared-unit GELU->FC(4b+3)->MLP residual. Block b+1 reads exactly block b's MLP-residual master coordinates. Final public Y is block11's output. This scope is the twelve transformer blocks before any external final LN_f/lm_head; no tokenization/embedding/text-inference claim.

All half-up rounding, rational-epsilon LN, GELU threshold/unit/coefficients, clamp1 Exp bytes/tail and original parameter quantization remain frozen R5B. No scaling/rounding constants changed. Local integer-lifting bounds apply independently because every new edge retains the declared ranges; this does not prove that every possible public input is admitted. Constructors reject unsupported domains with checked arithmetic.

| Block | Circuit layers | Input master offset | Attention residual | Output master offset |
|---:|---|---:|---:|---:|
| 0 | 1..23 | 0 | 164667516 | 166668664 |
| 1 | 24..46 | 166668664 | 167635444 | 169636592 |
| 2 | 47..69 | 169636592 | 170603372 | 172604520 |
| 3 | 70..92 | 172604520 | 173571300 | 175572448 |
| 4 | 93..115 | 175572448 | 176539228 | 178540376 |
| 5 | 116..138 | 178540376 | 179507156 | 181508304 |
| 6 | 139..161 | 181508304 | 182475084 | 184476232 |
| 7 | 162..184 | 184476232 | 185443012 | 187444160 |
| 8 | 185..207 | 187444160 | 188410940 | 190412088 |
| 9 | 208..230 | 190412088 | 191378868 | 193380016 |
| 10 | 231..253 | 193380016 | 194346796 | 196347944 |
| 11 | 254..276 | 196347944 | 197314724 | 199315872 |

auditGraph validates all11 exact edges,24 three-term residual branches,48FC activation/W coordinate lists,24 LN gamma/centered andbeta/root maps,12 ordered Exp operators and final output. Mutated residual coefficient and wrong-block LN parameter source are rejected. Full12 structural evidence is complete; actual numerical witness execution covered blocks0,1,2 only, with all declared ranges and padding checked. Blocks3..11 numerical sample admission has not yet been executed.
