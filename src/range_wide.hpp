#pragma once

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace range_wide {

using SignedWide = __int128;
using UnsignedWide = unsigned __int128;

constexpr unsigned kMaxBits = 126;
constexpr unsigned kDefaultChunkBits = 9;

struct EncodedWitnessValue {
    UnsignedWide encoded = 0;

    bool operator==(const EncodedWitnessValue &other) const {
        return encoded == other.encoded;
    }
    bool operator!=(const EncodedWitnessValue &other) const {
        return !(*this == other);
    }
};

struct RangeChunkQuery {
    unsigned chunk_bits = 0;
    std::vector<std::uint16_t> chunks;
};

inline UnsignedWide limitForBits(unsigned bits) {
    if (bits == 0 || bits > kMaxBits)
        throw std::invalid_argument("wide range bit width must be in [1, 126]");
    return static_cast<UnsignedWide>(1) << bits;
}

inline UnsignedWide encodeSigned(SignedWide value, unsigned bits) {
    const UnsignedWide limit = limitForBits(bits);
    const SignedWide half = static_cast<SignedWide>(limit >> 1);
    if (value < -half || value >= half)
        throw std::range_error("signed value does not fit the declared bit width");
    return static_cast<UnsignedWide>(value + half);
}

inline UnsignedWide encodeUnsigned(SignedWide value, unsigned bits) {
    const UnsignedWide limit = limitForBits(bits);
    if (value < 0 || static_cast<UnsignedWide>(value) >= limit)
        throw std::range_error("unsigned value does not fit the declared bit width");
    return static_cast<UnsignedWide>(value);
}

inline std::vector<std::uint16_t> decomposeWide(
    UnsignedWide value, unsigned bits,
    unsigned chunk_bits = kDefaultChunkBits) {
    const UnsignedWide limit = limitForBits(bits);
    if (chunk_bits == 0 || chunk_bits > 16)
        throw std::invalid_argument("chunk width must be in [1, 16]");
    if (value >= limit)
        throw std::range_error("encoded value does not fit the declared bit width");

    const unsigned chunk_count = (bits + chunk_bits - 1) / chunk_bits;
    std::vector<std::uint16_t> chunks;
    chunks.reserve(chunk_count);
    for (unsigned index = 0; index < chunk_count; ++index) {
        const unsigned offset = index * chunk_bits;
        const unsigned actual_bits = std::min(chunk_bits, bits - offset);
        const UnsignedWide mask =
            (static_cast<UnsignedWide>(1) << actual_bits) - 1;
        chunks.push_back(static_cast<std::uint16_t>((value >> offset) & mask));
    }
    return chunks;
}

inline UnsignedWide reconstructWide(
    const std::vector<std::uint16_t> &chunks, unsigned bits,
    unsigned chunk_bits = kDefaultChunkBits) {
    limitForBits(bits);
    if (chunk_bits == 0 || chunk_bits > 16)
        throw std::invalid_argument("chunk width must be in [1, 16]");
    const unsigned expected_count = (bits + chunk_bits - 1) / chunk_bits;
    if (chunks.size() != expected_count)
        throw std::invalid_argument("incorrect number of range chunks");

    UnsignedWide value = 0;
    for (unsigned index = 0; index < expected_count; ++index) {
        const unsigned offset = index * chunk_bits;
        const unsigned actual_bits = std::min(chunk_bits, bits - offset);
        const std::uint32_t chunk_limit = static_cast<std::uint32_t>(1)
                                          << actual_bits;
        if (chunks[index] >= chunk_limit)
            throw std::range_error("range chunk exceeds its declared width");
        value |= static_cast<UnsignedWide>(chunks[index]) << offset;
    }
    return value;
}

inline std::string unsignedWideToString(UnsignedWide value) {
    if (value == 0) return "0";
    std::string result;
    while (value != 0) {
        result.push_back(static_cast<char>('0' + value % 10));
        value /= 10;
    }
    std::reverse(result.begin(), result.end());
    return result;
}

}  // namespace range_wide
