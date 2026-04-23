from .transcript import flags
import torch
import torch.nn.functional as F
from .fields import fr, fp
from .subroutines.src.pcs.structure import AffinePointG1
from .subroutines.src.poly_iop.struct import IOPProverMessage
import struct

def serialize(item, flag=flags.EmptyFlags):
    if isinstance(item, int):
        return item.to_bytes(8, byteorder="little")
    
    
    if isinstance(item, list):
        # list of integer
        if isinstance(item[0], int):
            buffer = None
            for integer in item:
                buf = integer.to_bytes(8, byteorder="little")
                buffer = buf if buffer==None else buffer+buf
            return buffer
        # list of fr
        if isinstance(item[0], torch.Tensor) and item[0].numel() == fr.LIMBS():
            length = len(item)
            buffer = serialize(length)
            for tensor in item:
                buf = serialize(tensor)
                buffer += buf
            return buffer
    # fq
    if isinstance(item, torch.Tensor) and item.numel() == fp.LIMBS():
        assert flag.BIT_SIZE <= 8, "not enough space"
        item_base = F.to_base(item)
        byte_array = bytearray()
        for partial in reversed(item_base.tolist()):
            byte_array.extend(partial.to_bytes(8, byteorder="big"))
        byte_array[0] |= 1<<7
        return byte_array

    # fr
    if isinstance(item, torch.Tensor) and item.numel() == fr.LIMBS():
        assert flag.BIT_SIZE <= 8, "not enough space"
        item_base = F.to_base(item)
        output_byte_size = (fr.MODULUS_BITS() + flag.BIT_SIZE + 7) //8
        byte_array = bytearray()
        for partial in item_base.tolist():
            byte_array.extend(partial.to_bytes(8, byteorder="little"))
        byte_array[output_byte_size - 1] |= flag.u8_bitmask()
        return byte_array

    # proof
    if isinstance(item, IOPProverMessage):
        length = len(item.evaluations)
        buf = serialize(length)
        for eval in item.evaluations:
            buf += serialize(eval)
        return buf
    
    # commitment
    if isinstance(item, AffinePointG1):
        if item.is_zero():
            flag = flags.SWFlags.infinity()
            buf = serialize(fp.zero(), flag)
            buf[0] |= 1<<6
            return buf
        else:
            a = F.to_base(item.y)
            b = F.to_base(F.neg_mod(item.y))
            a_most_sig = a[-1].tolist()
            b_most_sig = b[-1].tolist()
            flag = flags.SWFlags.from_y_sign(a_most_sig > b_most_sig)
            buf = serialize(item.x)
            if not item.is_zero() and flag.flag == 1:
                buf[0] |= 1<<5
            return buf

    assert False, "unsupported type"

def deserialize(reader: bytearray, Flag = flags.EmptyFlags):
    # deserialize_compressed
    assert flags.EmptyFlags.BIT_SIZE <= 8, "empty flags too large"

    masked_bytes = bytearray(reader[:fr.BYTE_SIZE()])
    
    format_string = "<" + "Q" * fr.LIMBS()
    bigint = struct.unpack_from(format_string, masked_bytes)
    base_x = torch.tensor(bigint, dtype=fr.BASE_TYPE())
    return F.to_mont(base_x)

def from_random_bytes_with_flags(data: bytes, Flag = flags.EmptyFlags):
    shave_bits = fr.REPR_SHAVE_BITS()

    # 临时缓冲：对齐到 N*8 + 1（最后一个 limb 的 8 字节 + 额外 1 字节）
    need = fr.LIMBS() * 8 + 1
    buf_len = max(len(data), need)
    result_bytes = bytearray(buf_len)   # zeroed
    # Copy the input into a temporary buffer.
    result_bytes[:len(data)] = data

    last_limb_mask = ((0xFFFFFFFFFFFFFFFF >> shave_bits) & 0xFFFFFFFFFFFFFFFF).to_bytes(8, "little")

    last_bytes_mask = bytearray(9)
    last_bytes_mask[:8] = last_limb_mask
    last_bytes_mask[8] = 0

    # Length of the buffer containing the field element and the flag.
    output_byte_size = (fr.MODULUS_BITS() + Flag.BIT_SIZE + 7) // 8
    flag_location = output_byte_size - 1

    # At which byte is the flag located in the last limb?
    flag_location_in_last_limb = flag_location - 8 * (fr.LIMBS() - 1)
    if flag_location_in_last_limb < 0:
        flag_location_in_last_limb = 0

    # Take all but the last 9 bytes.
    start = fr.LIMBS() * 8 - 8
    end = start + 9
    
    if end > len(result_bytes):
        result_bytes += b"\x00" * (end - len(result_bytes))
    last9 = memoryview(result_bytes)[start:end]

    # The mask only has the last `F::BIT_SIZE` bits set
    flags_mask = (0xFF << (8 - Flag.BIT_SIZE)) & 0xFF

    # Mask away the remaining bytes, and try to reconstruct the flag
    flags_u8 = 0

    for i, m in enumerate(last_bytes_mask):
        if i >= len(last9):
            break
        if i == flag_location_in_last_limb:
            flags_u8 = last9[i] & flags_mask
        last9[i] = last9[i] & m

    fe = deserialize(bytes(result_bytes[:fr.LIMBS() * 8]))
    if fe is None:
        return None

    flag_obj = flags_u8
    if flag_obj is None:
        return None

    return fe, flag_obj

def from_be_bytes_mod_order(bytes: bytearray):
    """
    Reads bytes in big-endian, and converts them to a field element.
    If the integer represented by `bytes` is larger than the modulus `p`, this method
    performs the appropriate reduction.
    """
    num_bytes_to_directly_convert = min(fr.BYTE_SIZE() - 1, len(bytes))
    bytes_to_directly_convert = bytes[:num_bytes_to_directly_convert][::-1]
    # Copy the leading big-endian bytes directly into a field element.
    # The number of bytes directly converted must be less than the
    # number of bytes needed to represent the modulus, as we must begin
    # modular reduction once the data is of the same number of bytes as the modulus.
    res, flag_ = from_random_bytes_with_flags(bytes_to_directly_convert)
    window_size = fr.from_int(256)
    # Update the result, byte by byte.
    # We go through existing field arithmetic, which handles the reduction.
    # TODO: If we need higher speeds, parse more bytes at once, or implement
    # modular multiplication by a u64
    for b in bytes[num_bytes_to_directly_convert:]:
        F.mul_mod(res, window_size, inplace = True)
        F.add_mod(res, fr.from_int(b), inplace = True)
    return res



def from_le_bytes_mod_order(bytes: bytearray):
    """
    Reads bytes in little-endian, and converts them to a field element.
    If the integer represented by `bytes` is larger than the modulus `p`, this method
    performs the appropriate reduction.
    """
    return from_be_bytes_mod_order(bytes[::-1])
