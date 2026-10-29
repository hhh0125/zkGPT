import subprocess
p=Path('src/paper_r7/partitioned_layout_tests.cpp');s=p.read_text().replace('#include <set>','#include <set>\n#include <sstream>')
needle='auto bad=p;bad.base_runs.front().start.index++;'
assert needle in s
s=s.replace(needle,'std::ostringstream rendered;writeLayout(rendered,p);need(rendered.str().find("\\\"source_spans\\\"")!=std::string::npos && rendered.str().find("\\\"signed_bias_Fr_decimal\\\":\\\"2\\\"")!=std::string::npos && rendered.str().find("\\\"kind\\\"")!=std::string::npos,"standalone partition report retains spans, affine biases and kinds");\n '+needle);p.write_text(s)
stage=Path('reports/R7-FULL12-20261029-v1')
with (stage/'partition-serialization-red-build.log').open('xb') as out:r=subprocess.run(['cmake','--build','build/release','--target','r7_partitioned_layout_tests','-j4'],stdout=out,stderr=subprocess.STDOUT)
assert r.returncode==0
with (stage/'partition-serialization-red.log').open('xb') as out:r=subprocess.run(['build/release/src/paper_r7/r7_partitioned_layout_tests'],stdout=out,stderr=subprocess.STDOUT)
assert r.returncode==1;print((stage/'partition-serialization-red.log').read_text())
