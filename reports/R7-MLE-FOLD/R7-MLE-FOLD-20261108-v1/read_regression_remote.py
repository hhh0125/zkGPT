from pathlib import Path
import subprocess,shlex,shutil,json,os
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(R);S=R/'reports/R7-MLE-FOLD-20261108-v1';B=R/'build/release/r7-mle-fold-20261108-v1'
source=S/'read_amplification_regression.cpp'
source.write_text(r'''#include "unified_input.hpp"
#include <iostream>
using namespace zkgpt::r7;using namespace oneblock;namespace ui=oneblock::unified;
int main(){try{r1::init();std::vector<Fr>v(64,Fr(7));std::uint64_t reads=0;
 ui::Inputs input=[&](std::size_t){ui::InputSpan s;s.stored=s.padded=64;s.read=[&](std::size_t j){++reads;return v.at(j);};return s;};
 ui::Factors factors=[&](std::size_t,const ui::Consume&consume){consume([&](std::size_t j){return v[j];},[](std::size_t){return Fr(1);});};
 ui::UOptions options;options.threads=1;ui::quadraticRoundStreaming(64,{{0,64}},{},factors,input,options);
 need(reads==64,"instrumentation amplified original reads");std::cout<<"READ_COUNT_PASS "<<reads<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
''')
# Link the new regression against the already-built, bad instrumented archive;
# do not restore/rebuild any source or touch historical binaries.
unit=B/'src/paper_r7/CMakeFiles/r7_category_mle_fold_tests.dir'
flags={}
for line in (unit/'flags.make').read_text().splitlines():
 if ' = ' in line:
  k,v=line.split(' = ',1);flags[k]=v
obj=S/'read-regression.o'
args=['c++']+shlex.split(flags.get('CXX_DEFINES',''))+shlex.split(flags['CXX_INCLUDES'])+shlex.split(flags['CXX_FLAGS'])+['-c',str(source),'-o',str(obj)]
p=subprocess.run(args,capture_output=True,text=True);(S/'read-regression-compile.log').write_text(p.stdout+p.stderr);assert p.returncode==0
link=shlex.split((unit/'link.txt').read_text());at=next(i for i,v in enumerate(link) if v.endswith('category_mle_fold_tests.cpp.o'));link[at]=str(obj);out=S/'read-regression-RED';link[link.index('-o')+1]=str(out)
p=subprocess.run(link,cwd=B/'src/paper_r7',capture_output=True,text=True);assert p.returncode==0,p.stderr
p=subprocess.run([out],capture_output=True,text=True);(S/'read-amplification-RED.log').write_text(p.stdout+p.stderr);assert p.returncode!=0 and 'amplified' in p.stderr
# The full randomized test pins exact callback read counts in every round too.
p=R/'src/paper_r7/category_mle_fold_tests.cpp';text=p.read_text().replace('ui::Inputs inputs=[&]', 'std::uint64_t read_count=0;ui::Inputs inputs=[&]').replace('return original[z].at(j);','++read_count;return original[z].at(j);').replace('auto got=ui::quadraticRoundStreaming(', 'auto before_reads=read_count;auto got=ui::quadraticRoundStreaming(').replace('need(got.coefficients==want,', 'need(read_count-before_reads==plan.physical_values,"callback source read amplification");\n   need(got.coefficients==want,');p.write_text(text)
(S/'READ_REGRESSION_LINK.json').write_text(json.dumps({'link':link,'flags':flags,'expected_RED':True},indent=2))
print('read-amplification RED reproduced against pre-fix library')
