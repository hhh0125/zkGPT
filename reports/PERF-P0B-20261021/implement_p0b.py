from pathlib import Path
import os,subprocess,hashlib,json
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(r);os.sched_setaffinity(0,[0,1,2,3]);d=r/'reports/PERF-P0B-20261021';logs=r/'logs/PERF-P0B-20261021'
assert 'MATH_PASS' in (logs/'07-red-test.log').read_text()
assert json.loads((d/'04-p0a-kernel-baseline.json').read_text())['PASS']
p=Path('src/paper_r1/mapped_input.cpp');s=p.read_text();assert s==(d/'mapped_input.p0a.cpp').read_text()
s=s.replace('#include "mapped_input.hpp"','#include "mapped_input.hpp"\n#include <algorithm>',1)
s=s.replace('Fr spanKernel(', 'Fr spanKernelDP(',1)
fragment=(d/'kernel_optimization.fragment.cpp').read_text();marker='struct Batch {';assert marker in s;s=s.replace(marker,fragment+marker,1)
old='''    Fr out=0;
    for (std::size_t j=0;j<s.children.size();++j) {
        std::size_t i=0;Fr v=0;
        for (const auto &span:s.children[j].spans) {
            v+=spanKernel(b.points[j],i,r,span.offset,span.count);
            i+=span.count;
        }
        out+=b.lambda[j]*v;
    }
    for (std::size_t j=0;j<s.public_values.size();++j)
        out+=b.public_lambda[j]*eqIndex(r,s.public_values[j].index);
    for(std::size_t j=0;j<checked.size();++j){
        std::size_t i=0;Fr v=0;for(const auto &span:checked[j].spans){v+=spanKernel(checked[j].point,i,r,span.offset,span.count);i+=span.count;}
        out+=b.checked_lambda[j]*v;
    }'''
new='''    Fr out=0;
    const EqWeights master(r); // One immutable point snapshot for this invocation.
    for (std::size_t j=0;j<s.children.size();++j) {
        const SpanEvaluator evaluator(b.points[j],master);
        std::size_t i=0;Fr v=0;
        for (const auto &span:s.children[j].spans) {
            v+=evaluator.value(i,span.offset,span.count);
            i+=span.count;
        }
        out+=b.lambda[j]*v;
    }
    for (std::size_t j=0;j<s.public_values.size();++j)
        out+=b.public_lambda[j]*master.at(s.public_values[j].index);
    for(std::size_t j=0;j<checked.size();++j){
        const SpanEvaluator evaluator(checked[j].point,master);
        std::size_t i=0;Fr v=0;for(const auto &span:checked[j].spans){v+=evaluator.value(i,span.offset,span.count);i+=span.count;}
        out+=b.checked_lambda[j]*v;
    }'''
assert old in s;s=s.replace(old,new,1);p.write_text(s)
def extract(text,signature):
 begin=text.index(signature);at=text.index('{',begin);depth=1;end=at+1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[at:end]
dp=extract((d/'mapped_input.p0a.cpp').read_text(),'Fr spanKernel(');assert dp==extract(s,'Fr spanKernelDP(')
(d/'DP_BODY_UNCHANGED.json').write_text(json.dumps({'body_byte_identical':True,'sha256':hashlib.sha256(dp.encode()).hexdigest()},indent=2))
for name,args in [('08-green-build',['cmake','--build','build/release','--target','p0b_kernel_tests','-j4']),('09-green-test',['build/release/src/paper_r1/p0b_kernel_tests'])]:
 result=subprocess.run(args,capture_output=True,text=True,timeout=180);(logs/(name+'.log')).write_text(result.stdout+result.stderr);print(name,'exit',result.returncode);print((result.stdout+result.stderr)[-2000:],flush=True);assert result.returncode==0,name
with (d/'progress.md').open('a') as f:f.write('Task2 GREEN: scopedpoint-copy/split tables,singleton/direct/dyadic<=32blockpaths,frozenDPbodybyte-identicalfallback; all differential math anddispatch counters pass. No protocol/claim-order/serialization/G1/Profile changes, no realoneblock.\n')
