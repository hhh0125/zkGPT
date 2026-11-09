#include "crypto.hpp"
#include "perf6_options.hpp"
#include <iostream>
using namespace zkgpt::r7;
int main(){try{
 zkgpt::r1::init();const std::string label(128,'x');crypto::Writer writer;
 std::uint64_t count=0;
 {perf6::ScopedMessages mode(true);crypto::AllocationTrace trace;writer.text(label);count=trace.count();}
 std::cout<<"text allocations "<<count<<'\n';
 crypto::need(count==1,"PERF6 B2 text serialization must allocate once");
 crypto::Bytes expected{128,0,0,0};expected.insert(expected.end(),128,'x');
 crypto::need(writer.b==expected,"PERF6 text canonical bytes changed");
 for(bool enabled:{false,true}){
  perf6::ScopedMessages mode(enabled);crypto::Writer w;w.text("");w.text("abc");
  crypto::need(w.b==crypto::Bytes({0,0,0,0,3,0,0,0,'a','b','c'}),"PERF6 empty/sequence bytes changed");
  bool rejected=false;try{w.text(std::string(257,'x'));}catch(const std::exception&){rejected=true;}
  crypto::need(rejected,"PERF6 oversized string accepted");
  crypto::Writer limit;limit.b.resize(4*1024*1024-3);rejected=false;try{limit.text("a");}catch(const std::exception&){rejected=true;}
  crypto::need(rejected,"PERF6 oversized wire accepted");
 }
 crypto::need(!perf6::messages,"PERF6 message option leaked outside scope");
 std::cout<<"PASS PERF6 canonical text and allocation bounds\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
