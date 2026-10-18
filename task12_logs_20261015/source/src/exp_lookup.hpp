#pragma once
#include "range_protocol.hpp"
using ExpTableDigest = std::array<std::uint8_t,32>;
constexpr std::size_t kExpTableSize=655360;
constexpr std::size_t kExpTailStart=6891;
struct ExpQueryPosition {
    std::size_t operator_id, query_id, t_witness_index, E_witness_index;
};
struct ExpLookupProof {
    ExpTableDigest exp_table_id{};
    std::size_t query_count=0;
    // A canonical reuse of the existing bounded Range proof codec. This is
    // NOT a Range proof: the v4 verifier interprets its exact fixed layout.
    RangeProof payload;
};
std::uint32_t canonicalExpValue(std::uint64_t t);
ExpTableDigest canonicalExpTableId();
std::vector<ExpQueryPosition> expQueryLayout(const RangePublicStatement &);
ExpLookupProof proveExpLookup(const RangePublicStatement &,
    const std::vector<Fr> &witness, const Fr &context, int threads,
    bool adversarial_skip_sum_check=false);
bool verifyExpLookup(const RangePublicStatement &, const ExpLookupProof &,
    const Fr &context, std::string *error=nullptr);
