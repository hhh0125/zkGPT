#pragma once

#include "gkr_protocol.hpp"

#include <cstdint>
#include <string>
#include <vector>

std::vector<std::uint8_t> serializeGKRProof(const GKRProof &proof);
bool deserializeGKRProof(const std::vector<std::uint8_t> &bytes,
                         GKRProof &proof, std::string *error = nullptr);

