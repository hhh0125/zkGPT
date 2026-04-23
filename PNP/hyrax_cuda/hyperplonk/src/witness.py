from dataclasses import dataclass
from typing import List
import math

import torch
import torch.nn.functional as F
from .util import build_mles

def _is_power_of_two(n: int) -> bool:
    return n > 0 and (n & (n - 1)) == 0
# -----------------------------------------------------------------------------

@dataclass
class WitnessRow:
    """A row of witnesses with width = #wires."""
    values: List[torch.Tensor]

    @staticmethod
    def build_mles(matrix: List["WitnessRow"]) -> List[torch.Tensor]:
        """
        Build MLEs from a matrix of witness rows.

        Given matrix = [row1, row2, ...] where
          row1 = (a1, a2, ...)
          row2 = (b1, b2, ...)
          row3 = (c1, c2, ...)
        Output:
          [ MLE(a1,b1,c1,...), MLE(a2,b2,c2,...), ... ]
        """
        mles = build_mles(matrix)
        return mles


@dataclass
class WitnessColumn:
    """A column of witnesses with length = #constraints."""
    values: List[torch.Tensor]

    def get_nv(self) -> int:
        """
        Number of variables for the multilinear polynomial represented by this column.
        Equals log2(len(values)); requires length to be a power of two.
        """
        n = len(self.values)
        assert _is_power_of_two(n), f"selector/witness column length ({n}) is not a power of two"
        return int(math.log2(n))

    def append(self, new_element: torch.Tensor) -> None:
        """Append a new element to the witness column."""
        self.values.append(new_element)

    @staticmethod
    def from_witness_rows(
        witness_rows: List[WitnessRow],
    ) -> List["WitnessColumn"]:
        """
        Build witness columns from witness rows.
        """
        num_columns = len(witness_rows[0].values)

        res: List[WitnessColumn] = []
        for j in range(num_columns):
            col = [row.values[j] for row in witness_rows]
            res.append(WitnessColumn(col))
        return res

    def coeff_ref(self) -> List[torch.Tensor]:
        """Return a read-only-like view of the underlying coefficients."""
        return self.values  # In Python lists are mutable; return a copy if you need immutability.

    # Parity with Rust's `From<&WitnessColumn<F>> for DenseMultilinearExtension<F>`
    def to_mle(self) -> List[torch.Tensor]:
        """Convert this column to a DenseMultilinearExtension."""
        nv = self.get_nv()
        return self.values
