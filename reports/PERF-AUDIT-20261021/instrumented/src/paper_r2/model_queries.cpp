#include "/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/PERF-AUDIT-20261021/audit_timer.hpp"
#include "model_queries.hpp"
#include "gkr_protocol.hpp"
#include "paper_r1/wire.hpp"
#include <map>
#include <set>
namespace zkgpt::r2 {
namespace {
std::size_t padded(std::size_t n){r1::require(n>0,"empty model query");std::size_t p=1;while(p<n){r1::require(p<r1::kMaxVector,"model query size budget");p*=2;}return p;}
bool ends(const std::string &s,const std::string &suffix){return s.size()>=suffix.size() && s.compare(s.size()-suffix.size(),suffix.size(),suffix)==0;}
void push(std::vector<r1::InputSpan> &spans,std::size_t offset,std::size_t count,const Fr &bias){
    if(!spans.empty() && spans.back().offset+spans.back().count==offset && spans.back().bias==bias)spans.back().count+=count;
    else spans.push_back({offset,count,bias});
}
std::size_t addSource(ModelQueryPlan &p,const std::string &role,const std::vector<r1::InputSpan> &spans){
    std::size_t n=0;for(const auto &s:spans){r1::require(s.offset<p.master.actual && s.count<=p.master.actual-s.offset,"model source bounds");r1::require(s.count<=r1::kMaxVector-n,"model source length overflow");n+=s.count;}
    auto index=p.sources.size();p.sources.push_back({r1::PCSpec::make(role,n,padded(n),r1::Layout::ColumnMajor),spans});return index;
}
}
ModelQueryPlan planModelQueries(const layeredCircuit &c,const WitnessRegistry &registry,
    const std::vector<neuralNetwork::ParameterRegion> &parameters,r1::ExpProfile exp_profile){ perf_audit::Scope audit_scope("model.planModelQueries"); 
    ModelQueryPlan p;p.shape=registry.shape();p.r4_candidate=registry.r4NumericalCandidate();p.r5b_candidate=registry.r5bEngineeringCandidate();p.r6_engineering=registry.r6Engineering();
    r1::require(p.shape.sequence_length==32 && p.shape.hidden_dimension==768 && p.shape.head_dimension==64 && p.shape.head_count==12,"unsupported model query profile");
    registry.validateLayout(c.circuit.at(0).size);
    p.circuit_id=circuitFingerprintHex(fingerprintCircuit(c));
    p.master=r1::PCSpec::make("r2/model/master",c.circuit[0].size,std::size_t(1)<<c.circuit[0].bit_length,r1::Layout::RowMajor);
    std::map<std::pair<unsigned,bool>,std::vector<r1::InputSpan>> groups;
    std::set<std::size_t> constants;
    for(const auto &r:registry.constraints()){
        Fr bias=0;if(r.is_signed){bias=Fr(1);for(unsigned bit=1;bit<r.bits;++bit)bias*=Fr(2);}
        push(groups[{r.bits,r.is_signed}],r.val0_offset,r.count,bias);
        if(p.r5b_candidate && r.kind==WitnessKind::LAYER_NORM && ends(r.name,"_sigma"))
            push(groups[{r.bits,false}],r.val0_offset,r.count,Fr(-1));
        if(p.r4_candidate){
            const bool ln=r.kind==WitnessKind::LAYER_NORM;
            const bool gelu=r.kind==WitnessKind::GELU;
            const bool root=ln && (ends(r.name,"_sqrt_upper") || ends(r.name,"_sqrt_lower"));
            const bool positive=root || (ln && (ends(r.name,"_sigma") || ends(r.name,"_delta1") || ends(r.name,"_term1") || ends(r.name,"_term2"))) ||
                (gelu && (ends(r.name,"_delta2") || ends(r.name,"_term1") || ends(r.name,"_term2")));
            // Additional query; the original unsigned upper bound remains above.
            if(positive)push(groups[{r.bits,false}],r.val0_offset,r.count,Fr(-1));
            // Together with the two u32 sqrt factors this bounds all square
            // expressions/products far below the field modulus (not just delta).
            if(ln && ends(r.name,"_sigma"))push(groups[{32,false}],r.val0_offset,r.count,Fr(0));
        }
        unsigned reserve=0;
        if(r.name.rfind("round_",0)==0 && ends(r.name,"_q"))reserve=20;
        if(((r.name.rfind("layer_norm_",0)==0 || r.name.rfind("gelu_",0)==0) && ends(r.name,"_y")) ||
            (r.name.rfind("softmax_",0)==0 && ends(r.name,"_sumE")))reserve=10;
        if(reserve){r1::require(r.val0_offset>=reserve,"constant offset underflow");auto at=r.val0_offset-reserve;constants.insert(at);for(unsigned j=1;j<reserve;++j)p.reserved_zero.push_back(at+j);}
    }
    p.constants.assign(constants.begin(),constants.end());
    r1::require(p.constants.size()==8*p.shape.layer_count,"model constant coverage");
    for(const auto &group:groups){auto id="r2/model/range/"+std::to_string(p.ranges.size());auto child=addSource(p,id,group.second);p.ranges.push_back({child,id,group.first.first});}
    std::vector<r1::InputSpan> addresses,values;
    for(const auto &q:registry.expQueries()){push(addresses,q.t_witness_index,1,Fr(0));push(values,q.E_witness_index,1,Fr(0));}
    auto ti=addSource(p,"r2/model/Exp-t",addresses),ei=addSource(p,"r2/model/Exp-E",values);
    p.exps.push_back({ti,ei,"r2/model/Exp",exp_profile});
    std::vector<r1::InputSpan> weights;
    for(const auto &region:parameters)push(weights,region.offset,region.count,Fr(0));
    r1::require(parameters.size()==12*p.shape.layer_count,"model parameter region coverage");
    addSource(p,"r2/model/W",weights);
    const auto &last=c.circuit.back();r1::require(last.ty==layerType::RELU && last.zero_start_id==0 && last.size==32768,"model final residual structure");
    p.output.assign(last.size,p.master.actual);
    for(const auto &gate:last.uni_gates)if(!gate.lu && gate.sc==1){
        r1::require(gate.g<p.output.size() && p.output[gate.g]==p.master.actual,"ambiguous output source");
        p.output[gate.g]=last.ori_id_u.at(gate.u);
    }
    for(auto index:p.output)r1::require(index<p.master.actual,"missing output source");
    return p;
}
r1::LookupBatchSpec makeModelQueryStatement(const ModelQueryPlan &p,const r1::Commitment &master,
    const std::vector<r1::Commitment> &children,const r1::Commitment &approved_w,
    const std::vector<Fr> &x,const std::vector<Fr> &y,const std::string &model){ perf_audit::Scope audit_scope("model.makeModelQueryStatement"); 
    r1::require(!model.empty() && model.size()<=128,"externally approved model identity required");
    r1::require(master.spec==p.master && children.size()==p.sources.size(),"model query commitment shape");
    r1::require(children.back().spec==approved_w.spec && children.back().rows==approved_w.rows,"publisher W commitment mismatch");
    r1::require(x.size()==32768 && y.size()==p.output.size(),"complete public X/Y required");
    for(std::size_t i=0;i<x.size();++i)if(i%1024>=p.shape.hidden_dimension)r1::require(x[i].isZero() && y[i].isZero(),"public activation padding must be zero");
    r1::LookupBatchSpec s;s.input.context=(p.r6_engineering?"R6-R5B-engineering-candidate-NOT-PAPER-APPROVED/":p.r5b_candidate?"R5B-engineering-candidate-NOT-PAPER-APPROVED/":p.r4_candidate?"R4-partial-numerical-NOT-PAPER-APPROVED/":"R2-legacy-source-adapter-NOT-PAPER-APPROVED/")+model+"/"+p.circuit_id;s.input.master=master;
    for(std::size_t j=0;j<p.sources.size();++j){r1::require(children[j].spec==p.sources[j].spec,"model child identity");s.input.children.push_back({children[j],p.sources[j].spans});}
    for(std::size_t i=0;i<x.size();++i)s.input.public_values.push_back({i,x[i]});
    for(std::size_t i=0;i<y.size();++i)s.input.public_values.push_back({p.output[i],y[i]});
    for(auto index:p.constants)s.input.public_values.push_back({index,Fr(1)});
    for(auto index:p.reserved_zero)s.input.public_values.push_back({index,Fr(0)});
    s.ranges=p.ranges;s.exps=p.exps;s.validate();return s;
}
void requireModelQueryStatement(const ModelQueryPlan &p,const r1::LookupBatchSpec &actual,
    const r1::Commitment &approved_w,const std::vector<Fr> &x,const std::vector<Fr> &y,const std::string &model){ perf_audit::Scope audit_scope("model.requireModelQueryStatement"); 
    std::vector<r1::Commitment> children;for(const auto &c:actual.input.children)children.push_back(c.commitment);
    auto expected=makeModelQueryStatement(p,actual.input.master,children,approved_w,x,y,model);
    r1::require(r1::serializeLookup(expected)==r1::serializeLookup(actual),"model query statement differs from verifier-owned layout");
}
#ifndef R1_VERIFY_ONLY
std::vector<std::vector<Fr>> gatherModelQueries(const ModelQueryPlan &p,const std::vector<Fr> &w){ perf_audit::Scope audit_scope("model.gatherModelQueries"); 
    r1::require(w.size()==p.master.padded,"model master witness shape");std::vector<std::vector<Fr>> children;
    for(const auto &source:p.sources){std::vector<Fr> q(source.spec.padded,Fr(0));std::size_t i=0;for(const auto &span:source.spans)for(std::size_t j=0;j<span.count;++j)q[i++]=w[span.offset+j]+span.bias;children.push_back(std::move(q));}
    return children;
}
#endif
}
