#include "u_ram_budget.hpp"
#include "unified_input.hpp"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
namespace zkgpt::r7::oneblock::unified {
bool UMemoryRoom::fits(std::uint64_t additional)const{
 // Leave 10% of the currently available resource, and at least 2 GiB,
 // for the rest of the prover, allocator overhead and concurrent host work.
 auto fitsOne=[&](std::uint64_t available){auto reserve=std::max<std::uint64_t>(2ull<<30,available/10);return reserve<=available && additional<=available-reserve;};
 return inspected && fitsOne(system_available) && (cgroup_available==std::numeric_limits<std::uint64_t>::max() || fitsOne(cgroup_available));
}
UMemoryRoom inspectUMemory(const std::string&proc,const std::string&cgroup){
 UMemoryRoom out;
 try{
  std::ifstream info(proc+"/meminfo");std::string line;
  while(std::getline(info,line)){std::istringstream in(line);std::string key,unit;std::uint64_t kb=0;if(in>>key>>kb>>unit && key=="MemAvailable:" && unit=="kB" && kb<=std::numeric_limits<std::uint64_t>::max()/1024)out.system_available=kb*1024;}
  std::ifstream group(proc+"/self/cgroup");std::string path;
  while(std::getline(group,line))if(line.rfind("0::/",0)==0)path=line.substr(3);
  if(path.empty() || path[0]!='/')return out;
  std::filesystem::path relative=std::filesystem::path(path).relative_path();
  for(const auto&part:relative)if(part=="..")return out;
  const auto root=std::filesystem::path(cgroup).lexically_normal();auto directory=root/relative;bool observed=false;
  for(;;){
   std::ifstream limit(directory/"memory.max"),current(directory/"memory.current");std::string maximum;std::uint64_t usage=0;
   if(limit>>maximum){
    if(!(current>>usage))return out;
    observed=true;
    if(maximum!="max"){
     if(maximum.empty() || maximum.find_first_not_of("0123456789")!=std::string::npos)return out;
     auto cap=std::stoull(maximum);out.cgroup_available=std::min<std::uint64_t>(out.cgroup_available,usage<cap?cap-usage:0);
    }
   }else if(directory!=root)return out; // The cgroup2 root has no memory.max.
   if(directory==root)break;
   directory=directory.parent_path();
  }
  out.inspected=observed && out.system_available>0;
 }catch(const std::exception&){out.inspected=false;}
 return out;
}
UImmediateOptions UImmediateOptions::environment(){
 UImmediateOptions out;
 if(const auto*v=std::getenv("ZKGPT_CATEGORY_U_FOLD_MODE")){
  std::string mode(v);need(mode=="deferred" || mode=="immediate" || mode=="auto","R8 U fold mode");
  out.mode=mode=="immediate"?UFoldMode::Immediate:mode=="auto"?UFoldMode::Auto:UFoldMode::Deferred;
 }
 if(const auto*v=std::getenv("ZKGPT_CATEGORY_U_IMMEDIATE_RAM_BYTES")){
  std::string text(v);need(!text.empty() && text.find_first_not_of("0123456789")==std::string::npos,"R8 U immediate budget syntax");out.budget=std::stoull(text);
 }
 need(out.budget<=64ull<<30,"R8 U immediate budget cap");return out;
}
}
