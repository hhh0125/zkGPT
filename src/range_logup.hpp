#pragma once

#include "range_protocol.hpp"

#include <cstdint>
#include <string>
#include <vector>

LogUpProof proveLogUp(
    std::size_t query_index, std::size_t chunk_index,
    unsigned chunk_bits, const std::vector<std::uint16_t> &values,
    const std::vector<G1> &generators, const G1 &u,
    int thread_count);

bool verifyLogUp(
    const PublicRangeQuery &query, std::size_t query_index,
    std::size_t chunk_index, const std::vector<G1> &chunk_commitment,
    const LogUpProof &proof, const std::vector<G1> &generators,
    const G1 &u, std::string *error = nullptr);
