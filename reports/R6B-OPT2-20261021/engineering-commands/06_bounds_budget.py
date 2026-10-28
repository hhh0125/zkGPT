from pathlib import Path
import os,subprocess
r=Path.cwd();d=r/'reports/R6B-OPT2-20261021';l=r/'logs/R6B-OPT2-20261021'
# Bounds tests characterize the existing subtraction-based span protection.
test=r'''#include "mapped_input.hpp"
#include <limits>
#include <iostream>
using namespace zkgpt::r1;
template<class Fn>void rejects(Fn f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}require(bad,"mapped invalid boundary accepted");}
int main(){try{init();auto m=PCSpec::make("boundary/master",7,8,Layout::RowMajor),q=PCSpec::make("boundary/child",4,4,Layout::ColumnMajor);std::vector<Fr>w{Fr(-5),Fr(1),Fr(2),Fr(3),Fr(4),Fr(5),Fr(6),Fr(0)};std::vector<Fr>v{Fr(134),Fr(123),Fr(134),Fr(134)};
 MappedInputSpec s{"boundary",commit(m,w),{{commit(q,v),{{6,1,Fr(128)},{0,1,Fr(128)},{6,2,Fr(128)}}}}, {}};
 // Last span intentionally invalid: actual ends7, source6+2 reaches padding.
 rejects([&]{s.validate();});s.children[0].spans={{6,1,Fr(128)},{0,1,Fr(128)},{6,1,Fr(128)},{6,1,Fr(128)}};
 Fs a("bound"),b("bound");auto proof=proveMappedInput(s,w,{v},a);verifyMappedInput(s,proof,b);require(a.digest()==b.digest(),"boundary transcript");
 const auto max=std::numeric_limits<std::size_t>::max();unsigned rejected=0;
 for(auto span:std::vector<Span>{{max,1,Fr(0)},{0,max,Fr(0)},{max,max,Fr(0)},{7,1,Fr(0)},{6,2,Fr(0)},{0,0,Fr(0)}}){auto bad=s;bad.children[0].spans={span};rejects([&]{Fs t("bound");proveMappedInput(bad,w,{v},t);});++rejected;}
 for(auto wrong:std::vector<std::vector<Fr>>{{},{Fr(134)},{Fr(134),Fr(123),Fr(134)}, {Fr(134),Fr(123),Fr(134),Fr(134),Fr(0)},{Fr(0),Fr(123),Fr(134),Fr(134)}}){rejects([&]{Fs t("bound");proveMappedInput(s,w,{wrong},t);});++rejected;}
 auto bad=s;bad.children[0].spans={{0,3,Fr(128)},{1,max,Fr(128)}};rejects([&]{Fs t("bound");proveMappedInput(bad,w,{v},t);});++rejected;
 auto padded=PCSpec::make("boundary/padded",3,4,Layout::RowMajor);std::vector<Fr>vp{w[6],w[0],w[6],Fr(0)};s.children={{{commit(padded,vp)},{{6,1,Fr(0)},{0,1,Fr(0)},{6,1,Fr(0)}}}};
 {Fs x("tail"),y("tail");auto p=proveMappedInput(s,w,{vp},x);verifyMappedInput(s,p,y);}vp[3]=Fr(1);rejects([&]{Fs t("tail");proveMappedInput(s,w,{vp},t);});++rejected;
 auto shortw=w;shortw.pop_back();rejects([&]{Fs t("tail");proveMappedInput(s,shortw,{vp},t);});++rejected;
 std::cout<<"PASS "<<rejected<<" span/size/overflow negative cases; lastvalidindex,repeatednegativebias,padding included\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
'''
# Span is named GatherSpan in the actual interface; resolve from header.
header=(r/'src/paper_r1/mapped_input.hpp').read_text();print(header[:1600])
span_name='SourceSpan' if 'struct SourceSpan' in header else 'GatherSpan' if 'struct GatherSpan' in header else 'MappedSpan' if 'struct MappedSpan' in header else None
if span_name:test=test.replace('std::vector<Span>','std::vector<'+span_name+'>')
else:test=test.replace('std::vector<Span>','decltype(s.children[0].spans)')
p=r/'src/paper_r1/tests_opt2_span_bounds.cpp';assert not p.exists();p.write_text(test)
p=r/'src/paper_r1/CMakeLists.txt';(d/'before/src/paper_r1/CMakeLists.txt').write_text(p.read_text());p.open('a').write('\nadd_executable(opt2_span_bounds_tests tests_opt2_span_bounds.cpp)\ntarget_link_libraries(opt2_span_bounds_tests PRIVATE paper_r1)\nadd_test(NAME opt2_span_bounds COMMAND opt2_span_bounds_tests)\n')
# Exact size validation before any shared-mode shift/index/allocation.
p=r/'src/prover.cpp';s=p.read_text();a='    const size_t len=val[0].size();';b='''    if(shared_vector && (val.empty() || C.circuit.empty() ||
        C.circuit[0].bit_length<0 || C.circuit[0].bit_length>28))
        throw std::invalid_argument("shared commitment input domain");
    if(shared_vector && gens.size()<(std::size_t(1)<<(C.circuit[0].bit_length-C.circuit[0].bit_length/2))+1)
        throw std::invalid_argument("shared commitment generators incomplete");
'''+a;assert s.count(a)==1;s=s.replace(a,b);p.write_text(s)
p=r/'src/neuralNetwork.cpp';s=p.read_text();a='        pr.val[0].reserve(witness_capacity_hint_);';s=s.replace(a,a+'\n        if(pr.val[0].capacity()!=witness_capacity_hint_)throw std::logic_error("planned witness capacity not exact");');p.write_text(s)
for n in ['r5b_model_prove.cpp','r6_model_prove.cpp']:
 p=r/'src/paper_r2'/n;s=p.read_text();a='planned_actual=p.master.actual;';s=s.replace(a,a+'zkgpt::memory_diagnostics::logAddress("preflight/graph","object",&c,0,"scoped-preflight");zkgpt::memory_diagnostics::logAddress("preflight/graph","layers",c.circuit.data(),c.circuit.capacity()*sizeof(layer),"scoped-preflight");')
 a=' r5b::newDirectory';assert a in s;s=s.replace(a,' zkgpt::memory_diagnostics::logScalar("preflight/graph-destroyed","live_preflight_graphs",0);\n'+a,1);p.write_text(s)
