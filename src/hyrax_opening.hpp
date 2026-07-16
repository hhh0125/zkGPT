#pragma once

#include "range_protocol.hpp"

#include <cstdint>
#include <string>
#include <vector>

HyraxIpaProof hyraxInnerProductProve(
    const std::vector<Fr> &witness, const std::vector<Fr> &coefficients,
    const std::vector<G1> &generators, const G1 &u,
    const G1 &commitment, const Fr &evaluation, Transcript &transcript,
    const std::string &label);

bool hyraxInnerProductVerify(
    const std::vector<Fr> &coefficients,
    const std::vector<G1> &generators, const G1 &u,
    const G1 &commitment, const Fr &evaluation,
    const HyraxIpaProof &proof, Transcript &transcript,
    const std::string &label, std::string *error = nullptr);

MleOpeningProof hyraxMleOpenProve(
    const std::vector<std::uint16_t> &values,
    const std::vector<G1> &row_commitments,
    const std::vector<Fr> &point,
    const std::vector<G1> &generators, const G1 &u,
    const Fr &evaluation, Transcript &transcript,
    const std::string &label);

MleOpeningProof hyraxMleOpenProveFr(
    const std::vector<Fr> &values,
    const std::vector<G1> &row_commitments,
    const std::vector<Fr> &point,
    const std::vector<G1> &generators, const G1 &u,
    const Fr &evaluation, Transcript &transcript,
    const std::string &label);

bool hyraxMleOpenVerify(
    const std::vector<G1> &row_commitments,
    const std::vector<Fr> &point,
    const std::vector<G1> &generators, const G1 &u,
    const Fr &evaluation, const MleOpeningProof &proof,
    Transcript &transcript, const std::string &label,
    std::string *error = nullptr);
