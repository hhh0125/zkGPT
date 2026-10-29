#include "full12_model.hpp"
#include "master_layout.hpp"
#include "paper_r1/wire.hpp"
#include <iostream>
int main(){try{using namespace zkgpt::r7;zkgpt::r1::init();auto c=master3::trustedContext(12);auto t=full12::loadTrust(c);crypto::need(t.chunks.size()==10,"all Full12 model chunks");std::size_t n=0;for(auto&a:t.chunks){crypto::need(a.anchor.w.spec.layout==zkgpt::r1::Layout::RowMajor && a.anchor.w.spec.padded<=(1u<<24),"bounded RowMajor chunk");n+=a.anchor.w.spec.actual;}crypto::need(n==163749888,"all actual W coordinates covered");std::cout<<"FULL12_TRUST_TESTED chunks=10 W=163749888 pin="<<full12::anchorPin<<'\n';return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
