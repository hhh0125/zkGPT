from pathlib import Path
print(Path('src/gkr_proof_builder.cpp').read_text()[:900]);print('\n'.join(x for x in Path('src/gkr_proof_builder.hpp').read_text().splitlines() if 'beta_' in x));print('\n'.join(x for x in Path('src/prover.cpp').read_text().splitlines()[1110:1160]))
