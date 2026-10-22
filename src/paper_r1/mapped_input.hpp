#pragma once
#include "range.hpp"
namespace zkgpt::r1 {
// Verifier-owned layout. Concatenated spans select actual child entries;
// every remaining padded child entry must equal zero. Repeated references
// are permitted and counted, including across differently sized children.
struct InputSpan { std::size_t offset=0,count=0; Fr bias=0; };
struct MappedVector { Commitment commitment; std::vector<InputSpan> spans; };
struct PublicInputValue { std::size_t index=0; Fr value=0; };
struct MappedInputSpec {
    std::string context;
    Commitment master;
    std::vector<MappedVector> children;
    std::vector<PublicInputValue> public_values;
    void validate() const;
};
// Only the composition verifier may supply these, after local proof success.
// They evaluate a zero-padded gather from the same master, with no new oracle.
struct CheckedInputClaim {
    std::vector<Fr> point;
    Fr value=0;
    std::vector<InputSpan> spans;
};
void appendCheckedInput(const MappedInputSpec &s,const std::vector<CheckedInputClaim> &claims,Fs &t);
struct MappedInputProof {
    std::vector<OpenProof> children;
    SumcheckProof sumcheck;
    OpenProof master;
};
void appendMappedInput(const MappedInputSpec &s,Fs &t);
std::vector<Fr> gatherInput(const MappedInputSpec &s,std::size_t child,const std::vector<Fr> &w);
MappedInputProof proveMappedInput(const MappedInputSpec &s,const std::vector<Fr> &w,
    const std::vector<std::vector<Fr>> &children,Fs &t,
    const std::vector<CheckedInputClaim> &checked={});
void verifyMappedInput(const MappedInputSpec &s,const MappedInputProof &p,Fs &t,
    const std::vector<CheckedInputClaim> &checked={});
} // namespace zkgpt::r1
