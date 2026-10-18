#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <iostream>
#include <cmath>
#include <algorithm>

namespace stats {
    inline std::mutex mu;
    inline std::unordered_map<std::string, __uint128_t> max_abs;
    inline std::unordered_map<std::string, int> max_bits;
    inline std::vector<std::string> search_calls;

    inline __uint128_t abs_u128(__int128 v) {
        return v < 0 ? (__uint128_t)(-v) : (__uint128_t)v;
    }

    inline int bit_width_u128(__uint128_t v) {
        int bits = 0;
        while (v) {
            ++bits;
            v >>= 1;
        }
        return bits;
    }

    inline std::string u128_to_string(__uint128_t v) {
        if (v == 0) return "0";
        std::string s;
        while (v > 0) {
            int d = (int)(v % 10);
            s.push_back((char)('0' + d));
            v /= 10;
        }
        std::reverse(s.begin(), s.end());
        return s;
    }

    inline void update_max(const std::string &k, __int128 v) {
        __uint128_t av = abs_u128(v);
        int bits = bit_width_u128(av);
        std::lock_guard<std::mutex> lk(mu);
        auto it = max_abs.find(k);
        if (it == max_abs.end() || av > it->second) {
            max_abs[k] = av;
            std::cout << "[stats] " << k << " updated -> " << u128_to_string(av) << "\n";
        }
        auto itb = max_bits.find(k);
        if (itb == max_bits.end() || bits > itb->second) {
            max_bits[k] = bits;
        }
    }

    inline void update_bitwidth(const std::string &k, __int128 v) {
        int bits = bit_width_u128(abs_u128(v));
        std::lock_guard<std::mutex> lk(mu);
        auto it = max_bits.find(k);
        if (it == max_bits.end() || bits > it->second) {
            max_bits[k] = bits;
        }
    }

    // inline void record_search(double scale, int e, int c) {
    //     std::lock_guard<std::mutex> lk(mu);
    //     char buf[200];
    //     sprintf(buf, "search(scale=%.12g) -> (e=%d,c=%d)", scale, e, c);
    //     search_calls.emplace_back(buf);
    // }

    inline void print_stats() {
        std::lock_guard<std::mutex> lk(mu);
        // std::cout << "\n=== Quantization / aux stats ===\n";
        // for (auto &s: search_calls) std::cout << s << "\n";
        std::cout << "-- max abs values --\n";
        for (auto &p: max_abs) {
            std::cout << p.first << ": " << u128_to_string(p.second);
            auto itb = max_bits.find(p.first);
            if (itb != max_bits.end()) std::cout << " (bits=" << itb->second << ")";
            std::cout << "\n";
        }
        std::cout << "-- max bit widths --\n";
        for (auto &p: max_bits) {
            std::cout << p.first << ": " << p.second << "\n";
        }

        std::cout << "=== end stats ===\n";
    }
}