# Keep generic R1 inclusive estimator semantics; correct only R6 composition.
p=r/'src/paper_r2/r6_cli.hpp';s=p.read_text();a='auto val_capacity=std::max(2*initial*sizeof(Fr),master);';b='auto val_capacity=p.master.actual*sizeof(Fr);';assert a in s;s=s.replace(a,b)
a='r.prove=shared.lookup_prover_bytes+val_capacity+layers+matrix+2*master+3*graph+(std::size_t(2)<<30);';b='''// R1 mapped bound includes immutable master, already counted below.
 auto lookup_incremental=shared.lookup_retained_bytes+std::max(shared.lookup_local_bytes,shared.lookup_mapped_bytes-master);
 // Shared cc.w/caller vector has one owner. Keep two EXTRA graph capacities
 // as named safety reserve, in addition to the one actual graph, and25%.
 r.prove=lookup_incremental+val_capacity+layers+matrix+master+3*graph+(std::size_t(2)<<30);''';assert a in s;s=s.replace(a,b);p.write_text(s)
p=r/'src/paper_r2/r6_model.cpp';s=p.read_text();a='<<res.gkr_checked_input_padded<<';b='''<<res.gkr_checked_input_padded<<",\\\"lookup_immutable_master_bytes\\\":"<<p.master.padded*sizeof(Fr)<<",\\\"lookup_mapped_incremental_bytes\\\":"<<res.lookup_mapped_bytes-p.master.padded*sizeof(Fr)<<",\\\"lookup_incremental_prover_bytes\\\":"<<res.lookup_retained_bytes+std::max(res.lookup_local_bytes,res.lookup_mapped_bytes-p.master.padded*sizeof(Fr))<<",\\\"planned_val0_capacity_bytes\\\":"<<p.master.actual*sizeof(Fr)<<",\\\"shared_cc_caller_master_bytes\\\":"<<p.master.padded*sizeof(Fr)<<",\\\"graph_additional_safety_reserve_bytes\\\":"<<2*(gate_capacity+coordinate_capacity)<<''';assert s.count(a)==1;s=s.replace(a,b);p.write_text(s)
p=r/'src/paper_r2/tests_r6_integration.cpp';s=p.read_text().replace('reports/R6B-OPT-20261021/','reports/R6B-OPT2-20261021/');p.write_text(s)
os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
with (l/'release-build.log').open('x') as f:
 subprocess.run(['cmake','-S','.','-B','build/release'],stdout=f,stderr=subprocess.STDOUT,check=True)
 subprocess.run(['cmake','--build','build/release','-j','4'],stdout=f,stderr=subprocess.STDOUT,check=True,timeout=600)
with (l/'span-boundary.log').open('x') as f:subprocess.run(['build/release/src/paper_r1/opt2_span_bounds_tests'],stdout=f,stderr=subprocess.STDOUT,check=True,timeout=120)
print((l/'span-boundary.log').read_text())
