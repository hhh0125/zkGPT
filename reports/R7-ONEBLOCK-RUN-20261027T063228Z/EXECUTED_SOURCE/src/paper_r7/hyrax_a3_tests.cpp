#include "hyrax_a3.hpp"
#include <iostream>
#include <functional>
using namespace zkgpt::r7::crypto;
namespace a3=zkgpt::r7::hyrax_a3;
namespace {
int checks=0;
void check(bool b,const char*s){++checks;need(b,s);}
struct CountRandom:Random{unsigned count=0;void fill(std::uint8_t*p,std::size_t n)override{auto b=r1::encode(Fr(++count));need(n==b.size(),"fixture entropy width");std::copy(b.begin(),b.end(),p);}};
struct Tape:IO{
 bool p;std::vector<std::pair<std::string,Bytes>>records;std::vector<Fr>coins;std::size_t pos=0,ci=0;
 explicit Tape(bool role):p(role),coins{Fr(11),Fr(2),Fr(3)}{}
 bool prover()const override{return p;}std::string session()const override{return std::string(64,'a');}Digest prefix()const override{return {};}
 Bytes message(const std::string&label,const Bytes&b={})override{if(p){records.push_back({label,b});return b;}need(pos<records.size() && records[pos].first==label,"fixture message sequence");return records[pos++].second;}
 Fr coin(const std::string&)override{need(ci<coins.size(),"fixture coin sequence");return coins[ci++];}
};
void rejected(const std::function<void()>&f,const std::string&why){++checks;try{f();}catch(const std::exception&e){need(std::string(e.what()).find(why)!=std::string::npos,e.what());return;}throw std::runtime_error("expected rejection: "+why);}
}
int main(){try{
 r1::init();auto bases=a3::generators(2);auto g=parameters(1);const std::vector<Fr>a{Fr(5),Fr(7)};
 Hidden hidden;hidden.handle.session=std::string(64,'a');hidden.handle.role="fixture/hidden";hidden.value=31;hidden.blind=13;hidden.handle.point=scalarCommit(hidden.value,hidden.blind);
 a3::DotWitness witness{{Fr(2),Fr(3)},Fr(11),hidden};auto xi=a3::vectorCommit(witness.values,witness.blind);
 Tape p(true);CountRandom rng;a3::dotProduct(p,xi,hidden.handle,a,&witness,&rng);
 check(p.records.size()==6,"two-input PoK + literal n2 A3 message count");
 // Independent hand-derived Figure7 fixture: x=(2,3), a=(5,7), y=31; c=2.
 Reader cross{p.records[3].second};check(cross.point()==bases[1]*Fr(2)+g.scalar*Fr(14)+g.hiding*Fr(6),"Figure7 L cross/blind");check(cross.point()==bases[0]*Fr(3)+g.scalar*Fr(15)+g.hiding*Fr(7),"Figure7 R cross/blind");cross.end();
 Fr half,quarter,eighth;Fr::inv(half,Fr(2));Fr::inv(quarter,Fr(4));Fr::inv(eighth,Fr(8));auto ghat=bases[0]*half+bases[1]*Fr(2);
 Reader mask{p.records[4].second};check(mask.point()==ghat*Fr(8)+g.hiding*Fr(9),"Figure8 delta");check(mask.point()==g.scalar*Fr(8)+g.hiding*Fr(10),"Figure8 beta");mask.end();
 Reader z{p.records[5].second};check(z.scalar()==Fr(1121)*quarter,"Figure8 z1 uses yhat not xhat");check(z.scalar()==Fr(21093)*eighth,"Figure8 z2 includes all recursive blinds");z.end();
 Tape v(false);v.records=p.records;a3::dotProduct(v,xi,hidden.handle,a,nullptr,nullptr);check(v.pos==v.records.size(),"independent V consumes all messages");
 for(auto mode:{std::string("cross"),std::string("blind"),std::string("truncate"),std::string("trailing"),std::string("suite"),std::string("point"),std::string("scalar"),std::string("vector-guard"),std::string("scalar-guard")}){
  Tape bad(false);bad.records=p.records;
  if(mode=="cross"){Writer w;w.point(bases[1]*Fr(2)+g.scalar*Fr(14));w.point(bases[0]*Fr(3)+g.scalar*Fr(15)+g.hiding*Fr(7));bad.records[3].second=w.b;}
  if(mode=="blind"){Writer w;w.scalar(Fr(1121)*quarter);w.scalar(Fr(21093)*eighth+Fr(1));bad.records[5].second=w.b;}
  if(mode=="truncate")bad.records[3].second.pop_back();
  if(mode=="trailing")bad.records[5].second.push_back(0);
  if(mode=="suite")bad.records[0].second[4]^=1;
  if(mode=="point")std::fill(bad.records[3].second.begin(),bad.records[3].second.begin()+32,255);
  if(mode=="scalar")std::fill(bad.records[5].second.begin(),bad.records[5].second.begin()+32,255);
  if(mode=="vector-guard"){auto f=r1::decodeFr(Bytes(bad.records[2].second.begin(),bad.records[2].second.begin()+32));auto changed=r1::encode(f+Fr(1));std::copy(changed.begin(),changed.end(),bad.records[2].second.begin());}
  if(mode=="scalar-guard"){auto f=r1::decodeFr(Bytes(bad.records[2].second.end()-32,bad.records[2].second.end()));auto changed=r1::encode(f+Fr(1));std::copy(changed.begin(),changed.end(),bad.records[2].second.end()-32);}
  std::string why=mode=="truncate"?"truncated":mode=="trailing"?"trailing":mode=="suite"?"suite/version":mode=="point"?"noncanonical G1":mode=="scalar"?"noncanonical Fr":mode=="vector-guard"?"vector knowledge":mode=="scalar-guard"?"scalar knowledge":"Schnorr equation";
  rejected([&]{a3::dotProduct(bad,xi,hidden.handle,a,nullptr,nullptr);},why);
 }
 // Substituting a challenge while preserving messages cannot authorize the proof.
 Tape challenge(false);challenge.records=p.records;challenge.coins[1]=Fr(3);rejected([&]{a3::dotProduct(challenge,xi,hidden.handle,a,nullptr,nullptr);},"Schnorr equation");
 // Figure8 alone collapses xi+tau. The separate-input knowledge guard must reject.
 for(bool cancellation:{false,true}){
  auto fake=hidden;fake.value=cancellation?Fr(-17):Fr(14);fake.blind=13;fake.handle.point=scalarCommit(fake.value,fake.blind);
  G1 badxi=cancellation?-fake.handle.point:xi+g.scalar*Fr(17);auto w=witness;w.evaluation=fake;if(cancellation){w.values={Fr(0),Fr(0)};w.blind=-fake.blind;}
  Tape forged(true);CountRandom rand;a3::dotProduct(forged,badxi,fake.handle,a,&w,&rand);
  Tape judge(false);judge.records=forged.records;rejected([&]{a3::dotProduct(judge,badxi,fake.handle,a,nullptr,nullptr);},"vector knowledge equation");
 }
 Tape noinput(true);rejected([&]{a3::dotProduct(noinput,xi,hidden.handle,{},nullptr,nullptr);},"dimension");
 rejected([&]{a3::dotProduct(noinput,xi,hidden.handle,{Fr(0),Fr(0)},nullptr,nullptr);},"zero public vector");
 rejected([&]{a3::generators(3);},"dimension");rejected([&]{a3::generators(2048);},"dimension");
 // a=(4,-1), fold c=2 -> ahat=0: fail closed before Figure8 loses U.
 auto zeroa=std::vector<Fr>{Fr(4),Fr(-1)};auto zeroW=witness;zeroW.evaluation.value=5;zeroW.evaluation.handle.point=scalarCommit(Fr(5),hidden.blind);
 Tape deg(true);CountRandom rr;rejected([&]{a3::dotProduct(deg,xi,zeroW.evaluation.handle,zeroa,&zeroW,&rr);},"zero final coefficient");
 Tape degV(false);degV.records=deg.records;rejected([&]{a3::dotProduct(degV,xi,zeroW.evaluation.handle,zeroa,nullptr,nullptr);},"zero final coefficient");
 for(unsigned at=0;at<3;++at){Tape zc(false);zc.records=p.records;zc.coins[at]=Fr(0);rejected([&]{a3::dotProduct(zc,xi,hidden.handle,a,nullptr,nullptr);},"zero verifier challenge");}
 // Honest zero witness, zero blind, identity xi/tau and n=1 remain supported.
 a3::DotWitness zw;zw.values={Fr(0)};zw.blind=0;zw.evaluation.handle=hidden.handle;zw.evaluation.handle.point.clear();G1 identity;identity.clear();
 Tape zp(true);zp.coins={Fr(11),Fr(3)};CountRandom randomZero;a3::dotProduct(zp,identity,zw.evaluation.handle,{Fr(1)},&zw,&randomZero);
 Tape zv(false);zv.coins=zp.coins;zv.records=zp.records;a3::dotProduct(zv,identity,zw.evaluation.handle,{Fr(1)},nullptr,nullptr);check(zv.pos==zv.records.size(),"n1 identity zero-evaluation positive");
 struct FailedRandom:Random{void fill(std::uint8_t*,std::size_t)override{throw std::runtime_error("deliberate entropy failure");}}failed;
 Tape noentropy(true);rejected([&]{a3::dotProduct(noentropy,xi,hidden.handle,a,&witness,&failed);},"entropy failure");check(noentropy.records.size()==1,"entropy failure sends no knowledge masks/responses");
 // No honest secret or true evaluation is serialized; the only scalar responses are masked.
 std::cout<<"A3_UNIT_PASSED "<<checks<<"\n";return 0;
}catch(const std::exception&e){std::cerr<<"A3_UNIT_FAILED "<<e.what()<<'\n';return 1;}}
