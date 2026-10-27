#include "oneblock_gkr.hpp"
#include "prover.hpp"
#include "utils.hpp"
#include <algorithm>
#include <iostream>

using namespace zkgpt::r7;
using namespace crypto;
using namespace oneblock;
namespace {
// Deterministic coins are confined to this arithmetic/replay unit fixture.
struct CountRandom final : Random {
 unsigned draws=0;
 void fill(std::uint8_t*p,std::size_t n) override {
  auto b=r1::encode(Fr(++draws));need(n==b.size(),"GKR test entropy width");std::copy(b.begin(),b.end(),p);
 }
};
struct Tape final : IO {
 bool is_p;std::vector<std::pair<std::string,Bytes>> records;std::size_t at=0,coins=0;
 explicit Tape(bool p):is_p(p){}
 bool prover()const override{return is_p;}
 std::string session()const override{return std::string(64,'c');}
 Digest prefix()const override{return {};}
 Bytes message(const std::string&label,const Bytes&bytes={}) override {
  if(is_p){records.emplace_back(label,bytes);return bytes;}
  need(at<records.size() && records[at].first==label,"GKR test independent verifier message order");return records[at++].second;
 }
 Fr coin(const std::string&) override{return Fr(3+coins++);}
};
struct Fixture {
 ::prover p;
 Fixture(){
  p.C.init(0,5);p.val.resize(5);p.val[0].assign(32,Fr(0));p.val[0][0]=Fr(3);p.val[0][20]=p.val[0][21]=Fr(9);
  for(unsigned j=0;j<4;++j)p.val[0][4+4*j]=Fr(1);
  initLayer(p.C.circuit[0],32,layerType::INPUT);
  for(unsigned fc=0;fc<2;++fc){
   unsigned id=1+2*fc;auto&m=p.C.circuit[id];initLayer(m,4,layerType::FCONN);m.need_phase2=true;
   for(unsigned j=0;j<4;++j)m.ori_id_u.push_back(j);
   for(unsigned j=0;j<16;++j)m.ori_id_v.push_back(4+j);
   m.size_u[0]=4;m.size_v[0]=16;p.val[id].assign(4,Fr(3));
   auto&l=p.C.circuit[id+1];initLayer(l,4,layerType::Sqr);l.need_phase2=true;l.zero_start_id=0;
   for(unsigned j=0;j<4;++j){l.bin_gates.emplace_back(j,j,j,1,1);l.uni_gates.emplace_back(j,fc==0?21:20,0,-1);}
   p.val[id+1].assign(4,Fr(0));
  }
  p.C.initSubset();
 }
};
void expectedReject(const std::function<void()>&f,const char*why){bool bad=false;try{f();}catch(const std::exception&){bad=true;}need(bad,why);}
void replaceHandle(Tape&t,const Bytes&before,const Bytes&after){
 need(before.size()==after.size(),"GKR test handle mutation preserves canonical length");
 for(auto&record:t.records){auto it=record.second.begin();while((it=std::search(it,record.second.end(),before.begin(),before.end()))!=record.second.end()){std::copy(after.begin(),after.end(),it);it+=after.size();}}
}
void expectedAlgebraReject(const std::function<void()>&f,const char*why){
 bool bad=false;try{f();}catch(const std::exception&e){bad=std::string(e.what()).find("equation")!=std::string::npos;}need(bad,why);
}
}
int main(){try{
 r1::init();Fixture fixture;CountRandom random;Tape p(true);
 auto pending=runHiddenGKR(p,fixture.p.C,&fixture.p,1,&random);
 need(pending.layers==4 && pending.matrices==4 && pending.carries==4,"GKR covers both FC paths and every internal carry");
 need(pending.terminals.size()==10,"GKR retains both four-endpoint FC paths and normal input endpoints");
 need(!pending.master_authenticated,"GKR reduction alone must remain pending Master authentication");
 for(auto&t:pending.terminals){
  auto&layer=fixture.p.C.circuit.at(t.layer);auto&map=t.port?layer.ori_id_v:layer.ori_id_u;
  std::vector<Fr>values(t.padded,Fr(0));for(std::size_t j=0;j<t.actual;++j)values[j]=fixture.p.val[0].at(map.at(j));
  need(r1::mle(values,t.point)==t.value.value,"GKR pending terminal is the actual mapped common witness evaluation");
 }
 // The verifier has only its independent public graph and masked messages.
 Tape v(false);v.records=p.records;auto verified=runHiddenGKR(v,fixture.p.C,nullptr,1,nullptr);
 need(v.at==v.records.size() && verified.terminals.size()==pending.terminals.size(),"GKR witness-free verifier consumes all messages");
 for(std::size_t j=0;j<pending.terminals.size();++j)need(encode(verified.terminals[j].value.handle)==encode(pending.terminals[j].value.handle),"GKR verifier preserves original terminal handle");
 // Mutate a coefficient commitment as a valid group element. This must reach
 // a representation/recurrence equation, rather than only a format rejection.
 Tape coefficient(false);coefficient.records=p.records;bool changed=false;
 for(auto&record:coefficient.records)if(record.first.find("/c/0/commitment")!=std::string::npos){auto before=record.second;auto h=decodeHandle(before);h.point+=parameters(1).scalar;replaceHandle(coefficient,before,encode(h));changed=true;break;}
 need(changed,"GKR fixture locates hidden polynomial coefficient");
 expectedAlgebraReject([&]{runHiddenGKR(coefficient,fixture.p.C,nullptr,1,nullptr);},"GKR altered coefficient must reject algebraically");
 Tape endpoint(false);endpoint.records=p.records;changed=false;
 for(auto&record:endpoint.records)if(record.first.find("/final/u1/commitment")!=std::string::npos){auto before=record.second;auto h=decodeHandle(before);h.point+=parameters(1).scalar;replaceHandle(endpoint,before,encode(h));changed=true;break;}
 need(changed,"GKR fixture locates carried endpoint");
 expectedAlgebraReject([&]{runHiddenGKR(endpoint,fixture.p.C,nullptr,1,nullptr);},"GKR altered internal carry must reject algebraically");
 Tape truncated(false);truncated.records=p.records;truncated.records.pop_back();
 expectedReject([&]{runHiddenGKR(truncated,fixture.p.C,nullptr,1,nullptr);},"GKR truncated transcript must reject");
 auto wrong=fixture.p.C;wrong.circuit[4].uni_gates[0].sc=-2;Tape wiring(false);wiring.records=p.records;
 expectedReject([&]{runHiddenGKR(wiring,wrong,nullptr,1,nullptr);},"GKR changed public wiring must reject");
 // FC has no previous-layer carry. The zero constraint just below it must
 // still be checked independently rather than acquire an all-zero predicate.
 Fixture disconnected;disconnected.p.C.circuit[2].uni_gates[0].sc=-2;Tape disconnectedP(true);CountRandom disconnectedRandom;
 expectedReject([&]{runHiddenGKR(disconnectedP,disconnected.p.C,&disconnected.p,1,&disconnectedRandom);},"GKR disconnected zero constraint must not become vacuous below FC");
 Fixture wrongMaster;wrongMaster.p.val[0][21]=Fr(8);Tape wrongMasterP(true);CountRandom wrongMasterRandom;
 expectedReject([&]{runHiddenGKR(wrongMasterP,wrongMaster.p.C,&wrongMaster.p,1,&wrongMasterRandom);},"GKR rejects wrong Master in a zero-constraint layer without a parent carry");
 need(wrongMaster.p.val[0].size()==32 && wrongMaster.p.val[0][21]==Fr(8) && wrongMaster.p.val[1].size()==4,"GKR RAII returns borrowed witness after an algebraic failure");
 need(fixture.p.cc.w==nullptr && fixture.p.cc.comm==nullptr && fixture.p.gens.empty(),"GKR adapter never invokes nonhiding commitment/opening");
 std::cout<<"ONEBLOCK_GKR_UNIT_PASSED pending_terminals="<<pending.terminals.size()<<" matrices="<<pending.matrices<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"ONEBLOCK_GKR_UNIT_FAILED "<<e.what()<<'\n';return 1;}}
