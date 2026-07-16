#pragma once

#include "range_protocol.hpp"

#include <cstdint>
#include <string>
#include <vector>

constexpr std::uint32_t kRangeArtifactVersion = 1;

std::vector<std::uint8_t> serializeRangePublicStatement(
    const RangePublicStatement &statement);
bool deserializeRangePublicStatement(
    const std::vector<std::uint8_t> &bytes, RangePublicStatement &statement,
    std::string *error = nullptr);

std::vector<std::uint8_t> serializeRangeProof(const RangeProof &proof);
bool deserializeRangeProof(
    const std::vector<std::uint8_t> &bytes, RangeProof &proof,
    std::string *error = nullptr);

