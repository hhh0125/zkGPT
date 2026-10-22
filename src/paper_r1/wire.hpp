#pragma once
#include "bundle.hpp"
#include "lookup_batch.hpp"
#include "exp.hpp"
#include "division.hpp"
#include "positive_sqrt.hpp"
namespace zkgpt::r1 {
// Separate, versioned foundation artifact. Never accepts strict-v4 or B1/B2 bytes.
constexpr std::size_t kMaxWireBytes = 64 * 1024 * 1024;
Bytes serialize(const BundleStatement &s);
Bytes serialize(const BundleStatement &s, const BundleProof &p);
BundleStatement parseStatement(const Bytes &b);
BundleProof parseProof(const BundleStatement &s, const Bytes &b);
struct ExpStatement {
    ExpSpec spec;
    Commitment index, value;
    void validate() const;
};
Bytes serializeExp(const ExpStatement &s);
Bytes serializeExp(const ExpStatement &s, const ExpProof &p);
ExpStatement parseExpStatement(const Bytes &b);
ExpProof parseExpProof(const ExpStatement &s, const Bytes &b);
struct DivisionStatement {
    DivisionSpec spec;
    Commitment numerator, denominator, quotient;
    void validate() const;
};
Bytes serializeDivision(const DivisionStatement &s);
Bytes serializeDivision(const DivisionStatement &s, const DivisionProof &p);
DivisionStatement parseDivisionStatement(const Bytes &b);
DivisionProof parseDivisionProof(const DivisionStatement &s, const Bytes &b);
struct PositiveSqrtStatement {
    PositiveSqrtSpec spec;
    Commitment input, root;
    void validate() const;
};
Bytes serializePositiveSqrt(const PositiveSqrtStatement &s);
Bytes serializePositiveSqrt(const PositiveSqrtStatement &s, const PositiveSqrtProof &p);
PositiveSqrtStatement parsePositiveSqrtStatement(const Bytes &b);
PositiveSqrtProof parsePositiveSqrtProof(const PositiveSqrtStatement &s, const Bytes &b);
Bytes readFile(const std::string &path);
void writeFile(const std::string &path, const Bytes &b);
} // namespace zkgpt::r1

namespace zkgpt::r1 {
Bytes serializeLookup(const LookupBatchSpec &s);
LookupBatchSpec parseLookupSpec(const Bytes &b);
Bytes serializeLookup(const LookupBatchSpec &s,const LookupBatchProof &p);
LookupBatchProof parseLookupProof(const LookupBatchSpec &s,const Bytes &b);
Bytes serializeMappedInput(const LookupBatchSpec &s,const MappedInputProof &p);
MappedInputProof parseMappedInput(const LookupBatchSpec &s,const Bytes &b);
}
