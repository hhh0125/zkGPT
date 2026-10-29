def edit(name,old,new):
 p=Path('src/paper_r7')/name;s=p.read_text();assert old in s,(name,old);p.write_text(s.replace(old,new))
edit('oneblock_plan.hpp','std::string native_json;','std::vector<std::string>model_regions;std::string native_json;')
edit('oneblock_plan.cpp','for(const auto&r:nn.parameterRegions())add(p,"model/region/"+r.name,"MODEL_PARAMETER_LINK",object(q.sources.back().spec),r.count);','for(const auto&r:nn.parameterRegions()){p.model_regions.push_back(r.name);add(p,"model/region/"+r.name,"MODEL_PARAMETER_LINK",object(q.sources.back().spec),r.count);}\n if(blocks==12)for(auto&e:nn.blockConnections())if(e.block)add(p,"full12/block/"+std::to_string(e.block+1)+"/input","CROSS_BLOCK_STATE","native-master/"+std::to_string(e.input),32768);')
edit('oneblock_main.cpp','#include "fs_io.hpp"','#include "fs_io.hpp"\n#ifdef R7_FS_FULL12\n#include "full12_model.hpp"\n#include "execution_target.hpp"\n#endif')
edit('oneblock_main.cpp','constexpr auto liveSuite=zkgpt::r7::fs::suite;','#ifdef R7_FS_FULL12\nconstexpr auto liveSuite=zkgpt::r7::fs::full12Suite;\n#else\nconstexpr auto liveSuite=zkgpt::r7::fs::suite;\n#endif')
edit('oneblock_main.cpp','alarm(21600);r1::init();','#ifndef R7_FS_FULL12\n alarm(21600);\n#endif\n r1::init();')
edit('oneblock_main.cpp','auto plan=discover();auto&ctx=plan.context;','#ifdef R7_FS_FULL12\n auto plan=discover(12);\n#else\n auto plan=discover();\n#endif\n auto&ctx=plan.context;')
edit('oneblock_main.cpp','auto fsTrust=[](){master3::QuietStdout quiet;return model::loadTrust();}();','#ifdef R7_FS_FULL12\n auto fsTrust=full12::loadTrust(ctx);\n#else\n auto fsTrust=[](){master3::QuietStdout quiet;return model::loadTrust();}();\n#endif')
edit('oneblock_main.cpp','instance.text(model::trustIdentity(fsTrust));','\n#ifdef R7_FS_FULL12\n instance.text(full12::trustIdentity(fsTrust));\n#else\n instance.text(model::trustIdentity(fsTrust));\n#endif\n')
edit('oneblock_main.cpp','fs::Binding fsBinding{"Oneblock",argv[3],fs::generatorIdentity(r1::generators(fsTrust.anchor.w.spec).g),instance.b};','#ifdef R7_FS_FULL12\n std::vector<G1>extraGenerators;for(auto&t:fsTrust.chunks){auto&g=r1::generators(t.anchor.w.spec).g;extraGenerators.insert(extraGenerators.end(),g.begin(),g.end());}\n fs::Binding fsBinding{"Full12",argv[3],fs::generatorIdentity(extraGenerators),instance.b};\n#else\n fs::Binding fsBinding{"Oneblock",argv[3],fs::generatorIdentity(r1::generators(fsTrust.anchor.w.spec).g),instance.b};\n#endif')
# Preprocessor directives must start a new logical line in the compact entrypoint.
p=Path('src/paper_r7/oneblock_main.cpp');s=p.read_text().replace(';#ifndef',';\n#ifndef').replace(';#ifdef',';\n#ifdef');p.write_text(s)
edit('classified_controller.inc','std::optional<model::Secret>modelSecret;R7WitnessStore witness;Value master;','#ifdef R7_FS_FULL12\n std::vector<model::Secret>modelSecrets;\n#else\n std::optional<model::Secret>modelSecret;\n#endif\n R7WitnessStore witness;Value master;')
old='{master3::QuietStdout quiet;witness.generateR6(ctx,"reports/R6A/oneblock-model-manifest-v1.txt");modelSecret=model::prepare(trust,io.session(),random);}'
new='''{master3::QuietStdout quiet;
#ifdef R7_FS_FULL12
 witness.generateR6(ctx,executionTarget(12).manifest_path);modelSecrets=full12::prepare(trust,ctx,masterValue(witness),io.session(),random);
#else
 witness.generateR6(ctx,"reports/R6A/oneblock-model-manifest-v1.txt");modelSecret=model::prepare(trust,io.session(),random);
#endif
 }'''
