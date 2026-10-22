#pragma once
#include "input.hpp"
namespace zkgpt::r1 {
enum class GateKind : unsigned { Add = 0, Multiply = 1 };
struct Gate {
    GateKind kind;
    std::size_t left, right;
    Fr coefficient = 1;
};
struct GkrSpec {
    std::string context;
    std::size_t input_size = 0;
    std::vector<std::vector<Gate>> layers;
    std::vector<Fr> output;
    void validate() const;
};
struct GkrLayerProof {
    SumcheckProof sumcheck;
    Fr left = 0, right = 0;
};
struct GkrProof {
    std::vector<GkrLayerProof> layers;
};
std::vector<std::vector<Fr>> evaluate(const GkrSpec &s, const std::vector<Fr> &input);
GkrProof proveGkr(const GkrSpec &s, const std::vector<Fr> &input, Fs &t);
std::vector<InputClaim> verifyGkr(const GkrSpec &s, const GkrProof &p, Fs &t);
void appendGkrStatement(const GkrSpec &s, Fs &t);
} // namespace zkgpt::r1
