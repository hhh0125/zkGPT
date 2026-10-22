#pragma once
#include "memory.hpp"
namespace zkgpt::r1 {
struct LinearProof {
    SumcheckProof sumcheck;
    OpenProof endpoint;
};
LinearProof proveLinear(const PCSpec &s, const Commitment &c, const std::vector<Fr> &w,
                        const std::vector<Fr> &coefficients, const Fr &claim, Fs &t,
                        const std::string &label);
void verifyLinear(const PCSpec &s, const Commitment &c, const std::vector<Fr> &coefficients,
                  const Fr &claim, const LinearProof &p, Fs &t, const std::string &label);
LinearProof proveZeroTail(const PCSpec &s, const Commitment &c, const std::vector<Fr> &w, Fs &t);
void verifyZeroTail(const PCSpec &s, const Commitment &c, const LinearProof &p, Fs &t);
struct RangeSpec {
    std::string id;
    PCSpec query;
    unsigned bits = 0;
    void validate() const;
};
struct RangeProof {
    std::vector<Commitment> digits;
    OpenProof query;
    std::vector<OpenProof> digit_values;
    std::vector<MemoryProof> memories;
    LinearProof tail;
};
PCSpec digitSpec(const RangeSpec &s, unsigned h);
MemorySpec digitMemory(const RangeSpec &s, unsigned h);
std::vector<Fr> rangeRowPoint(const RangeSpec &s, const Commitment &c, const RangeProof &p, Fs &t);
RangeProof proveRange(const RangeSpec &s, const Commitment &c, const std::vector<Fr> &w, Fs &t);
void verifyRange(const RangeSpec &s, const Commitment &c, const RangeProof &p, Fs &t);
struct RangeBindingProof {
    OpenProof source, encoded;
};
RangeBindingProof proveRangeBinding(const RangeSpec &s, const PCSpec &source, const Commitment &cs,
                                    const Commitment &cq, const std::vector<Fr> &raw,
                                    const std::vector<Fr> &q, bool is_signed, Fs &t);
void verifyRangeBinding(const RangeSpec &s, const PCSpec &source, const Commitment &cs,
                        const Commitment &cq, bool is_signed, const RangeBindingProof &p, Fs &t);
} // namespace zkgpt::r1
