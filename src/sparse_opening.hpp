#pragma once

#include "range_protocol.hpp"

#include <string>
#include <vector>

SparseLinearOpeningProof proveSparseVal0Opening(
    const RangePublicStatement &statement, std::size_t query_index,
    const std::vector<Fr> &point, const std::vector<F> &val0_witness,
    const Fr &encoded_evaluation, Transcript &transcript);

bool verifySparseVal0Opening(
    const RangePublicStatement &statement, std::size_t query_index,
    const std::vector<Fr> &point, const Fr &encoded_evaluation,
    const SparseLinearOpeningProof &proof, Transcript &transcript,
    std::string *error = nullptr);
