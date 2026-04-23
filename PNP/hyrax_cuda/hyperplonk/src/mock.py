import random
import math
from dataclasses import dataclass
from typing import List
import torch
import torch.nn.functional as F
from ..fields import fr
from .custom_gate import CustomizedGates
from .selector import SelectorColumn
from .witness import WitnessColumn
from .struct import HyperPlonkParams, HyperPlonkIndex
from ..arithmetic.src.multilinear_polynomial import identity_permutation
from .util import log2



@dataclass
class MockCircuit:
    public_inputs: List[torch.Tensor]
    witnesses: List[WitnessColumn]
    index: HyperPlonkIndex

    # === helpers mirroring Rust impl ===
    def num_variables(self) -> int:
        return self.index.num_variables()

    def num_selector_columns(self) -> int:
        return self.index.num_selector_columns()

    def num_witness_columns(self) -> int:
        return self.index.num_witness_columns()

    # === factory: equivalent to Rust `new` ===
    @staticmethod
    def new(
        num_constraints: int,
        gate: CustomizedGates,
        rng: random.Random,
    ) -> "MockCircuit":
        """
        Create a mock circuit with random selectors/witnesses consistent with the gate:
        """
        nv = int(math.log2(num_constraints))
        num_selectors = gate.num_selector_columns()
        num_witnesses = gate.num_witness_columns()
        log_n_wires = log2(num_witnesses)
        merged_nv = nv + log_n_wires  # used for identity permutation size

        selectors: List[SelectorColumn] = [SelectorColumn([]) for _ in range(num_selectors)]
        witnesses: List[WitnessColumn] = [WitnessColumn([]) for _ in range(num_witnesses)]

        for _ in range(num_constraints):
            # first (num_selectors - 1) selector values are random; the last one is solved
            # cur_selectors: List[torch.Tensor] = [fr.sample(rng) for _ in range(num_selectors - 1)]
            # cur_witness:   List[torch.Tensor] = [fr.sample(rng) for _ in range(num_witnesses)]
            cur_selectors: List[torch.Tensor] = [fr.one() for _ in range(num_selectors - 1)]
            cur_witness:   List[torch.Tensor] = [fr.GENERATOR() for _ in range(num_witnesses)]
            last_selector = fr.zero()

            for index, (coeff, q, wit_idxs) in enumerate(gate.gates):
                # convert signed integer coeff to field element
                if coeff < 0:
                    coeff_F = F.neg_mod(fr.from_int(-coeff))
                else:
                    coeff_F = fr.from_int(coeff)

                if index != num_selectors - 1:
                    cur_monomial = coeff_F
                    # multiply by selector, if present
                    if q is not None:
                        F.mul_mod(cur_monomial, cur_selectors[q], inplace = True)
                    # multiply by the product of selected witnesses
                    for w_idx in wit_idxs:
                        F.mul_mod(cur_monomial, cur_witness[w_idx], inplace = True)
                    F.add_mod(last_selector, cur_monomial, inplace = True)
                else:
                    # the last selector is solved so that the total gate sum is zero
                    cur_monomial = coeff_F
                    for w_idx in wit_idxs:
                        F.mul_mod(cur_monomial, cur_witness[w_idx], inplace = True)
                    # last_selector += ?  ==> bring to RHS: last_selector = - (sum other monomials) / cur_monomial
                    # Rust does: last_selector /= -cur_monomial after accumulating others.
                    F.div_mod(last_selector, F.neg_mod(cur_monomial), inplace = True)

            # push solved selector column
            cur_selectors.append(last_selector)

            # append into column containers
            for i in range(num_selectors):
                selectors[i].append(cur_selectors[i])
            for i in range(num_witnesses):
                witnesses[i].append(cur_witness[i])

        # public inputs: first min(4, num_constraints) entries from witness[0]
        pub_input_len = min(4, num_constraints)
        public_inputs = list(witnesses[0].values[:pub_input_len])

        params = HyperPlonkParams(
            num_constraints=num_constraints,
            num_pub_input=len(public_inputs),
            gate_func=gate,
        )

        permutation: torch.Tensor = identity_permutation(merged_nv, 1)
        index = HyperPlonkIndex(
            params=params,
            permutation=permutation,
            selectors=selectors,
        )

        return MockCircuit(
            public_inputs=public_inputs,
            witnesses=witnesses,
            index=index,
        )

    def is_satisfied(self) -> bool:
        """
        Check if the current witnesses/selectors satisfy the gate constraints row-wise.
        """

        for current_row in range(self.num_variables()):
            cur = fr.zero()
            for coeff, q, wit_idxs in self.index.params.gate_func.gates:
                if coeff < 0:
                    coeff_F = F.neg_mod(fr.from_int(-coeff))
                else:
                    coeff_F = fr.from_int(coeff)

                cur_monomial = coeff_F
                if q is not None:
                    F.mul_mod(cur_monomial, self.index.selectors[q].values[current_row], inplace = True)
                for w_idx in wit_idxs:
                    F.mul_mod(cur_monomial, self.witnesses[w_idx].values[current_row], inplace = True)
                F.add_mod(cur, cur_monomial, inplace = True)

            if not F.trace_equal(cur, fr.zero()):
                return False
        return True
