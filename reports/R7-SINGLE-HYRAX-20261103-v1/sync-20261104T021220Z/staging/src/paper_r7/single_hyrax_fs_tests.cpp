#include "fs_io.hpp"
#include <fstream>
#include <iostream>
#include <unistd.h>
using namespace zkgpt::r7::crypto;
namespace fs=zkgpt::r7::fs;
namespace {
template<class F>void rejects(F&&f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}need(failed,"single FS verifier accepted legacy/corrupt proof");}
void consume(const std::string&name,const fs::Binding&binding){fs::FsVerifierIO v(name,binding);need(v.message("row-chunk")==Bytes({7,8}),"single FS row chunk");auto c=v.coin("opening-challenge");need(v.message("response")==r1::encode(c*Fr(11)),"single FS recomputed challenge");v.finish();}
void write(const std::string&name,const Bytes&b){std::ofstream out(name,std::ios::binary);out.write(reinterpret_cast<const char*>(b.data()),b.size());need(bool(out),"single FS fixture write");}
}
int main(){try{r1::init();char pattern[]="build/single-hyrax-fs-XXXXXX";auto dir=mkdtemp(pattern);need(dir,"single FS temp directory");
 for(bool full:{false,true}){fs::Binding binding{full?"Full12SingleHyrax":"OneblockSingleHyrax","single-hyrax-fixture",std::string(64,'a'),{1,2,3}};auto name=std::string(dir)+(full?"/full12":"/oneblock");
  {fs::FsProverIO p(name,binding);p.message("row-chunk",{7,8});auto c=p.coin("opening-challenge");p.message("response",r1::encode(c*Fr(11)));p.finish();}consume(name,binding);
  for(auto type:{full?"Full12":"Oneblock",full?"Full12Unified":"OneblockUnified",full?"OneblockSingleHyrax":"Full12SingleHyrax"}){auto old=binding;old.proof_type=type;rejects([&]{consume(name,old);});auto legacy=name+"."+type;{fs::FsProverIO p(legacy,old);p.message("row-chunk",{7,8});auto c=p.coin("opening-challenge");p.message("response",r1::encode(c*Fr(11)));p.finish();}rejects([&]{consume(legacy,binding);});}
  auto wrong=binding;wrong.public_instance[0]^=1;rejects([&]{consume(name,wrong);});wrong=binding;wrong.context+="/wrong";rejects([&]{consume(name,wrong);});
  std::ifstream in(name,std::ios::binary);Bytes bytes{std::istreambuf_iterator<char>(in),{}};auto bad=bytes;bad.pop_back();write(name+".short",bad);rejects([&]{consume(name+".short",binding);});bad=bytes;bad.push_back(0);write(name+".trailing",bad);rejects([&]{consume(name+".trailing",binding);});
  rejects([&]{fs::FsVerifierIO v(name,binding);v.message("response");});
 }
 std::cout<<"SINGLE_HYRAX_FS_TESTED\n";return 0;
}catch(const std::exception&e){std::cerr<<"SINGLE_HYRAX_FS_FAILED "<<e.what()<<'\n';return 1;}}
