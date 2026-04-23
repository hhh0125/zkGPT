import torch
import torch.nn.functional as F

def TYPE():
    return torch.ALT_BN128_Fq_G1_Mont

def TYPE_BASE():
    return torch.ALT_BN128_Fq_G1_Base


def MODULUS_BITS():
    return 254


def BYTE_SIZE():
    return (254+7) // 8


def LIMBS():
    return (254+63) // 64


def zero():
    return torch.tensor([0, 0, 0, 0], dtype=TYPE())


def one():
    return torch.tensor(
        [
            15099507465913791893,  
            747663757447569901,   
            7338689775589656092,  
            1000894565557965703   
        ],
        dtype=TYPE(),
    )


