#include "lookup_batch.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace zkgpt::r1 {
RangeSpec rangeSpec(const LookupBatchSpec &s,std::size_t j) {
    const auto &r=s.ranges.at(j);return {r.id,s.input.children.at(r.child).commitment.spec,r.bits};
}
ExpSpec expSpec(const LookupBatchSpec &s,std::size_t j) {
    const auto &e=s.exps.at(j);
    return {e.id,s.input.children.at(e.index_child).commitment.spec,
        s.input.children.at(e.value_child).commitment.spec,e.profile};
}
void LookupBatchSpec::validate() const {
    input.validate();require(!ranges.empty() || !exps.empty(),"no lookup obligations");
    require(ranges.size()<=65536 && exps.size()<=65536,"lookup obligation limit");
    std::set<std::string> ids;
    for (std::size_t j=0;j<ranges.size();++j) {
        require(ids.insert(ranges[j].id).second,"duplicate lookup ID");rangeSpec(*this,j).validate();
    }
    for (std::size_t j=0;j<exps.size();++j) {
        require(ids.insert(exps[j].id).second,"duplicate lookup ID");expSpec(*this,j).validate();
    }
}
void appendLookupBatch(const LookupBatchSpec &s,Fs &t) {
    s.validate();t.text("lookup.protocol","indexed-Lasso-mapped-input-v1");
    appendMappedInput(s.input,t);
    t.integer("lookup.ranges",s.ranges.size());
    for (const auto &r:s.ranges) {
        t.integer("lookup.child",r.child);t.text("lookup.id",r.id);t.integer("lookup.bits",r.bits);
    }
    t.integer("lookup.exps",s.exps.size());
    for (const auto &e:s.exps) {
        t.integer("lookup.index",e.index_child);t.integer("lookup.value",e.value_child);t.text("lookup.id",e.id);t.integer("lookup.Exp.profile",unsigned(e.profile));
        auto digest=expTableDigest(e.profile);t.bytes("lookup.Exp.instance-table",Bytes(digest.begin(),digest.end()));
    }
    const auto digest=expTableDigest();t.bytes("lookup.Exp.table",Bytes(digest.begin(),digest.end()));
}
namespace {
std::size_t add(std::size_t a,std::size_t b) {
    require(b<=std::numeric_limits<std::size_t>::max()-a,"lookup resource overflow");return a+b;
}
std::size_t mul(std::size_t a,std::size_t b) {
    require(!a || b<=std::numeric_limits<std::size_t>::max()/a,"lookup resource overflow");return a*b;
}
}
LookupResources planLookupBatch(const LookupBatchSpec &s,std::size_t checked_input_padded) {
    if(checked_input_padded)log2exact(checked_input_padded); // validates power of two and field-domain cap
    s.validate();LookupResources p;
    p.max_generators=s.input.master.spec.columns;
    p.max_sumcheck_rounds=log2exact(s.input.master.spec.padded);
    std::size_t retained=0,lookup_peak=0,max_child=0;
    for (const auto &c:s.input.children) {
        const auto &pc=c.commitment.spec;p.query_elements=add(p.query_elements,pc.padded);
        retained=add(retained,mul(pc.padded,sizeof(Fr)));
        max_child=std::max(max_child,mul(pc.padded,sizeof(Fr)));
        p.max_generators=std::max(p.max_generators,pc.columns);
        p.max_sumcheck_rounds=std::max(p.max_sumcheck_rounds,std::size_t(log2exact(pc.padded)));
    }
    // Sequential local proofs; all query vectors retained. Conservative count
    // includes digits, product trees, sumcheck factors, IPA working vectors.
    // Not an OS RSS guarantee: allocator, circuit and GKR storage are separate.
    for (std::size_t j=0;j<s.ranges.size();++j) {
        auto r=rangeSpec(s,j);std::size_t digits=(r.bits+8)/9;
        const auto table_size=std::size_t(1)<<std::min(9U,r.bits);
        const auto table_bytes=mul(mul(table_size,16),sizeof(Fr));
        lookup_peak=std::max(lookup_peak,add(mul(mul(r.query.padded,digits+32),sizeof(Fr)),table_bytes));
        p.max_generators=std::max(p.max_generators,std::size_t(32));
        p.max_sumcheck_rounds=std::max(p.max_sumcheck_rounds,std::size_t(log2exact(table_size)));
    }
    for (std::size_t j=0;j<s.exps.size();++j) {
        auto e=expSpec(s,j);
        lookup_peak=std::max(lookup_peak,add(mul(mul(e.index.padded,48),sizeof(Fr)),8192*32*16));
        p.max_generators=std::max(p.max_generators,std::size_t(128));
        p.max_sumcheck_rounds=std::max(p.max_sumcheck_rounds,std::size_t(13));
    }
    // Mapped input: immutable master, moved kernel and one folding-witness
    // copy = three master buffers. Budget four, keeping a full extra master
    // for allocator/lifetime slack. Kernel construction has one temporary
    // child or checked-input equality vector; include unusually large domains.
    // No initializer_list copies and no gathered-child validation temporary.
    const auto master_bytes=mul(s.input.master.spec.padded,sizeof(Fr));
    const auto master_peak=std::max(mul(master_bytes,4),add(mul(master_bytes,2),std::max(max_child,mul(checked_input_padded,sizeof(Fr)))));
    p.retained_query_bytes=retained;p.local_lookup_workspace_bytes=lookup_peak;
    p.mapped_input_workspace_bytes=master_peak;
    p.estimated_prover_bytes=add(retained,std::max(lookup_peak,master_peak));
    return p;
}
void requireLookupBudget(const LookupBatchSpec &s,std::size_t bytes) {
    require(planLookupBatch(s).estimated_prover_bytes<=bytes,"lookup exceeds selected memory budget");
}
#ifndef R1_VERIFY_ONLY
LookupBatchProof proveLookupLocals(const LookupBatchSpec &s,
    const std::vector<std::vector<Fr>> &children,Fs &t,const LookupProgress &progress) {
    appendLookupBatch(s,t);require(children.size()==s.input.children.size(),"lookup witness shape");
    LookupBatchProof p;
    for (std::size_t j=0;j<s.ranges.size();++j) {
        if(progress)progress("range/"+std::to_string(j)+"/begin");
        auto r=rangeSpec(s,j);auto k=s.ranges[j].child;
        p.ranges.push_back(proveRange(r,s.input.children[k].commitment,children[k],t));
        if(progress)progress("range/"+std::to_string(j)+"/end");
    }
    for (std::size_t j=0;j<s.exps.size();++j) {
        if(progress)progress("exp/"+std::to_string(j)+"/begin");
        const auto &e=s.exps[j];
        p.exps.push_back(proveExp(expSpec(s,j),s.input.children[e.index_child].commitment,
            s.input.children[e.value_child].commitment,children[e.index_child],children[e.value_child],t));
        if(progress)progress("exp/"+std::to_string(j)+"/end");
    }
    return p;
}
LookupBatchProof proveLookupBatch(const LookupBatchSpec &s,const std::vector<Fr> &w,
    const std::vector<std::vector<Fr>> &children,Fs &t) {
    auto p=proveLookupLocals(s,children,t);
    p.input=proveMappedInput(s.input,w,children,t);return p;
}
#endif
void verifyLookupLocals(const LookupBatchSpec &s,const LookupBatchProof &p,Fs &t,const LookupProgress &progress) {
    appendLookupBatch(s,t);
    require(p.ranges.size()==s.ranges.size() && p.exps.size()==s.exps.size(),"lookup proof obligation count");
    for (std::size_t j=0;j<s.ranges.size();++j) {
        if(progress)progress("range/"+std::to_string(j)+"/begin");
        verifyRange(rangeSpec(s,j),s.input.children[s.ranges[j].child].commitment,p.ranges[j],t);
        if(progress)progress("range/"+std::to_string(j)+"/end");
    }
    for (std::size_t j=0;j<s.exps.size();++j) {
        if(progress)progress("exp/"+std::to_string(j)+"/begin");
        const auto &e=s.exps[j];verifyExp(expSpec(s,j),s.input.children[e.index_child].commitment,
            s.input.children[e.value_child].commitment,p.exps[j],t);
        if(progress)progress("exp/"+std::to_string(j)+"/end");
    }
}
void verifyLookupBatch(const LookupBatchSpec &s,const LookupBatchProof &p,Fs &t) {
    verifyLookupLocals(s,p,t);verifyMappedInput(s.input,p.input,t);
}
} // namespace zkgpt::r1
