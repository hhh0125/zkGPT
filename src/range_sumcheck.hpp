#pragma once

#include "range_protocol.hpp"

#include <string>
#include <vector>

Degree1SumcheckProof proveDegree1Sumcheck(
    const std::vector<Fr> &values, const Fr &initial_claim,
    Transcript &transcript, const std::string &label,
    std::vector<Fr> *opening_point = nullptr);

bool verifyDegree1Sumcheck(
    const Fr &initial_claim, const Degree1SumcheckProof &proof,
    Transcript &transcript, const std::string &label,
    std::vector<Fr> *opening_point = nullptr,
    std::string *error = nullptr);

Degree3SumcheckProof proveDegree3Sumcheck(
    const std::vector<Fr> &equality_point,
    const std::vector<Fr> &f_values,
    const std::vector<Fr> &g_values, const Fr &initial_claim,
    Transcript &transcript, const std::string &label,
    std::vector<Fr> *opening_point = nullptr);

bool verifyDegree3Sumcheck(
    const std::vector<Fr> &equality_point, const Fr &initial_claim,
    const Degree3SumcheckProof &proof, Transcript &transcript,
    const std::string &label, std::vector<Fr> *opening_point = nullptr,
    std::string *error = nullptr);
