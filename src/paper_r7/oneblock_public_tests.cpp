#include "oneblock_public.hpp"
#include "paper_r2/r5b_trust.hpp"
#include <algorithm>
#include <functional>
#include <iostream>
using namespace zkgpt::r7;
using namespace crypto;
using namespace oneblock;
namespace {
struct CountRandom:Random {unsigned n=0;void fill(std::uint8_t*p,std::size_t size)override{auto b=r1::encode(Fr(++n));need(size==b.size(),"public test entropy size");std::copy(b.begin(),b.end(),p);}};
struct Tape:IO {
 bool p,entries_bound=false;std::vector<std::pair<std::string,Bytes>>records;std::size_t at=0,coins=0;
 explicit Tape(bool role):p(role){}bool prover()const override{return p;}std::string session()const override{return std::string(64,'d');}Digest prefix()const override{return {};}
 Bytes message(const std::string&label,const Bytes&b={})override{if(label=="public/unit/public-entries")entries_bound=true;if(p){records.emplace_back(label,b);return b;}need(at<records.size() && records[at].first==label,"public test independent message order");return records[at++].second;}
 Fr coin(const std::string&label)override{if(label.find("public/unit/position-query/")==0)need(entries_bound,"public query precedes entry-list binding");return Fr(3+coins++);}
};
unsigned checks=0;
void check(bool good,const char*why){++checks;need(good,why);}
void rejects(const std::function<void()>&f,const char*why){++checks;try{f();}catch(const std::exception&e){need(std::string(e.what()).find(why)!=std::string::npos,e.what());return;}throw std::runtime_error("public test expected rejection");}
master3::Spec smallSpec(){
 master3::Spec s;s.actual=s.N=4096;s.rows=4;s.columns=1024;s.layout=std::string(64,'a');s.profile=zkgpt::r5b::profileHash();s.scope="Base-Master-prefix";
 Writer w;w.text(hyrax_a3::suite);w.text("MCL-BN254-y2=x3+2-Fr-v1");for(auto&g:hyrax_a3::generators(1024))w.point(g);w.point(parameters(1).scalar);w.point(parameters(1).hiding);s.generators=hex(r1::hash(w.b));master3::validate(s);return s;
}
void protocol(Tape&io,const master3::Master&m,const master3::Secret*s,const PublicEntries&entries,Random*rng){
 masterB::RegistrationMetrics metrics;auto registered=masterB::Registered::establish(io,m,s,"public-unit",metrics);Value source;if(io.prover())source=s->value;publicBinding(io,registered,s,source,entries,rng,"public/unit");
}
}
int main(){try{
 r1::init();std::vector<Fr>w(4096,Fr(0));for(std::size_t i=0;i<w.size();++i)w[i]=Fr(int(i%13)-6);w[0]=5;w[17]=11;w[64]=1;w[65]=0;auto secret=master3::commitMaster(smallSpec(),[&w](std::size_t i){return w.at(i);});
 // Repeated Y index is intentional: K[17] must ADD its independent list
 // position weights. Overwriting it makes this honest proof fail.
 PublicEntries entries{{0,Fr(5)},{17,Fr(11)},{64,Fr(1)},{65,Fr(0)},{17,Fr(11)}};CountRandom rng;Tape p(true);protocol(p,secret.public_pc,&secret,entries,&rng);Tape v(false);v.records=p.records;protocol(v,secret.public_pc,nullptr,entries,nullptr);check(v.at==v.records.size(),"public X/Y/constants independent verifier and repeated map");
 for(std::size_t position:{std::size_t(0),std::size_t(1),std::size_t(2),std::size_t(3)}){
  auto wrong=entries;wrong[position].second+=Fr(1);Tape altered(false);altered.records=p.records;rejects([&]{protocol(altered,secret.public_pc,nullptr,wrong,nullptr);},"public statement/parent binding mismatch");
  // The fixed public claim must also fail algebraically against a valid,
  // registered Master containing another X, Y, one, or zero value.
  CountRandom entropy;Tape malicious(true);rejects([&]{protocol(malicious,secret.public_pc,&secret,wrong,&entropy);},"compressed linear relation failed");
 }
 auto badIndex=entries;badIndex[0].first=4096;Tape outside(true);CountRandom extra;rejects([&]{protocol(outside,secret.public_pc,&secret,badIndex,&extra);},"public entry outside actual Master");
 R7Context context;context.plan.master=r1::PCSpec::make("Master",40000,65536,r1::Layout::ColumnMajor);context.plan.shape.sequence_length=32;context.plan.shape.hidden_dimension=768;context.plan.output={33000,33001};context.plan.constants={39000};context.plan.reserved_zero={39001};R7Statement statement;statement.master_actual=40000;statement.master_padded=65536;statement.x.assign(32768,Fr(0));statement.x[0]=5;statement.y={Fr(11),Fr(13)};auto trusted=trustedPublicEntries(statement,context);
 check(trusted.x.size()==32768 && trusted.x.front().first==0 && trusted.x.back().first==32767 && trusted.x.front().second==Fr(5),"trusted X is original contiguous padded input");
 check(trusted.y.size()==2 && trusted.y[0].first==33000 && trusted.y[1].first==33001 && trusted.y[1].second==Fr(13),"trusted Y uses verifier-owned output mapping");
 check(trusted.constants.size()==2 && trusted.constants[0]==std::make_pair(std::size_t(39000),Fr(1)) && trusted.constants[1]==std::make_pair(std::size_t(39001),Fr(0)),"trusted ones and reserved zeros");
 std::cout<<"ONEBLOCK_PUBLIC_UNIT_PASSED "<<checks<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"ONEBLOCK_PUBLIC_UNIT_FAILED "<<e.what()<<'\n';return 1;}}
