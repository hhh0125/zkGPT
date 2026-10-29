import subprocess
stage=Path('reports/R7-FULL12-UNRESTRICTED-20261029-v1')
Path('src/paper_r7/full12_anchor_tests.cpp').write_text(r'''#include "full12_model.hpp"
#include "master_layout.hpp"
#include "paper_r1/wire.hpp"
#include <iostream>
int main(){try{using namespace zkgpt::r7;zkgpt::r1::init();auto c=master3::trustedContext(12);auto t=full12::loadTrust(c);crypto::need(t.chunks.size()==10,"all Full12 model chunks");std::size_t n=0;for(auto&a:t.chunks){crypto::need(a.anchor.w.spec.layout==zkgpt::r1::Layout::RowMajor && a.anchor.w.spec.padded<=(1u<<24),"bounded RowMajor chunk");n+=a.anchor.w.spec.actual;}crypto::need(n==163749888,"all actual W coordinates covered");std::cout<<"FULL12_TRUST_TESTED chunks=10 W=163749888 pin="<<full12::anchorPin<<'\n';return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
''')
p=Path('src/paper_r7/CMakeLists.txt');p.write_text(p.read_text()+'\nadd_executable(r7_full12_anchor_tests full12_anchor_tests.cpp)\ntarget_link_libraries(r7_full12_anchor_tests PRIVATE paper_r7)\n')
subprocess.run(['cmake','-S','.','-B','build/release'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)
with (stage/'anchor-red-build.log').open('xb') as out:r=subprocess.run(['cmake','--build','build/release','--target','r7_full12_anchor_tests','-j4'],stdout=out,stderr=subprocess.STDOUT)
assert r.returncode!=0;print((stage/'anchor-red-build.log').read_text()[-1600:])
