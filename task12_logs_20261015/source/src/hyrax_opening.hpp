#pragma once

#include "range_protocol.hpp"

#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

HyraxIpaProof hyraxInnerProductProve(
    const std::vector<Fr> &witness, const std::vector<Fr> &coefficients,
    const std::vector<G1> &generators, const G1 &u,
    const G1 &commitment, const Fr &evaluation, Transcript &transcript,
    const std::string &label, bool check_witness_commitment = true);

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

// C++17 bounded read-only view. The caller owns storage through the synchronous
// opening call; the view is never retained in a proof or background task.
class HyraxFrWitnessView {
public:
    HyraxFrWitnessView(const Fr *data, std::size_t size):data_(data),size_(size) {
        if (!data && size) throw std::invalid_argument("null Hyrax witness view");
    }
    std::size_t size() const noexcept { return size_; }
    const Fr &operator[](std::size_t index) const noexcept { return data_[index]; }
private:
    const Fr *data_;
    std::size_t size_;
};

MleOpeningProof hyraxMleOpenProveFrRowMajor(
    HyraxFrWitnessView values,
    const std::vector<G1> &row_commitments,
    const std::vector<Fr> &point,
    const std::vector<G1> &generators, const G1 &u,
    const Fr &evaluation, Transcript &transcript,
    const std::string &label);

MleOpeningProof hyraxMleOpenProveFrRowMajor(
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

bool hyraxMleOpenVerifyRowMajor(
    const std::vector<G1> &row_commitments,
    const std::vector<Fr> &point,
    const std::vector<G1> &generators, const G1 &u,
    const Fr &evaluation, const MleOpeningProof &proof,
    Transcript &transcript, const std::string &label,
    std::string *error = nullptr);

// Production little-endian Boolean equality weights, shared by hidden openings.
std::vector<Fr> hyraxMleEqualityWeights(const std::vector<Fr>& point,
                                      std::size_t begin,std::size_t count);
