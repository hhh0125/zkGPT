# yourpkg/fields/bls12_381.py
import random
import torch
import torch.nn.functional as F 


# =============================== Fr shell ===============================

class Fr:
    """
    Shell namespace for BLS12-381 scalar field (Fr) in Montgomery form.
    All functions operate on raw torch tensors (no element wrapper class).
    """

    # ---- type & constants ----
    @staticmethod
    def TYPE():
        """Montgomery dtype for Fr elements."""
        return torch.BLS12_381_Fr_G1_Mont

    @staticmethod
    def BASE_TYPE():
        """Base (non-Montgomery) dtype for Fr elements."""
        return torch.BLS12_381_Fr_G1_Base

    @staticmethod
    def MODULUS_BITS():  # Fr域的模数位数
        return 255

    @staticmethod
    def BYTE_SIZE():     # 存储Fr域元素所需的字节数
        return (255 + 7) // 8

    @staticmethod
    def LIMBS():
        return (255 + 63) // 64  # 4 limbs of 64-bit

    @staticmethod
    def TWO_ADICITY():
        return 32

    # ---- constants as tensors (Montgomery form, 4 limbs) ----
    @staticmethod
    def zero():
        return torch.tensor([0, 0, 0, 0], dtype=Fr.TYPE())

    @staticmethod
    def one():
        return torch.tensor(
            [
                8589934590,
                6378425256633387010,
                11064306276430008309,
                1739710354780652911,
            ],
            dtype=Fr.TYPE(),
        )

    @staticmethod
    def MODULUS():
        return torch.tensor(
            [
                18446744069414584321,
                6034159408538082302,
                3691218898639771653,
                8353516859464449352,
            ],
            dtype=Fr.TYPE(),
        )

    @staticmethod
    def TWO_ADIC_ROOT_OF_UNITY():
        return torch.tensor(
            [
                13381757501831005802,
                6564924994866501612,
                789602057691799140,
                6625830629041353339,
            ],
            dtype=Fr.TYPE(),
        )

    @staticmethod
    def GENERATOR():
        return torch.tensor(
            [64424509425, 1721329240476523535, 18418692815241631664, 3824455624000121028],
            dtype=Fr.TYPE(),
        )

    @staticmethod
    def REPR_SHAVE_BITS():
        return 1

    # ---- predicates ----
    @staticmethod
    def is_zero(t: torch.Tensor) -> bool:
        """Return True iff the tensor encodes zero (all limbs are zero)."""
        return torch.all(t == 0).item()

    # ---- constructors from small ints (base -> Montgomery) ----
    @staticmethod
    def from_int(x: int, n: int = 1) -> torch.Tensor:
        """
        Convert a small base-domain integer x into Montgomery form tensor.
        x must fit in one 64-bit limb. If n>1, returns flattened n copies.
        """
        assert x.bit_length() < 64, "x must fit in 64 bits"
        limbs = [x] + [0] * (Fr.LIMBS() - 1)  # [x,0,0,0]
        base = torch.tensor(limbs * n, dtype=Fr.BASE_TYPE())
        return F.to_mont(base)  # returns dtype=Fr.TYPE()
    
    @staticmethod
    def rand(rng) -> torch.Tensor:
        x = rng.randint(0, 1<<32-1)
        x_Fr = Fr.from_int(x)
        return x_Fr

# =============================== Fq shell ===============================

class Fp:
    """
    Shell namespace for BLS12-381 base field (Fq) in Montgomery form.
    All functions operate on raw torch tensors (no element wrapper class).
    """
    COEFF_A = 0
    # ---- type & constants ----
    @staticmethod
    def TYPE():
        """Montgomery dtype for Fq elements."""
        return torch.BLS12_381_Fq_G1_Mont

    @staticmethod
    def BASE_TYPE():
        """Montgomery dtype for Fq elements."""
        return torch.BLS12_381_Fq_G1_Base
    
    @staticmethod
    def MODULUS_BITS():
        return 381

    @staticmethod
    def BYTE_SIZE():
        return (381 + 7) // 8
    
    @staticmethod
    def LIMBS():
        return (381 + 63) // 64  # 6 limbs of 64-bit

    @staticmethod
    def extension_degree():
        return 1
    
    # ---- constants as tensors (Montgomery form, 6 limbs) ----
    @staticmethod
    def zero():
        return torch.tensor([0, 0, 0, 0, 0, 0], dtype=Fp.TYPE())

    @staticmethod
    def one():
        return torch.tensor(
            [
                8505329371266088957,
                17002214543764226050,
                6865905132761471162,
                8632934651105793861,
                6631298214892334189,
                1582556514881692819,
            ],
            dtype=Fp.TYPE(),
        )

    @staticmethod
    def TWO_INV():
        return [
                13402431016077863595,
                2210141511517208575,
                7435674573564081700,
                7239337960414712511,
                5412103778470702295,
                1873798617647539866,
            ]
    
    @staticmethod
    def MODULUS():
        return torch.tensor(
            [
                13402431016077863595,
                2210141511517208575,
                7435674573564081700,
                7239337960414712511,
                5412103778470702295,
                1873798617647539866,
            ], 
            dtype=Fp.TYPE(),
        )
    
    @staticmethod
    def MODULUS_PLUS_ONE_DIV_FOUR():
        return torch.tensor(
            [
                17185665809301629611,
                552535377879302143,
                15693976698673184137,
                15644892545385841839,
                10576397981472451381,
                468449654411884966,
            ], 
            dtype=Fp.TYPE(),
        )
    
    @staticmethod
    def MODULUS_MINUS_ONE_DIV_TWO():
        return torch.tensor(
            [
                15924587544893707605,
                1105070755758604287,
                12941209323636816658,
                12843041017062132063,
                2706051889235351147,
                936899308823769933,
            ], 
            dtype=Fp.TYPE(),
        )
    # ---- predicates ----
    @staticmethod
    def is_zero(t: torch.Tensor) -> bool:
        return torch.all(t == 0).item()

    # ---- constructors from small ints ----
    @staticmethod
    def from_int(x: int) -> torch.Tensor:
        """
        Construct an Fq tensor from a small Python int.
        NOTE: if you have a base->Montgomery converter for Fq, replace this with it.
        """
        assert x.bit_length() < 64, "x must fit in 64 bits"
        limbs = [x] + [0] * 5  # [x,0,0,0,0,0]
        base = torch.tensor(limbs, dtype=Fp.BASE_TYPE())
        return F.to_mont(base)

    # The Frobenius map has no effect in a prime field.
    @staticmethod
    def frobenius_map_in_place(self, _: int):
        return self
    
    @staticmethod
    def sample(rng: random.Random):
        x = rng.randint(0, 1<<32-1)
        x_Fq = Fp.from_int(x)
        return x_Fq
    

Fr = Fr
Fp = Fp