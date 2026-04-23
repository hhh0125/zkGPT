import torch
import torch.nn.functional as F

class Fr:
    pass

def TYPE():
    return torch.ALT_BN128_Fr_G1_Mont

def TYPE_BASE():
    return torch.ALT_BN128_Fr_G1_Base


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
           12398697334308763643, 
           3874526638670607657,  
           7338689775589656110,   
           1000894565557965703
        ],
        dtype=TYPE(),
    )

# def MODULUS():
#     return torch.tensor(
#         [
#             18446744069414584321,
#             6034159408538082302,
#             3691218898639771653,
#             8353516859464449352,
#         ],
#         dtype=TYPE(),
#     )
# def GENERATOR():
#     return torch.tensor(
#         [64424509425, 1721329240476523535, 18418692815241631664, 3824455624000121028],
#         dtype=TYPE(),
#     )



def make_tensor(x, n=1):  # x is a integer in base domain
    assert x.bit_length() < 64
    output = [x] + [0] * (LIMBS() - 1)
    output = torch.tensor(output * n, dtype=TYPE_BASE())
    return F.to_mont(output)
