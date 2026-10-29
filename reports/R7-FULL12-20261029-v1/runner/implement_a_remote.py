def edit(name,old,new):
    p=Path('src/paper_r7')/name;s=p.read_text();assert old in s,(name,old);p.write_text(s.replace(old,new))
Path('src/paper_r7/execution_target.hpp').write_text(r'''#pragma once
#include "framework.hpp"
namespace zkgpt::r7 {
struct ExecutionTarget {
 unsigned blocks;
 const char* manifest_path;
 const char* manifest_pin;
 const char* circuit;
};
inline ExecutionTarget executionTarget(unsigned blocks){
 r1::require(blocks==1 || blocks==12,"R7 supports only native R6 oneblock or Full12");
 if(blocks==12)return {12,"reports/R6A/full12-model-manifest-v1.txt","b9a948a34b7f68c2efc0c6a36a3353f918380c0556e259e9f9c0b3d7604ba5c1","ac9d0926a13049f10e1a252642508519c13ecef34aa39e42d1813e1cd30b22c7"};
 return {1,"reports/R6A/oneblock-model-manifest-v1.txt","3ab43263570f08a2fe314bca99f2f95c11020abae17eca587f1717f52d298073","1cf3bc83a9a9f289e540238255d467b7fa6e2801209aa00f07fe232c48635a9a"};
}
}
''')
edit('r6_adapter.hpp','const std::string& independently_pinned_manifest_sha256);','const std::string& independently_pinned_manifest_sha256,unsigned blocks=1);')
edit('r6_adapter.cpp','#include "r6_adapter.hpp"','#include "r6_adapter.hpp"\n#include "execution_target.hpp"')
edit('r6_adapter.cpp','R7Context buildR6Context(const std::string&pin){','R7Context buildR6Context(const std::string&pin,unsigned blocks){\n const auto target=executionTarget(blocks);')
edit('r6_adapter.cpp','LLM nn(1,12,64,768,3072);','LLM nn(blocks,12,64,768,3072);')
edit('r6_adapter.cpp','c.plan.circuit_id=="1cf3bc83a9a9f289e540238255d467b7fa6e2801209aa00f07fe232c48635a9a","frozen oneblock circuit changed"','c.plan.circuit_id==target.circuit,"frozen native R6 target circuit changed"')
edit('r6_adapter.cpp','layout("zkGPT.R7.R6-layout.split-aux.rowmajor.v2")','layout(blocks==1?"zkGPT.R7.R6-layout.split-aux.rowmajor.v2":"zkGPT.R7.R6-full12-layout.split-aux.rowmajor.v3")')
edit('master_layout.hpp','R7Context trustedContext();','R7Context trustedContext(unsigned blocks=1);')
edit('master_layout.cpp','#include "master_layout.hpp"','#include "master_layout.hpp"\n#include "execution_target.hpp"')
edit('master_layout.cpp','R7Context trustedContext(){QuietStdout quiet;return buildR6Context(stage2::manifestPin);}','R7Context trustedContext(unsigned blocks){const auto target=executionTarget(blocks);QuietStdout quiet;return buildR6Context(target.manifest_pin,blocks);}')
edit('oneblock_plan.hpp','Coverage discover();','Coverage discover(unsigned blocks=1);')
edit('oneblock_plan.cpp','Coverage discover(){','Coverage discover(unsigned blocks){')
edit('oneblock_plan.cpp','master3::trustedContext();master3::QuietStdout quiet;LLM nn(1,12,64,768,3072);','master3::trustedContext(blocks);master3::QuietStdout quiet;LLM nn(blocks,12,64,768,3072);')
edit('oneblock_plan.cpp','r1::Fs f("R7-ONEBLOCK-NATIVE-COVERAGE-ROWMAJOR-v2")','r1::Fs f(p.context.plan.shape.layer_count==1?"R7-ONEBLOCK-NATIVE-COVERAGE-ROWMAJOR-v2":"R7-FULL12-NATIVE-COVERAGE-ROWMAJOR-v3")')
edit('oneblock_plan.cpp','o<<"{\\\"schema\\\":\\\"R7-ONEBLOCK-NATIVE-COVERAGE-ROWMAJOR-v2\\\",\\\"coverage_sha256\\\":"','o<<"{\\\"schema\\\":"<<quoted(p.context.plan.shape.layer_count==1?"R7-ONEBLOCK-NATIVE-COVERAGE-ROWMAJOR-v2":"R7-FULL12-NATIVE-COVERAGE-ROWMAJOR-v3")<<",\\\"coverage_sha256\\\":"')
edit('framework.cpp','R7Statement s;s.profile_sha256=','R7Statement s;s.blocks=c.plan.shape.layer_count;if(s.blocks==12){s.layout_id="r7-r6-full12-base-split-aux-rowmajor-v3";s.commitment_suite="r7-full12-hiding-pc-rowmajor-v3";}s.profile_sha256=')
# Avoid overwriting the target-specific layout with the historical default.
edit('framework.cpp','s.layout_id="r7-r6-base-split-aux-rowmajor-v2";s.layout_sha256=','if(s.blocks==1)s.layout_id="r7-r6-base-split-aux-rowmajor-v2";s.layout_sha256=')
edit('framework.cpp','if(s.version!=1 || s.blocks!=1)return bad("R7 version/oneblock required");','if(s.version!=1 || (s.blocks!=1 && s.blocks!=12) || s.blocks!=c.plan.shape.layer_count)return bad("R7 version/native target mismatch");')
edit('framework.cpp','c.plan.shape.layer_count!=1 ||','(c.plan.shape.layer_count!=1 && c.plan.shape.layer_count!=12) ||')
edit('framework.cpp','s.layout_id!="r7-r6-base-split-aux-rowmajor-v2"','s.layout_id!=(s.blocks==1?"r7-r6-base-split-aux-rowmajor-v2":"r7-r6-full12-base-split-aux-rowmajor-v3")')
edit('framework.cpp','s.commitment_suite!="r7-hiding-pc-rowmajor-v2"','s.commitment_suite!=(s.blocks==1?"r7-hiding-pc-rowmajor-v2":"r7-full12-hiding-pc-rowmajor-v3")')
Path('src/paper_r7/full12_plan_main.cpp').write_text(r'''#include "oneblock_plan.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
int main(int argc,char**argv){try{
 using namespace zkgpt::r7;zkgpt::r1::init();crypto::need(argc==2,"usage: r7_full12_plan NEW_REPORT_DIRECTORY");
 const auto root=std::filesystem::canonical(std::filesystem::current_path()),dest=std::filesystem::weakly_canonical(std::filesystem::absolute(argv[1]));auto rel=dest.lexically_relative(root);
 crypto::need(!rel.empty() && *rel.begin()!=".." && !std::filesystem::exists(dest),"new in-workspace directory required");
 auto p=oneblock::discover(12);std::filesystem::create_directories(dest);
 std::ofstream native(dest/"R6_NATIVE_PLAN.json"),coverage(dest/"R7_FULL12_COVERAGE.json");
 native<<p.native_json;oneblock::writeCoverage(coverage,p);native.close();coverage.close();crypto::need(!native.fail() && !coverage.fail(),"Full12 plan output failed");
 std::cout<<"FULL12_NATIVE_PLAN_DISCOVERED relations="<<p.required.size()<<" FC="<<p.fc_layers<<" objects="<<p.objects.size()<<" terminals="<<p.native_terminals<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"INCOMPLETE "<<e.what()<<'\n';return 2;}}
''')
p=Path('src/paper_r7/CMakeLists.txt');p.write_text(p.read_text()+'\nadd_executable(r7_full12_plan full12_plan_main.cpp)\ntarget_link_libraries(r7_full12_plan PRIVATE paper_r7)\n')
print('TARGET_DRIVEN_PLAN_IMPLEMENTED')