edit('classified_controller.inc',old,new)
edit('classified_controller.inc','network.emplace(1,12,64,768,3072);','network.emplace(ctx.plan.shape.layer_count,12,64,768,3072);')
edit('classified_controller.inc','commitments.emplace(io,layout,master,isP?&prepared:nullptr,rng,&trust,isP?&*modelSecret:nullptr);','\n#ifdef R7_FS_FULL12\n commitments.emplace(io,layout,master,isP?&prepared:nullptr,rng,nullptr,nullptr,&trust.chunks,isP?&modelSecrets:nullptr);\n#else\n commitments.emplace(io,layout,master,isP?&prepared:nullptr,rng,&trust,isP?&*modelSecret:nullptr);\n#endif\n')
edit('classified_controller.inc','if(isP && attack=="wrong-model-witness")modelSecret->values.at(0)+=Fr(1);','#ifdef R7_FS_FULL12\n if(isP && attack=="wrong-model-witness")modelSecrets.front().values.at(0)+=Fr(1);\n#else\n if(isP && attack=="wrong-model-witness")modelSecret->values.at(0)+=Fr(1);\n#endif')
edit('classified_controller.inc','for(auto&r:trust.regions)checked(io,"model/region/"+r.name);','for(auto&name:plan.model_regions)checked(io,"model/region/"+name);for(auto&r:plan.required)if(r.family=="CROSS_BLOCK_STATE")checked(io,r.id);')
Path('src/paper_r7/full12_statement.cpp').write_text(r'''#include "oneblock_plan.hpp"
#include "wire.hpp"
#include "paper_r1/wire.hpp"
#include <filesystem>
#include <cstring>
#include <iostream>
int main(int argc,char**argv){try{using namespace zkgpt::r7;zkgpt::r1::init();crypto::need(argc==2 && !std::filesystem::exists(argv[1]),"usage: full12_statement NEW_PUBLIC_STATEMENT");auto c=master3::trustedContext(12);auto read=[](const char*name){auto b=zkgpt::r1::readFile(std::string("reports/R6-FULL12-P0AB-20261021/D-EXECUTION-20261022/")+name);crypto::need(b.size()==262144,"Full12 public X/Y padded shape");std::vector<Fr>v;v.reserve(32768);for(std::size_t i=0;i<32768;++i){std::uint64_t bits=0;for(unsigned j=0;j<8;++j)bits|=std::uint64_t(b[8*i+j])<<(8*j);std::int64_t value;std::memcpy(&value,&bits,8);v.emplace_back(value);}return v;};auto s=makeStatement(c,read("FULL12_PUBLIC_X_SIGNED64_LE.bin"),read("FULL12_FINAL_BLOCK_Y_SIGNED64_LE.bin"));crypto::need(validateStatement(s,c).code==Code::Ok,"Full12 public statement native binding");auto b=R7Wire::encodeStatement(s);zkgpt::r1::writeFile(argv[1],b);std::cout<<"FULL12_PUBLIC_STATEMENT_CREATED sha256="<<zkgpt::r5b::digestHex(b)<<" bytes="<<b.size()<<'\n';return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 2;}}
''')
p=Path('src/paper_r7/CMakeLists.txt');p.write_text(p.read_text()+'''
add_executable(r7_fs_full12_prover oneblock_main.cpp)
target_link_libraries(r7_fs_full12_prover PRIVATE paper_r7)
target_compile_definitions(r7_fs_full12_prover PRIVATE R7_CLASSIFIED_V2=1 R7_FS_ONEBLOCK=1 R7_FS_FULL12=1)
add_executable(r7_fs_full12_verifier oneblock_main.cpp)
target_link_libraries(r7_fs_full12_verifier PRIVATE paper_r7)
target_compile_definitions(r7_fs_full12_verifier PRIVATE R7_CLASSIFIED_V2=1 R7_FS_ONEBLOCK=1 R7_FS_FULL12=1 R7_COMPLETE_VERIFIER=1)
add_executable(r7_full12_statement full12_statement.cpp)
target_link_libraries(r7_full12_statement PRIVATE paper_r7)
add_test(NAME r7_full12_partition_crypto COMMAND r7_full12_runtime_tests)
add_test(NAME r7_full12_fs_framing COMMAND r7_full12_fs_tests)
set_tests_properties(r7_full12_partition_crypto r7_full12_fs_framing PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} ENVIRONMENT "ZKGPT_THREADS=4;MALLOC_ARENA_MAX=2")
''')
print('UNIFIED_FULL12_FS_ENTRYPOINT_CONNECTED_ALL_NATIVE_RELATIONS_RETAINED')
