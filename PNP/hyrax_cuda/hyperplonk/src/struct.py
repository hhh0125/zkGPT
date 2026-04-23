from dataclasses import dataclass
from typing import List
import math
import torch
import torch.nn.functional as F
from ..fields import fr
from .custom_gate import CustomizedGates
from .selector import SelectorColumn
from .util import log2
from ..subroutines.src.pcs.structure import AffinePointG1
from ..subroutines.src.pcs.multilinear_kzg.batching import BatchProof
from ..subroutines.src.poly_iop.struct import IOPProof
from ..subroutines.src.pcs.multilinear_kzg.mod import MultilinearProverParam, MultilinearVerifierParam
# =============================== Proof container ===============================

@dataclass
class HyperPlonkProof:
    """
    HyperPlonk PolyIOP proof consists of:
      - witness commitments
      - a batch opening for all queried MLEs
      - zero-check proof (custom gate satisfiability)
      - permutation-check proof (copy constraints)
    """
    witness_commits: List[AffinePointG1]
    batch_openings: BatchProof
    zero_check_proof: IOPProof       
    perm_check_proof: IOPProof      


# =============================== Instance params ===============================

@dataclass
class HyperPlonkParams:
    """
    Instance parameters:
      - num_constraints: total constraints (must be power of two elsewhere)
      - num_pub_input: number of public-input entries (<= num_constraints)
      - gate_func: customized gate description
    """
    num_constraints: int
    num_pub_input: int
    gate_func: CustomizedGates

    def num_variables(self) -> int:
        """#variables for multilinear system = log2(num_constraints)."""
        # NOTE: caller should ensure num_constraints is power of two.
        return int(math.log2(self.num_constraints))

    def num_selector_columns(self) -> int:
        return self.gate_func.num_selector_columns()

    def num_witness_columns(self) -> int:
        return self.gate_func.num_witness_columns()

    def eval_id_oracle(self, point: List[torch.Tensor]) -> torch.Tensor:
        """
        Evaluate the identity oracle at `point`.
        """  
        length = self.num_variables() + log2(self.num_witness_columns())
        assert len(point) == length, f"ID oracle point length = {len(point)}, expected {length}"
        # accumulate base-2 weighted sum in the field
        res = fr.zero()
        base = fr.one()
        for v in point:
            F.add_mod(res, F.mul_mod(base, v), inplace = True)
            F.add_mod(base, base, inplace = True)
        return res


# ================================ Index object =================================

@dataclass
class HyperPlonkIndex:
    """
    Index holds:
      - params
      - permutation (vector over the field)
      - selector columns
    """
    params: HyperPlonkParams
    permutation: torch.Tensor
    selectors: List[SelectorColumn]

    def num_variables(self) -> int:
        return self.params.num_variables()

    def num_selector_columns(self) -> int:
        return self.params.num_selector_columns()

    def num_witness_columns(self) -> int:
        return self.params.num_witness_columns()


# ============================== Proving / Verifying keys ==============================

@dataclass
class HyperPlonkProvingKey:
    """
    Proving key includes:
      - params
      - preprocessed permutation polynomials (MLEs)
      - preprocessed selector polynomials (MLEs)
      - commitments to selectors & permutations
      - PCS prover params
    """
    params: HyperPlonkParams
    permutation_oracles: List[torch.Tensor]
    selector_oracles: List[torch.Tensor]
    selector_commitments: List[torch.Tensor]
    permutation_commitments: List[torch.Tensor]
    pcs_param: MultilinearProverParam


@dataclass
class HyperPlonkVerifyingKey:
    """
    Verifying key includes:
      - params
      - PCS verifier params
      - commitments to preprocessed polynomials
    """
    params: HyperPlonkParams
    pcs_param: MultilinearVerifierParam
    selector_commitments: List[AffinePointG1]
    perm_commitments: List[AffinePointG1]
