from enum import Enum


class TwistType(Enum):
    """
    A particular BLS12 group can have G2 being either a multiplicative or a
    divisive twist.
    """
    M = 0
    D = 1