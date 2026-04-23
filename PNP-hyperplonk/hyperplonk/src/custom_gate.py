from __future__ import annotations
from dataclasses import dataclass
from typing import List, Tuple
    
@dataclass
class CustomizedGates:
    """
    A customized gate is a list of tuples of
        (coefficient, selector_index, wire_indices)

    Example:
        q_L(X) * W_1(X)^5 - W_2(X) = 0

    Represented as:
        [
            ( 1,  id_qL, [id_W1, id_W1, id_W1, id_W1, id_W1] ),
            (-1,  None,  [id_W2] )
        ]
    """
    gates: List[Tuple[int, int, List]]

    # ---------------- Core queries ----------------
    def degree(self) -> int:
        """
        Return the algebraic degree of the customized gate.
        For each term, degree = len(wire_indices) + (1 if it has a selector else 0).
        """
        res = 0
        for _coeff, sel, wires in self.gates:
            deg = len(wires) + (1 if sel is not None else 0)
            if deg > res:
                res = deg
        return res

    def num_selector_columns(self) -> int:
        """
        Return the number of selectors used in the customized gate.
        Constraint (as in the original): the same selector should not be reused
        across multiple monomials.
        """
        cnt = 0
        for _coeff, sel, _ws in self.gates:
            if sel is not None:
                cnt += 1
        return cnt

    def num_witness_columns(self) -> int:
        """
        Return the number of witness columns.
        Mirrors the original Rust logic: assumes wire indices are ordered and
        only compares the last element of each list. Finally add +1 because
        indices start from 0.
        """
        res = 0
        for _coeff, _sel, ws in self.gates:
            if ws:
                p = ws[-1]
                if res < p:
                    res = p
        return res + 1  # indices are 0-based

    # ---------------- Factory helpers ----------------
    @staticmethod
    def vanilla_plonk_gate() -> "CustomizedGates":
        """
        Return the vanilla PLONK gate:
            q_L w1 + q_R w2 + q_O w3 + q_M w1w2 + q_C = 0
        """
        return CustomizedGates(
            gates=[
                (1, 0, [0]),       # q_L * w1
                (1, 1, [1]),       # q_R * w2
                (1, 2, [2]),       # q_O * w3
                (1, 3, [0, 1]),    # q_M * (w1 * w2)
                (1, 4, []),        # q_C
            ]
        )

    @staticmethod
    def jellyfish_turbo_plonk_gate() -> "CustomizedGates":
        """
        Return the Jellyfish Turbo PLONK gate.

        Variables:
          w = [w1, w2, w3, w4, w5]
          q = [q1, q2, q3, q4, qM1, qM2, qH1, qH2, qH3, qH4, qE, qO, qC]
        """
        return CustomizedGates(
            gates=[
                (1, 0,  [0]),
                (1, 1,  [1]),
                (1, 2,  [2]),
                (1, 3,  [3]),
                (1, 4,  [0, 1]),
                (1, 5,  [2, 3]),
                (1, 6,  [0, 0, 0, 0, 0]),
                (1, 7,  [1, 1, 1, 1, 1]),
                (1, 8,  [2, 2, 2, 2, 2]),
                (1, 9,  [3, 3, 3, 3, 3]),
                (1, 10, [0, 1, 2, 3]),
                (1, 11, [4]),
                (1, 12, []),
            ]
        )

    @staticmethod
    def mock_gate(num_witness: int, degree: int) -> "CustomizedGates":
        """
        Generate a mock gate for `num_witness` with highest degree `degree`.

        Structure:
        - Highest-degree term: selector 0 with wire_indices [0, 0, ..., 0, 1]
          (length = degree). This matches the original Rust construction:
              vec![0; degree-1] then push(1).
        - Linear terms: selectors 1..num_witness, each selecting wire i.
        - Constant term: selector num_witness + 1 with empty wires.
        """
        if degree < 1:
            raise ValueError("degree must be >= 1")

        gates = []

        high_degree_term = [0] * (degree - 1)
        high_degree_term.append(1)
        gates.append((1, 0, high_degree_term))

        for i in range(num_witness):
            gates.append((1, i + 1, [i]))

        gates.append((1, num_witness + 1, []))

        return CustomizedGates(gates=gates)

    @staticmethod
    def super_long_selector_gate() -> "CustomizedGates":
        """
        Return a gate with #selectors > 2 * #witnesses:

            q1 w1 + q2 w2 + q3 w3
          + q4 w1w2 + q5 w1w3 + q6 w2w3
          + q7 = 0
        """
        return CustomizedGates(
            gates=[
                (1, 0, [0]),
                (1, 1, [1]),
                (1, 2, [2]),
                (1, 3, [0, 1]),
                (1, 4, [0, 2]),
                (1, 5, [1, 2]),
                (1, 6, []),
            ]
        )
