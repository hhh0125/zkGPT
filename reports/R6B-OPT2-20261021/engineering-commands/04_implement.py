from pathlib import Path
import os,subprocess
r=Path.cwd();d=r/'reports/R6B-OPT2-20261021';l=r/'logs/R6B-OPT2-20261021'
p=r/'src/prover.hpp';s=p.read_text();s=s.replace('void commitInput(const vector<G1> &gens,int thread_n=1);','void commitInput(const vector<G1> &gens,int thread_n=1,bool shared_vector=false);\n    const std::vector<Fr>& sharedInputWitness() const;');s=s.replace('    std::unique_ptr<Fr[]> committed_witness_;','    std::unique_ptr<Fr[]> committed_witness_;\n    std::vector<Fr> committed_witness_vector_;');p.write_text(s)
p=r/'src/memory_diagnostics.hpp';s=p.read_text();s=s.replace('#include <cstddef>','#include <cstddef>\n#include <cstdio>');a='bool enabled();';s=s.replace(a,a+'''
inline void logAddress(const std::string&stage,const std::string&name,const void*data,
                       std::size_t bytes,const char*owner){
 if(enabled())std::fprintf(stderr,"[ZKGPT_ADDR] stage=%s object=%s address=%p bytes=%zu owner=%s\\n",stage.c_str(),name.c_str(),const_cast<void*>(data),bytes,owner);
}
''');p.write_text(s)
p=r/'src/prover.cpp';s=p.read_text();s=s.replace('#include "host_safety.hpp"','#include "host_safety.hpp"\n#include <algorithm>');a='void prover::commitInput(const vector<G1> &gens,int thr)';assert a in s;s=s.replace(a,'void prover::commitInput(const vector<G1> &gens,int thr,bool shared_vector)');a='    if (zkgpt::memory_diagnostics::enabled()) {\n        zkgpt::memory_diagnostics::logProcess("commit_input_before_padding");';b='''    if(shared_vector){
        // Exact logical witness only. No temporary padding/compaction of val0.
        if(len!=C.circuit[0].size)throw std::length_error("shared commitment requires complete logical witness");
        std::vector<Fr> owner(padded_len,Fr(0));
        std::copy(val[0].begin(),val[0].end(),owner.begin());
        const int bits=C.circuit[0].bit_length;
        std::unique_ptr<G1[]> rows(prover_commit(owner.data(),(G1*)gens.data(),bits,thr));
        // All workers joined. Publish only after success; failure leaves the
        // previous commitment owner and borrowed views intact.
        committed_witness_vector_=std::move(owner);committed_witness_.reset();
        committed_rows_=std::move(rows);
        cc.comm=committed_rows_.get();cc.G=gens.back();cc.g=(G1*)gens.data();
        cc.l=bits;cc.w=committed_witness_vector_.data();cc.ww=nullptr;
        using namespace zkgpt::memory_diagnostics;
        logAddress("shared_commit/published","cc.w",cc.w,padded_len*sizeof(Fr),"prover.vector");
        logAddress("shared_commit/published","val0",val[0].data(),val[0].capacity()*sizeof(Fr),"prover.val0");
        logVector("shared_commit/published","committed_vector",committed_witness_vector_);
        logProcess("shared_commit/published");return;
    }
'''+a
assert s.count(a)==1;s=s.replace(a,b);a='    committed_witness_=std::move(witness_owner);';s=s.replace(a,'    std::vector<Fr>().swap(committed_witness_vector_);\n'+a)
a='__int128 convert(Fr x)';assert a in s;s=s.replace(a,'''const std::vector<Fr>& prover::sharedInputWitness() const {
    if(cc.l<0 || cc.l>28 || committed_witness_vector_.empty() ||
       committed_witness_vector_.size()!=(std::size_t(1)<<cc.l) || cc.w!=committed_witness_vector_.data())
        throw std::logic_error("no shared vector-owned commitment");
    return committed_witness_vector_;
}

'''+a);p.write_text(s)
p=r/'src/neuralNetwork.hpp';s=p.read_text();a='    void create(prover &pr, bool merge);';s=s.replace(a,'''    void setWitnessCapacityHint(std::size_t expected){
        if(!expected || expected>(std::size_t(1)<<28))throw std::invalid_argument("witness capacity hint outside supported domain");
        witness_capacity_hint_=expected;
    }
'''+a);s=s.replace('    std::vector<ParameterRegion> parameter_regions_;','    std::vector<ParameterRegion> parameter_regions_;\n    std::size_t witness_capacity_hint_=0;');p.write_text(s)
p=r/'src/neuralNetwork.cpp';s=p.read_text();a='    pr.val.resize(SIZE);\n    val = pr.val.begin();';b='''    pr.val.resize(SIZE);
    if(!structure_only && witness_capacity_hint_){
        if(!r5b_numerical_candidate_)throw std::invalid_argument("capacity hint requires engineering candidate");
        pr.val[0].reserve(witness_capacity_hint_);
        zkgpt::memory_diagnostics::logVector("val0/planned-reserve","val0",pr.val[0]);
        zkgpt::memory_diagnostics::logAddress("val0/planned-reserve","val0",pr.val[0].data(),pr.val[0].capacity()*sizeof(Fr),"prover.val0");
    }
    val = pr.val.begin();''';assert s.count(a)==1;s=s.replace(a,b)
