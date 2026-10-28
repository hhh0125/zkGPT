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
}
int main(){try{
 r1::init();char pattern[]="build/fs-unit-XXXXXX";auto d=mkdtemp(pattern);need(d,"FS test temp");std::string p=std::string(d)+"/proof.fsproof";
 fs::Binding b{"Unit","test-context",std::string(64,'a'),{1,2,3}};
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
