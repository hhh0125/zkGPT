#pragma once
#include "range.hpp"
namespace zkgpt::r1 {
// Explicit mathematical instances; not an assertion about the old GPT-2 profile.
// E(t)=round(2^20 exp(-t/512)), t in u32, without clamp.
enum class ExpProfile : unsigned { Unclamped=0, LegacyClampOne=1 };
struct ExpSpec {
    std::string id;
    PCSpec index, value;
    ExpProfile profile=ExpProfile::Unclamped;
    void validate() const;
};
struct ExpProof {
    std::array<Commitment, 4> helper;
    std::array<RangeProof, 4> range;
    std::array<SumcheckProof, 2> constraints;
    std::array<std::array<OpenProof, 3>, 2> endpoint;
    std::array<OpenProof, 2> value_binding;
    MemoryProof memory;
};
std::uint32_t expValue(std::uint32_t t,ExpProfile profile=ExpProfile::Unclamped);
const std::vector<Fr> &expTable(ExpProfile profile=ExpProfile::Unclamped);
Digest expTableDigest(ExpProfile profile=ExpProfile::Unclamped);
PCSpec expHelperSpec(const ExpSpec &s, unsigned k);
RangeSpec expRangeSpec(const ExpSpec &s, unsigned k);
MemorySpec expMemorySpec(const ExpSpec &s);
ExpProof proveExp(const ExpSpec &s, const Commitment &ct, const Commitment &ce,
                  const std::vector<Fr> &index, const std::vector<Fr> &value, Fs &t);
void verifyExp(const ExpSpec &s, const Commitment &ct, const Commitment &ce, const ExpProof &p,
               Fs &t);
} // namespace zkgpt::r1
