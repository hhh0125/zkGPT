#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

enum class WitnessKind {
    ROUNDING,
    LAYER_NORM,
    GELU,
    SOFTMAX
};

inline const char *witnessKindName(WitnessKind kind) {
    switch (kind) {
        case WitnessKind::ROUNDING: return "ROUNDING";
        case WitnessKind::LAYER_NORM: return "LAYER_NORM";
        case WitnessKind::GELU: return "GELU";
        case WitnessKind::SOFTMAX: return "SOFTMAX";
    }
    return "UNKNOWN";
}

struct WitnessShape {
    std::size_t sequence_length = 0;
    std::size_t layer_count = 0;
    std::size_t head_count = 0;
    std::size_t head_dimension = 0;
    std::size_t hidden_dimension = 0;
    std::size_t mlp_dimension = 0;
};

struct RangeConstraint {
    WitnessKind kind;
    std::string name;
    std::size_t val0_offset;
    std::size_t count;
    unsigned bits;
    bool is_signed;
};

inline bool fitsSignedBits(__int128 value, unsigned bits) {
    if (bits == 0 || bits > 126) return false;
    const __int128 half = static_cast<__int128>(1) << (bits - 1);
    return value >= -half && value < half;
}

inline bool fitsUnsignedBits(__int128 value, unsigned bits) {
    if (bits == 0 || bits > 126 || value < 0) return false;
    const unsigned __int128 limit =
        static_cast<unsigned __int128>(1) << bits;
    return static_cast<unsigned __int128>(value) < limit;
}

inline std::string int128ToString(__int128 value) {
    if (value == 0) return "0";
    const bool negative = value < 0;
    unsigned __int128 magnitude = negative
        ? static_cast<unsigned __int128>(-(value + 1)) + 1
        : static_cast<unsigned __int128>(value);
    std::string result;
    while (magnitude != 0) {
        result.push_back(static_cast<char>('0' + magnitude % 10));
        magnitude /= 10;
    }
    if (negative) result.push_back('-');
    std::reverse(result.begin(), result.end());
    return result;
}

class WitnessRegistry {
public:
    void setShape(const WitnessShape &shape) {
        if (shape.sequence_length == 0 || shape.layer_count == 0 ||
            shape.head_count == 0 || shape.head_dimension == 0 ||
            shape.hidden_dimension == 0 || shape.mlp_dimension == 0) {
            throw std::invalid_argument("witness shape dimensions must be nonzero");
        }
        shape_ = shape;
    }

    const WitnessShape &shape() const { return shape_; }

    void addRange(WitnessKind kind, const std::string &name,
                  std::size_t val0_offset, std::size_t count,
                  unsigned bits, bool is_signed) {
        if (count == 0) return;
        if (name.empty())
            throw std::invalid_argument("range constraint name must not be empty");
        // The current Range Proof stores transformed values in signed __int128.
        // Keeping one headroom bit also avoids ambiguous field-to-integer casts.
        if (bits == 0 || bits > 126)
            throw std::invalid_argument("invalid range constraint bit width");
        if (val0_offset > std::numeric_limits<std::size_t>::max() - count)
            throw std::overflow_error("range constraint offset overflow: " + name);
        constraints_.push_back({kind, name, val0_offset, count, bits, is_signed});
    }

    const std::vector<RangeConstraint> &constraints() const {
        return constraints_;
    }

    void validateLayout(std::size_t val0_size) const {
        std::vector<const RangeConstraint *> ordered;
        ordered.reserve(constraints_.size());
        for (const auto &constraint : constraints_) {
            if (constraint.val0_offset > val0_size ||
                constraint.count > val0_size - constraint.val0_offset) {
                std::ostringstream message;
                message << "range constraint " << constraint.name
                        << " is outside val[0]: offset=" << constraint.val0_offset
                        << ", count=" << constraint.count
                        << ", val[0].size()=" << val0_size;
                throw std::out_of_range(message.str());
            }
            ordered.push_back(&constraint);
        }
        std::sort(ordered.begin(), ordered.end(),
                  [](const RangeConstraint *lhs, const RangeConstraint *rhs) {
                      return lhs->val0_offset < rhs->val0_offset;
                  });
        for (std::size_t i = 1; i < ordered.size(); ++i) {
            const auto &previous = *ordered[i - 1];
            const auto &current = *ordered[i];
            if (previous.val0_offset + previous.count > current.val0_offset) {
                throw std::logic_error("overlapping range constraints: " +
                                       previous.name + " and " + current.name);
            }
        }
    }

    std::size_t constrainedValueCount() const {
        std::size_t total = 0;
        for (const auto &constraint : constraints_) {
            if (constraint.count > std::numeric_limits<std::size_t>::max() - total)
                throw std::overflow_error("range constraint count overflow");
            total += constraint.count;
        }
        return total;
    }

    void clear() {
        constraints_.clear();
        shape_ = WitnessShape{};
    }

private:
    WitnessShape shape_;
    std::vector<RangeConstraint> constraints_;
};
