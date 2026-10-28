#include "framework.hpp"
#include "wire.hpp"
#include "master_layout.hpp"
#include "paper_r2/r5b_trust.hpp"
#include "paper_r1/wire.hpp"
#include <filesystem>
#include <iostream>
using namespace zkgpt::r7;
namespace r1=zkgpt::r1;
int main(int argc,char**argv){try{
 r1::init();r1::require(argc==3 && !std::filesystem::exists(argv[2]),"statement migration requires old public statement and NEW output");
 auto old=R7Wire::decodeStatement(r1::readFile(argv[1]));auto ctx=master3::trustedContext();
 r1::require(old.circuit_id==ctx.plan.circuit_id && old.profile_sha256==zkgpt::r5b::profileHash() && old.model_manifest_sha256==ctx.model_manifest_sha256 && old.master_actual==ctx.plan.master.actual && old.master_padded==ctx.plan.master.padded,"historical public statement model mismatch");
 auto now=makeStatement(ctx,old.x,old.y);r1::require(now.x==old.x && now.y==old.y && validateStatement(now,ctx).code==Code::Ok,"public X/Y changed");r1::writeFile(argv[2],R7Wire::encodeStatement(now));
 std::cout<<"PASS new metadata with identical public X/Y; circuit "<<ctx.plan.circuit_id<<" native_layout "<<ctx.layout_sha256<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
