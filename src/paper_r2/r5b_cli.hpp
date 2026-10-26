#pragma once
#include "r5b_trust.hpp"
#include "model_resources.hpp"
#include <filesystem>
namespace zkgpt::r5b {
struct Options {std::string anchor,sha,manifest;bool hostile=false,witness_only=false;std::size_t budget=64;};
inline Options options(int argc,char**argv,int start,bool prover){
 Options o;for(int i=start;i<argc;++i){std::string flag=argv[i];auto value=[&](){r1::require(i+1<argc,"missing R5B option value");return std::string(argv[++i]);};
  if(flag=="--trusted-anchor")o.anchor=value();else if(flag=="--anchor-sha256")o.sha=value();else if(flag=="--model-manifest")o.manifest=value();
  else if(flag=="--blocks")r1::require(value()=="1","R5B candidate blocks=1 only; full12 not authorized");
  else if(flag=="--numerical-profile")r1::require(value()==kProfile,"unsupported R5B numerical profile");
  else if(prover && flag=="--memory-gib")o.budget=r2::parseModelBudgetGiB(value());
  else if(prover && flag=="--hostile-range")o.hostile=true;
  else if(prover && flag=="--witness-only")o.witness_only=true;
  else throw std::invalid_argument("unknown R5B option: "+flag);
 }
 if(!o.witness_only)r1::require(!o.anchor.empty() && !o.sha.empty() && !o.manifest.empty(),"R5B requires an independently pinned operator trust anchor and model manifest");
 return o;
}
inline void newDirectory(const std::filesystem::path &path){
 auto root=std::filesystem::canonical(std::filesystem::current_path()),target=std::filesystem::weakly_canonical(std::filesystem::absolute(path));
 auto relative=target.lexically_relative(root);r1::require(!relative.empty() && *relative.begin()!=".." && !std::filesystem::exists(target),"R5B output must be a new directory inside the current workspace");std::filesystem::create_directories(target);
}
}
