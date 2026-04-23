from dataclasses import dataclass
from typing import List
import math

import torch
import torch.nn.functional as F
from .util import build_mles

def _is_power_of_two(n: int) -> bool:
    return n > 0 and (n & (n - 1)) == 0


# -------------------------- Core data structures --------------------------

@dataclass
class SelectorRow:
    """A row of selectors with width = #selectors."""
    values: List[torch.Tensor]

    @staticmethod
    def build_mles(matrix: List["SelectorRow"]) -> List[torch.Tensor]:
        mle = build_mles(matrix)
        return mle


@dataclass
class SelectorColumn:
    """A column of selectors with length = #constraints."""
    values: List

    def get_nv(self) -> int:
        """
        Number of variables of the multilinear polynomial represented by this column.
        Equals log2(#constraints). Requires #constraints to be a power of two.
        """
        n = len(self.values)
        assert _is_power_of_two(n), f"selector column length ({n}) is not a power of two"

        return int(math.log2(n))

    def append(self, new_element: torch.Tensor):
        """Append a new selector value to the column."""
        self.values.append(new_element)

    @staticmethod
    def from_selector_rows(selector_rows: List[SelectorRow]) -> List[torch.Tensor]:
        """
        Build selector columns from selector rows.
        """
        num_columns = len(selector_rows[0])

        res = []
        for j in range(num_columns):
            col = [row[j] for row in selector_rows]
            res.append(SelectorColumn(col))
        return res
