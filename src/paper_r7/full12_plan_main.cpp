#include "oneblock_plan.hpp"
#include "classified_layout.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
int main(int argc,char**argv){try{
 using namespace zkgpt::r7;zkgpt::r1::init();crypto::need(argc==2,"usage: r7_full12_plan NEW_REPORT_DIRECTORY");
 const auto root=std::filesystem::canonical(std::filesystem::current_path()),dest=std::filesystem::weakly_canonical(std::filesystem::absolute(argv[1]));auto rel=dest.lexically_relative(root);
 crypto::need(!rel.empty() && *rel.begin()!=".." && !std::filesystem::exists(dest),"new in-workspace directory required");
 auto p=oneblock::discover(12);std::filesystem::create_directories(dest);
 std::ofstream native(dest/"R6_NATIVE_PLAN.json"),coverage(dest/"R7_FULL12_COVERAGE.json");
 native<<p.native_json;oneblock::writeCoverage(coverage,p);native.close();coverage.close();auto layout=oneblock::classified::makeLayout(p.context);std::ofstream partitions(dest/"R7_FULL12_PARTITIONS.json");oneblock::classified::writeLayout(partitions,layout);partitions.close();crypto::need(!partitions.fail(),"partition output failed");crypto::need(!native.fail() && !coverage.fail(),"Full12 plan output failed");
 std::cout<<"FULL12_NATIVE_PLAN_DISCOVERED relations="<<p.required.size()<<" FC="<<p.fc_layers<<" objects="<<p.objects.size()<<" terminals="<<p.native_terminals<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"INCOMPLETE "<<e.what()<<'\n';return 2;}}
