#pragma once

#include "range_protocol.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace range_logup {

// Commit a nonnegative, power-of-two integer vector in the same column-major
// Hyrax layout used by the LogUp prover.  Kept as a kernel-level API so its
// optimized path can be checked directly against an ordinary MSM.
std::vector<G1> commitIntegerValues(
    const std::vector<ll> &values, const std::vector<G1> &generators,
    int threads);

}  // namespace range_logup

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

// Fixed public arbitrary Fr table. Counts are committed before the pole
// challenge; the caller binds query commitments and the context to its statement.
LogUpProof proveFieldLogUp(std::size_t query_index,
    const std::vector<Fr> &values, const std::vector<Fr> &table,
    const std::vector<ll> &counts, const std::vector<G1> &generators,
    const G1 &u, int threads, const Fr &context,
    bool adversarial_skip_sum_check = false);
bool verifyFieldLogUp(std::size_t query_size, std::size_t query_index,
    const std::vector<Fr> &table, const std::vector<G1> &query_commitment,
    const LogUpProof &, const std::vector<G1> &generators, const G1 &u,
    const Fr &context, std::string *error = nullptr);
std::vector<G1> commitLookupFields(const std::vector<Fr> &,
    const std::vector<G1> &, int threads);
