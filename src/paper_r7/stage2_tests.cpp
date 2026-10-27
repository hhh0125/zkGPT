#include "stage2_opening.hpp"
#include <iostream>
#include <functional>
#include <fcntl.h>
#include <unistd.h>
using namespace zkgpt::r7::stage2;
namespace {
int checks=0;
void check(bool b,const char*m){++checks;need(b,m);}
void reject(const std::function<void()>&f,const std::string&message){++checks;try{f();}catch(const std::exception&e){need(std::string(e.what()).find(message)!=std::string::npos,e.what());return;}throw std::runtime_error("missing rejection: "+message);}
struct Count:Random{unsigned next=0;void fill(std::uint8_t*p,std::size_t n)override{auto b=r1::encode(Fr(++next));need(n==32,"unit entropy width");std::copy(b.begin(),b.end(),p);}};
struct Tape:IO{bool p;std::vector<std::pair<std::string,Bytes>>messages;std::size_t at=0;explicit Tape(bool prover):p(prover){}bool prover()const override{return p;}std::string session()const override{return std::string(64,'a');}Digest prefix()const override{return {};}
 Bytes message(const std::string&s,const Bytes&b={})override{if(p){messages.push_back({s,b});return b;}need(at<messages.size() && messages[at].first==s,"unit sequence");return messages[at++].second;}Fr coin(const std::string&)override{return Fr(11);}};
}
int main(){try{
 r1::init();Count rng;std::vector<Fr>w(1024,Fr(0));for(std::size_t j=0;j<768;++j)w[j]=Fr(j%17)-Fr(8);auto secret=commit(PCSpec::make(componentId,1024,1024),w,rng);auto approved=registry(secret.public_pc);auto bytes=encodeRegistry(approved);check(encodeRegistry(decodeRegistry(bytes))==bytes,"canonical registry roundtrip");
 Tape p(true);registration(p,approved,&secret,&rng);check(p.messages.size()==3,"full row registration protocol");Tape v(false);v.messages=p.messages;registration(v,approved,nullptr,nullptr);check(v.at==3,"V checks all row knowledge equations");
 Tape bad(false);bad.messages=p.messages;auto f=r1::decodeFr(Bytes(bad.messages[2].second.begin(),bad.messages[2].second.begin()+32));auto changed=r1::encode(f+Fr(1));std::copy(changed.begin(),changed.end(),bad.messages[2].second.begin());reject([&]{registration(bad,approved,nullptr,nullptr);},"row knowledge equation");
 auto tail=w;tail[1000]=Fr(1);auto badSecret=commit(PCSpec::make(componentId,1024,1024),tail,rng);auto badRegistry=registry(badSecret.public_pc);Tape tailP(true);registration(tailP,badRegistry,&badSecret,&rng);Tape tailV(false);tailV.messages=tailP.messages;reject([&]{registration(tailV,badRegistry,nullptr,nullptr);},"row knowledge equation");
 auto gen=approved;gen.generators=std::string(64,'0');reject([&]{encodeRegistry(gen);},"suite/generator");auto shape=approved;shape.shape_columns=512;reject([&]{encodeRegistry(shape);},"shape/object");auto padding=approved;padding.pc.spec.actual=1023;reject([&]{encodeRegistry(padding);},"zero-tail");
 auto badBytes=bytes;badBytes.push_back(0);reject([&]{decodeRegistry(badBytes);},"trailing");
 char name[]="build/.r7-s2-unit-XXXXXX";int out=mkstemp(name);need(out>2,"unit fixture output");writeOwnedFD(out,bytes);close(out);int input=open(name,O_RDONLY|O_NOFOLLOW);unlink(name);need(input>2,"unit fixture input");auto trusted=TrustedRegistry::load(input,registryDigest(approved));check(encodeRegistry(trusted.value())==bytes,"independently pinned registry loaded");reject([&]{TrustedRegistry::load(input,std::string(64,'0'));},"untrusted registry");close(input);
 MaskLedger ledger;auto mask=p.messages[1].second;ledger.observe("A3/knowledge/masks",mask);reject([&]{ledger.observe("A3/knowledge/masks",mask);},"repeated public protocol masks");
 Tape role(false);reject([&]{RegisteredCW::prover(role,approved,secret,rng);},"prover context role");
 std::cout<<"S2_UNIT_PASSED "<<checks<<"\n";return 0;
}catch(const std::exception&e){std::cerr<<"S2_UNIT_FAILED "<<e.what()<<'\n';return 1;}}
