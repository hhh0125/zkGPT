#pragma once
#include "host_safety.hpp"
#include "global_var.hpp"
#include <memory>
#include "paper_r7_native_workers.hpp"
#include <array>
#include <chrono>
#include <limits>
#include <vector>

namespace zkgpt::r7::oneblock::detail {

inline void addScaled(F& out, const F& value, long long coefficient) {
    if (coefficient == 1) out += value;
    else if (coefficient == -1) out -= value;
    else if (coefficient != 0) out += value * host_safety::field(coefficient);
}

using GkrClock = std::chrono::steady_clock;
inline double gkrElapsed(GkrClock::time_point start) {
    return std::chrono::duration<double>(GkrClock::now() - start).count();
}

inline void expandBeta(std::vector<F>& weights, unsigned bits,
                       std::vector<F>::const_iterator point, const F& scale) {
    weights.resize(std::size_t{1} << bits);
    weights[0] = scale;
    for (std::size_t n = 1; bits; --bits, ++point, n *= 2) {
        for (std::size_t j = 0; j < n; ++j) {
            weights[j+n] = weights[j] * *point;
            weights[j] -= weights[j+n];
        }
    }
}

inline void gkrBeta(std::vector<F>& out, unsigned bits,
                    std::vector<F>::const_iterator first, const F& first_scale,
                    std::vector<F>::const_iterator second, const F& second_scale,
                    NativeWorkers& pool, std::size_t zero_start,
                    const F& zero_scale) {
    const auto low_bits = bits / 2;
    const auto mask = (std::size_t{1} << low_bits) - 1;
    const bool one = !first_scale.isZero(), two = !second_scale.isZero();
    std::vector<F> low1, high1, low2, high2;
    if (one) {
        expandBeta(low1, low_bits, first, first_scale);
        expandBeta(high1, bits-low_bits, first+low_bits, F_ONE);
    }
    if (two) {
        expandBeta(low2, low_bits, second, second_scale);
        expandBeta(high2, bits-low_bits, second+low_bits, F_ONE);
    }
    out.resize(std::size_t{1} << bits);
    const auto scaled = zero_start < out.size() && zero_scale != F_ONE;
    nativeRanges(out.size(), pool, 4096, [&](std::size_t, std::size_t begin, std::size_t end) {
        for (auto i = begin; i < end; ++i) {
            const auto lo = i & mask, hi = i >> low_bits;
            F w = one ? low1[lo] * high1[hi] : F_ZERO;
            if (two) w += low2[lo] * high2[hi];
            if (scaled && i >= zero_start) w *= zero_scale;
            out[i] = w;
        }
    });
}

struct GatePartitionTiming {
    unsigned workers = 1;
    std::size_t index_bytes = 0;
    double partition_seconds = 0, accumulate_seconds = 0, max_worker_seconds = 0;
    double total_worker_seconds = 0;
};

// Assign every 64-coordinate tile, in either workspace branch, to one owner.
// Rows are input ranges and columns are target owners. Scanning input ranges
// in order during prefix assignment keeps the original order within each owner.
template<class Gate, class Coordinate, class Accumulate>
GatePartitionTiming accumulateGates(const std::vector<Gate>& gates, NativeWorkers& pool,
                                    const Coordinate& coordinate,
                                    const Accumulate& accumulate) {
    const auto n = nativePartitions(gates.size(), pool, 8192);
    GatePartitionTiming timing;
    timing.workers = n;
    auto start = GkrClock::now();
    if (n == 1 || gates.size() > std::numeric_limits<std::uint32_t>::max()) {
        timing.workers = 1;
        for (const auto& gate : gates) accumulate(gate);
        timing.accumulate_seconds = timing.max_worker_seconds =
            timing.total_worker_seconds = gkrElapsed(start);
        return timing;
    }
    struct alignas(64) Counts {
        std::array<std::size_t,NativeWorkers::capacity> offsets{};
    };
    std::array<Counts,NativeWorkers::capacity> ranges{};
    std::array<std::size_t,NativeWorkers::capacity+1> offsets{};
    auto owner = [&](const Gate& gate) { return (coordinate(gate) / 64) % n; };
    pool.run(n, [&](std::size_t k) {
        const auto end = gates.size()*(k+1)/n;
        for (auto i = gates.size()*k/n; i < end; ++i)
            ++ranges[k].offsets[owner(gates[i])];
    }, true);
    std::size_t next = 0;
    for (unsigned k = 0; k < n; ++k) {
        offsets[k] = next;
        for (unsigned row = 0; row < n; ++row) {
            auto& entry = ranges[row].offsets[k];
            const auto count = entry;
            entry = next;
            next += count;
        }
    }
    offsets[n] = next;
    std::unique_ptr<std::uint32_t[]> ids(new std::uint32_t[gates.size()]);
    pool.run(n, [&](std::size_t k) {
        const auto end = gates.size()*(k+1)/n;
        for (auto i = gates.size()*k/n; i < end; ++i)
            ids[ranges[k].offsets[owner(gates[i])]++] = static_cast<std::uint32_t>(i);
    }, true);
    timing.partition_seconds = gkrElapsed(start);
    timing.index_bytes = gates.size()*sizeof(ids[0]);
    std::array<double,NativeWorkers::capacity> work{};
    start = GkrClock::now();
    pool.run(n, [&](std::size_t k) {
        const auto begin = GkrClock::now();
        for (auto i = offsets[k]; i < offsets[k+1]; ++i) accumulate(gates[ids[i]]);
        work[k] = gkrElapsed(begin);
    }, true);
    timing.accumulate_seconds = gkrElapsed(start);
    for (unsigned k = 0; k < n; ++k) {
        timing.total_worker_seconds += work[k];
        timing.max_worker_seconds = std::max(timing.max_worker_seconds, work[k]);
    }
    return timing;
}
}  // namespace zkgpt::r7::oneblock::detail
