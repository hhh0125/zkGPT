#pragma once

#include "global_var.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct GeneratorDomain {
    std::string domain;
    std::uint32_t count = 0;
};

struct GeneratorSet {
    std::vector<G1> generators;
    G1 u;
};

constexpr std::uint32_t kMaxGeneratorCount = 1U << 20;

const GeneratorSet &getGeneratorSet(const GeneratorDomain &descriptor);

