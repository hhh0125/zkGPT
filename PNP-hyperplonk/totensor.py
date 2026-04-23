import torch
import torch.nn.functional as F
import numpy as np
import torch.nn as nn

def to_sign_and_limbs(value: int, limb_bits: int = 64):
    """
    将整数 value 拆成 (is_positive, limbs)，模仿 ark-ff 宏。
    - value: 可以是 int 或 str（十进制/十六进制字符串）
    - limb_bits: 每个 limb 的位宽，默认 64
    """
    if isinstance(value, str):
        # 支持十进制或十六进制字符串
        if value.startswith("0x") or value.startswith("0X"):
            value = int(value, 16)
        else:
            value = int(value)

    is_positive = value >= 0
    value = abs(value)

    mask = (1 << limb_bits) - 1
    limbs = []
    for i in range(6):
        limbs.append(value & mask)
        value >>= limb_bits

    if not limbs:  # 0 的情况
        limbs = [0]
    limbs = torch.tensor(limbs, dtype=torch.BLS12_381_Fq_G1_Base)
    limbs = F.to_mont(limbs)
    return is_positive, limbs


    

value =4
is_positive, limbs = to_sign_and_limbs(value)

print("torch." + str(limbs))