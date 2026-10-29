#include "fs_io.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
using namespace zkgpt::r7::crypto;
namespace fs=zkgpt::r7::fs;
namespace {
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}need(bad,"FS negative accepted");}
Bytes read(const std::string&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void write(const std::string&p,const Bytes&b){std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());}
void consume(const std::string&p,const fs::Binding&b){fs::FsVerifierIO v(p,b);need(v.message("first")==Bytes({7,8}),"FS first");auto c=v.coin("challenge");need(v.message("response")==r1::encode(c*Fr(7)),"FS challenge recomputation");v.finish();}
void legacySuites(const std::string&p){
 for(bool full12:{false,true}){fs::Binding b{full12?"Full12":"Oneblock","suite-transition",std::string(64,'a'),{2,7,1,8}};auto name=p+(full12?".full12":".oneblock");
  {fs::FsProverIO w(name,b);w.message("first",{7,8});auto x=w.coin("challenge");w.message("response",r1::encode(x*Fr(7)));w.finish();}consume(name,b);
  auto bytes=read(name);std::string current=full12?fs::full12Suite:fs::suite;
  const std::string legacy=full12?"R7-FULL12-CLASSIFIED-HYRAX-LOOKUP-FS-ROWMAJOR-CANDIDATE-v1":"R7-CLASSIFIED-HYRAX-LOOKUP-FS-ROWMAJOR-CANDIDATE-v2";
  need(current.size()==legacy.size(),"suite transition fixture length");auto pos=std::search(bytes.begin(),bytes.end(),current.begin(),current.end());need(pos!=bytes.end(),"current suite absent from header");std::copy(legacy.begin(),legacy.end(),pos);write(name+".legacy",bytes);
  bool rejected=false;try{fs::FsVerifierIO v(name+".legacy",b);}catch(const std::exception&e){need(std::string(e.what()).find("unknown suite/version")!=std::string::npos,e.what());rejected=true;}need(rejected,"legacy full-registration suite accepted");
 }
}
void buffered(const std::string&p,const fs::Binding&b){
 Bytes big(131073,17);fs::FsProverIO tx(p,b);
 for(unsigned i=0;i<1000;++i)tx.message("batch/"+std::to_string(i),Bytes(31,std::uint8_t(i)));
 tx.message("large",big);tx.message("empty",{});tx.finish();
 auto syscallCount=[](const fs::FsIO&io){auto s=io.metadataJson();auto at=s.find("\"file_syscalls\":");need(at!=std::string::npos,"FS bounded buffering metrics missing");return std::stoull(s.substr(at+16));};
 need(syscallCount(tx)<100,"FS small messages caused unbounded write syscalls");
 auto consumeAll=[&](const std::string&path){fs::FsVerifierIO rx(path,b);
  for(unsigned i=0;i<1000;++i)need(rx.message("batch/"+std::to_string(i))==Bytes(31,std::uint8_t(i)),"FS buffered small payload");
  need(rx.message("large")==big && rx.message("empty").empty(),"FS buffer boundary payload");rx.finish();
  need(rx.prefix()==tx.prefix() && rx.proofBytes()==tx.proofBytes(),"FS buffered transcript/bytes differ");
  need(syscallCount(rx)<100,"FS small messages caused unbounded read syscalls");};
 consumeAll(p);auto data=read(p);data.push_back(1);write(p+".append",data);rejects([&]{consumeAll(p+".append");});
 data.resize(data.size()-2);write(p+".short",data);rejects([&]{consumeAll(p+".short");});
}
}
int main(){try{
 r1::init();char pattern[]="build/fs-unit-XXXXXX";auto d=mkdtemp(pattern);need(d,"FS test temp");std::string p=std::string(d)+"/proof.fsproof";
 fs::Binding b{"Unit","test-context",std::string(64,'a'),{1,2,3}};
 buffered(p+".buffered",b);legacySuites(p);
 fs::CanonicalTranscript t({1,2,3},{4,5});auto initial=t.prefix();need(t.prefix()==initial,"FS prefix mutated state");t.message("m",{7,8});auto c=t.coin("c");std::cout<<"REFERENCE "<<hex(t.prefix())<<' '<<hex(t.challengeDigest())<<' '<<hex(r1::hash(r1::encode(c)))<<' '<<t.rejections()<<'\n';rejects([&]{t.coin("c");});
 fs::FsProverIO w(p,b);w.message("first",{7,8});auto x=w.coin("challenge");w.message("response",r1::encode(x*Fr(7)));need(!std::filesystem::exists(p),"FS early publication");auto prefix=w.prefix();w.finish();consume(p,b);need(std::filesystem::exists(p) && w.prefix()==prefix,"FS publication/prefix");
 auto bytes=read(p);rejects([&]{fs::FsProverIO duplicate(p,b);});need(read(p)==bytes,"FS overwrite changed original");
 auto fifo=p+".fifo";need(mkfifo(fifo.c_str(),0600)==0,"FS FIFO fixture");alarm(2);rejects([&]{fs::FsVerifierIO v(fifo,b);});alarm(0);
 auto link=p+".link";need(symlink(p.substr(p.find_last_of('/')+1).c_str(),link.c_str())==0,"FS symlink fixture");rejects([&]{fs::FsVerifierIO v(link,b);});rejects([&]{fs::FsVerifierIO v(d,b);});
 auto race=p+".race";{fs::FsProverIO w(race,b);write(race,{42});rejects([&]{w.finish();});need(read(race)==Bytes({42}),"FS publication race overwrote existing file");}
 auto modified=p+".modified";write(modified,bytes);{fs::FsVerifierIO v(modified,b);v.message("first");v.coin("challenge");v.message("response");auto changed=bytes;changed[0]^=1;write(modified,changed);struct stat st{};need(stat(modified.c_str(),&st)==0,"FS mutation stat");struct timespec times[2]={{0,UTIME_OMIT},{st.st_mtim.tv_sec+1,st.st_mtim.tv_nsec}};need(utimensat(AT_FDCWD,modified.c_str(),times,0)==0,"FS deterministic mutation timestamp");rejects([&]{v.finish();});}
 rejects([&]{w.coin("after-completion");});rejects([&]{w.message("after-completion",{});});
 auto other=b;other.context="other";rejects([&]{consume(p,other);});other=b;other.public_instance[0]^=1;rejects([&]{consume(p,other);});other=b;other.parameter_id[0]='b';rejects([&]{consume(p,other);});
 auto bad=p+".bad";auto changed=bytes;changed.push_back(0);write(bad,changed);rejects([&]{consume(bad,b);});changed=bytes;changed.pop_back();write(bad,changed);rejects([&]{consume(bad,b);});changed=bytes;changed[0]^=1;write(bad,changed);rejects([&]{consume(bad,b);});
 {fs::FsVerifierIO v(p,b);rejects([&]{v.message("reordered");});}
 auto interrupted=p+".interrupted";{fs::FsProverIO partial(interrupted,b);partial.message("first",{7,8});}need(!std::filesystem::exists(interrupted),"FS incomplete artifact published");
 for(const auto&e:std::filesystem::directory_iterator(d))if(e.path().string().find(".interrupted.partial.")!=std::string::npos)rejects([&]{consume(e.path().string(),b);});
 std::cout<<"PASS FS canonical transcript, independent challenge, file corruption, context, no-overwrite and atomic completion\n";return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