a='    if(r6_skip_commit_){pr.mat_val=mat_values;return;}';b='''    if(!structure_only && witness_capacity_hint_ && pr.val[0].size()!=witness_capacity_hint_)
        throw std::logic_error("actual witness differs from planned capacity domain");
    zkgpt::memory_diagnostics::logAddress("private_graph/ready","graph_object",&pr.C,0,"prover.C");
    zkgpt::memory_diagnostics::logAddress("private_graph/ready","graph_layers",pr.C.circuit.data(),pr.C.circuit.capacity()*sizeof(layer),"prover.C");
'''+a;assert a in s;s=s.replace(a,b);s=s.replace('    pr.commitInput(pr.gens,32);','    pr.commitInput(pr.gens,32,witness_capacity_hint_!=0);')
a='        return std::move(public_builder.C);';s=s.replace(a,'''        zkgpt::memory_diagnostics::logAddress("public_graph/move-out","graph_layers",public_builder.C.circuit.data(),public_builder.C.circuit.capacity()*sizeof(layer),"moved-to-preflight");
'''+a);p.write_text(s)
# Existing preflight supplies exact actual count; no additional private graph.
for name,planvar,config in [('r5b_model_prove.cpp','p','pub.setR5BEngineeringCandidate()'),('r6_model_prove.cpp','p','r6::configure(pub)')]:
 p=r/'src/paper_r2'/name;s=p.read_text();s='#include "memory_diagnostics.hpp"\n'+s
 if name.startswith('r5b'):
  s=s.replace('r2::ModelResources resources;r5b::Anchor anchor;','r2::ModelResources resources;r5b::Anchor anchor;std::size_t planned_actual=0;')
  a='resources=r2::modelResources(c,p);';s=s.replace(a,'planned_actual=p.master.actual;'+a)
  s=s.replace('nn.setR5BEngineeringCandidate();prover pr;nn.create(pr,false);','nn.setR5BEngineeringCandidate();nn.setWitnessCapacityHint(planned_actual);prover pr;nn.create(pr,false);')
  a='std::vector<Fr>master(pr.cc.w,pr.cc.w+plan.master.padded);'
 else:
  s=s.replace('r5b::Anchor anchor;std::string fingerprint;r6::Resources budgets;','r5b::Anchor anchor;std::string fingerprint;r6::Resources budgets;std::size_t planned_actual=0;')
  s=s.replace('budgets=r6::resources(c,pub,p);','planned_actual=p.master.actual;budgets=r6::resources(c,pub,p);')
  s=s.replace('r6::configure(nn);prover pr{};nn.create(pr,false);','r6::configure(nn);nn.setWitnessCapacityHint(planned_actual);prover pr{};nn.create(pr,false);')
  a='std::vector<Fr>master(pr.cc.w,pr.cc.w+p.master.padded);'
 assert a in s;s=s.replace(a,'''const auto&master=pr.sharedInputWitness();
 zkgpt::memory_diagnostics::logAddress("caller/master-ref","caller_master",master.data(),master.capacity()*sizeof(Fr),"prover.vector/const-ref");
 zkgpt::memory_diagnostics::logAddress("caller/master-ref","val0",pr.val[0].data(),pr.val[0].capacity()*sizeof(Fr),"prover.val0");
 zkgpt::memory_diagnostics::logAddress("caller/master-ref","actual_graph",&pr.C,0,"prover.C");
''')
 # Public preflight graph scope has ended before this new private owner starts.
 a=' r5b::newDirectory' if name.startswith('r5b') else ' r5b::newDirectory'
 # Both entrypoints retain their existing scoped preflight structure.
 p.write_text(s)
p=r/'src/paper_r2/protocol.cpp';s=p.read_text();s='#include "memory_diagnostics.hpp"\n'+s;a='    builder.build();p.gkr=builder.proof();';b='''    r1::require(&builder.C==&pr.C,"GKR builder must borrow actual circuit");
    zkgpt::memory_diagnostics::logAddress("gkr/builder-created","builder_C_ref",&builder.C,0,"borrowed-prover.C");
    zkgpt::memory_diagnostics::logAddress("gkr/builder-created","master_readonly",w.data(),w.capacity()*sizeof(Fr),"caller-const-ref");
'''+a;assert a in s;s=s.replace(a,b);p.write_text(s)
os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
with (l/'shared-green-build.log').open('x') as f:subprocess.run(['cmake','--build','build/release','--target','opt2_ownership_tests','-j','4'],stdout=f,stderr=subprocess.STDOUT,check=True,timeout=600)
with (l/'shared-green.log').open('x') as f:subprocess.run(['build/release/src/opt2_ownership_tests'],stdout=f,stderr=subprocess.STDOUT,check=True,timeout=120,env={**os.environ,'ZKGPT_MEMORY_DIAGNOSTICS':'1'})
print((l/'shared-green.log').read_text()[-1700:])
with (d/'PROGRESS.md').open('a') as f:f.write('Sharedowner test trueRED missingAPI ->GREEN; realGKR verifies and does not mutate sharedmaster; addresses recorded. Optinvectorcommit bypasses logicalval0paddingcopy and retains legacydefaultnew[] semantics. Exactpublicplan hint reservedonce and validatedbeforecommit. Sourceonlyfixedworkspace.\n')
