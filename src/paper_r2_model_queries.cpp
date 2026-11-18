#include "paper_r2_model_queries.hpp"
#include "paper_r2_range_split.hpp"
#include <gkr_protocol.hpp>
#include "paper_r1_wire.hpp"
#include <numeric>
namespace zkgpt::r2 {
namespace {
std::size_t padded(std::size_t n){r1::require(n>0,"empty model query");std::size_t p=1;while(p<n){r1::require(p<r1::kMaxVector,"model query size budget");p*=2;}return p;}
std::size_t addSource(ModelQueryPlan &p,const std::string &role,std::vector<r1::InputSpan> spans){
    std::size_t n=0;for(const auto &s:spans){r1::require(s.offset<p.master.actual && s.count<=p.master.actual-s.offset,"model source bounds");r1::require(s.count<=r1::kMaxVector-n,"model source length overflow");n+=s.count;}
    auto index=p.sources.size();p.sources.push_back({r1::PCSpec::make(role,n,padded(n),r1::Layout::RowMajor),std::move(spans)});return index;
}
}
ModelQueryPlan finalizeModelQueries(const layeredCircuit &c,const WitnessRegistry &registry,r1::ExpProfile exp_profile){
    ModelQueryPlan p;p.shape=registry.shape();p.r4_candidate=registry.r4NumericalCandidate();p.r5b_candidate=registry.r5bEngineeringCandidate();p.r6_engineering=registry.r6Engineering();
    r1::require(p.shape.sequence_length==32 && p.shape.hidden_dimension==768 && p.shape.head_dimension==64 && p.shape.head_count==12,"unsupported model query profile");
    p.circuit_id=circuitFingerprintHex(fingerprintCircuit(c));
    p.master=r1::PCSpec::make("r2/model/master",c.circuit[0].size,std::size_t(1)<<c.circuit[0].bit_length,r1::Layout::RowMajor);
    p.constants=registry.constants();p.reserved_zero=registry.reservedZero();
    std::size_t range_group=0;
    for(const auto &group:registry.rangeSources()){
        const auto parent="r2/model/range/"+std::to_string(range_group++);
        std::size_t actual=0;for(const auto&span:group.second)actual+=span.count;
        const auto sizes=rangeSplitSizes(actual,largeRangeSplitEnabled());
        if(sizes.size()==1){
            auto child=addSource(p,parent,group.second);
            p.ranges.push_back({child,parent,group.first.first});
        }else{
            auto pieces=splitRangeSpans(group.second,sizes);
            for(std::size_t part=0;part<pieces.size();++part){
                auto id=parent+"/part/"+std::to_string(part);
                auto child=addSource(p,id,std::move(pieces[part]));
                if(sizes[part]==1)p.sources[child].spec=r1::PCSpec::make(id,1,2,r1::Layout::RowMajor);
                p.ranges.push_back({child,id,group.first.first});
            }
        }
    }
    auto ti=addSource(p,"r2/model/Exp-t",registry.expAddresses()),ei=addSource(p,"r2/model/Exp-E",registry.expValues());
    p.exps.push_back({ti,ei,"r2/model/Exp",exp_profile});
    addSource(p,"r2/model/W",registry.parameterSources());
    r1::require(registry.outputOffset()<=p.master.actual && registry.outputCount()<=p.master.actual-registry.outputOffset(),"model output bounds");
    p.output.resize(registry.outputCount());
    std::iota(p.output.begin(),p.output.end(),registry.outputOffset());
    return p;
}
r1::LookupBatchSpec makeModelQueryStatement(const ModelQueryPlan &p,const r1::Commitment &master,
    const std::vector<r1::Commitment> &children,const r1::Commitment &approved_w,
    const std::vector<Fr> &x,const std::vector<Fr> &y,const std::string &model){
    r1::require(!model.empty() && model.size()<=128,"externally approved model identity required");
    r1::require(master.spec==p.master && children.size()==p.sources.size(),"model query commitment shape");
    r1::require(children.back().spec==approved_w.spec && children.back().rows==approved_w.rows,"publisher W commitment mismatch");
    r1::require(x.size()==32768 && y.size()==p.output.size(),"complete public X/Y required");
    for(std::size_t i=0;i<x.size();++i)if(i%1024>=p.shape.hidden_dimension)r1::require(x[i].isZero() && y[i].isZero(),"public activation padding must be zero");
    r1::LookupBatchSpec s;s.input.context=(p.r6_engineering?"R6-ROWMAJOR-v2-R5B-engineering-candidate-NOT-PAPER-APPROVED/":p.r5b_candidate?"R5B-engineering-candidate-NOT-PAPER-APPROVED/":p.r4_candidate?"R4-partial-numerical-NOT-PAPER-APPROVED/":"R2-legacy-source-adapter-NOT-PAPER-APPROVED/")+model+"/"+p.circuit_id;s.input.master=master;
    for(std::size_t j=0;j<p.sources.size();++j){r1::require(children[j].spec==p.sources[j].spec,"model child identity");s.input.children.push_back({children[j],p.sources[j].spans});}
    for(std::size_t i=0;i<x.size();++i)s.input.public_values.push_back({i,x[i]});
    for(std::size_t i=0;i<y.size();++i)s.input.public_values.push_back({p.output[i],y[i]});
    for(auto index:p.constants)s.input.public_values.push_back({index,Fr(1)});
    for(auto index:p.reserved_zero)s.input.public_values.push_back({index,Fr(0)});
    s.ranges=p.ranges;s.exps=p.exps;s.validate();return s;
}
void requireModelQueryStatement(const ModelQueryPlan &p,const r1::LookupBatchSpec &actual,
    const r1::Commitment &approved_w,const std::vector<Fr> &x,const std::vector<Fr> &y,const std::string &model){
    std::vector<r1::Commitment> children;for(const auto &c:actual.input.children)children.push_back(c.commitment);
    auto expected=makeModelQueryStatement(p,actual.input.master,children,approved_w,x,y,model);
    r1::require(r1::serializeLookup(expected)==r1::serializeLookup(actual),"model query statement differs from verifier-owned layout");
}
#ifndef R1_VERIFY_ONLY
std::vector<std::vector<Fr>> gatherModelQueries(const ModelQueryPlan &p,const std::vector<Fr> &w){
    r1::require(w.size()==p.master.padded,"model master witness shape");std::vector<std::vector<Fr>> children;
    for(const auto &source:p.sources){std::vector<Fr> q(source.spec.padded,Fr(0));std::size_t i=0;for(const auto &span:source.spans)for(std::size_t j=0;j<span.count;++j)q[i++]=w[span.offset+j]+span.bias;children.push_back(std::move(q));}
    return children;
}
#endif
}
