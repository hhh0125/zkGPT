#pragma once

#include "zkgpt_protocol.hpp"

#include <cstdint>
#include <string>
#include <vector>

std::vector<std::uint8_t> serializeZkGPTPublicStatement(
    const ZkGPTPublicStatement &statement);
bool deserializeZkGPTPublicStatement(
    const std::vector<std::uint8_t> &bytes,
    ZkGPTPublicStatement &statement,
    std::string *error = nullptr);

std::vector<std::uint8_t> serializeZkGPTProof(const ZkGPTProof &proof);
bool deserializeZkGPTProof(const std::vector<std::uint8_t> &bytes,
                           ZkGPTProof &proof,
                           std::string *error = nullptr);

