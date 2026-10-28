#pragma once
#include "mapped_input.hpp"
#include "exp.hpp"
#include <functional>
namespace zkgpt::r1 {
using LookupProgress=std::function<void(const std::string&)>;
struct RangeLookup {std::size_t child=0;std::string id;unsigned bits=0;};
struct ExpLookup {std::size_t index_child=0,value_child=0;std::string id;ExpProfile profile=ExpProfile::Unclamped;};
struct LookupBatchSpec {
    MappedInputSpec input;
    std::vector<RangeLookup> ranges;
    std::vector<ExpLookup> exps;
    void validate() const;
};
struct LookupBatchProof {
    std::vector<RangeProof> ranges;
    std::vector<ExpProof> exps;
    MappedInputProof input;
};
struct LookupResources {
    std::size_t query_elements=0,max_generators=0,max_sumcheck_rounds=0;
    std::size_t estimated_prover_bytes=0;
};
RangeSpec rangeSpec(const LookupBatchSpec &s,std::size_t j);
ExpSpec expSpec(const LookupBatchSpec &s,std::size_t j);
void appendLookupBatch(const LookupBatchSpec &s,Fs &t);
LookupResources planLookupBatch(const LookupBatchSpec &s);
void requireLookupBudget(const LookupBatchSpec &s,std::size_t bytes);
// Local Lasso phase can precede another protocol. Its input proof is completed
// only after all local claims have been verified and fixed by the caller.
LookupBatchProof proveLookupLocals(const LookupBatchSpec &s,
    const std::vector<std::vector<Fr>> &children,Fs &t,const LookupProgress &progress={});
void verifyLookupLocals(const LookupBatchSpec &s,const LookupBatchProof &p,Fs &t,const LookupProgress &progress={});
LookupBatchProof proveLookupBatch(const LookupBatchSpec &s,const std::vector<Fr> &w,
    const std::vector<std::vector<Fr>> &children,Fs &t);
void verifyLookupBatch(const LookupBatchSpec &s,const LookupBatchProof &p,Fs &t);
} // namespace zkgpt::r1
