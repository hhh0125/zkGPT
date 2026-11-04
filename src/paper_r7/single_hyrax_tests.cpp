#include "single_hyrax.hpp"
#include "fs_io.hpp"
#include <algorithm>
#include <iostream>
using namespace zkgpt::r7;
using namespace oneblock;
namespace sh=oneblock::single;
namespace {
// A transport only: all commitments, transcript challenges, Sigma equations and
// IPA verification are the production implementations, with no witness on V.
struct Tape:IO {
 bool p;std::size_t at=0;std::vector<std::pair<std::string,Bytes>>data;
 fs::CanonicalTranscript transcript{{1,4,7},{2,5,8}};
 explicit Tape(bool prover):p(prover){}
 bool prover()const override{return p;}
 std::string session()const override{return std::string(64,'a');}
 Digest prefix()const override{return transcript.prefix();}
 Bytes message(const std::string&n,const Bytes&b={})override{
  Bytes out=b;if(p)data.emplace_back(n,b);else{need(at<data.size() && data[at].first==n,"single test frame order/EOF");out=data[at++].second;}
  transcript.message(n,out);return out;
 }
 Fr coin(const std::string&n)override{return transcript.coin(n);}
};
template<class F>void rejects(F&&f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}need(failed,"single verifier accepted malicious proof");}
sh::Plan fixture(){return sh::makePlan(unified::pack({{1,8},{2,4},{3,2}},4),{3,5,4,2},std::string(64,'b'),4);}
std::vector<Fr>dense(const sh::Plan&p){std::vector<Fr>v(p.domain,Fr(0));for(std::size_t k=0;k<p.segments.size();++k)for(std::size_t j=0;j<p.segments[k].actual;++j)v[p.segments[k].offset+j]=Fr(10*k+j+1);return v;}
std::vector<Value>sources(const sh::Plan&p,const std::shared_ptr<std::vector<Fr>>&v){std::vector<Value>out;for(auto s:p.segments)out.push_back([v,s](std::size_t j){return v->at(s.offset+j);});return out;}
void shapeTests(){
 auto p=fixture();need(p.domain==32 && p.rows==8 && p.columns==4 && p.logical_values==14,"single hand-derived packing/shape");
 auto big=sh::makePlan(unified::pack({{1,std::size_t(1)<<28}},4),{3,std::size_t(1)<<28},std::string(64,'b'));
 need(big.domain==(std::size_t(1)<<30) && big.rows==262144 && big.columns==4096,"single virtual domain uses bounded matrix metadata");
 rejects([&]{sh::makePlan(unified::pack({{1,8}},4),{5,8},std::string(64,'b'));});
 auto changed=p;changed.rows++;rejects([&]{sh::descriptor(changed);});
 changed=p;changed.segments[1].offset=0;rejects([&]{sh::descriptor(changed);});
 changed=p;changed.logical_values++;rejects([&]{sh::descriptor(changed);});
}
void openings(){
 auto plan=fixture();auto values=std::make_shared<std::vector<Fr>>(dense(plan));SessionRandom rng;
 auto secret=sh::commit(plan,sources(plan,values),rng,2);std::vector<Fr>x{Fr(1),Fr(2),Fr(3)};
 auto encoded=sh::encode(secret.pc);need(sh::encode(sh::decode(encoded,plan))==encoded,"single canonical PC roundtrip");
 auto malformed=encoded;malformed.push_back(0);rejects([&]{sh::decode(malformed,plan);});
 auto other=plan;other.source_identity=std::string(64,'c');rejects([&]{sh::decode(encoded,other);});
 // Boolean corners pin low-column/high-row bit order; random points check folds.
 std::vector<std::vector<Fr>>queries{{Fr(0),Fr(0),Fr(0),Fr(0),Fr(0)},{Fr(1),Fr(1),Fr(1),Fr(1),Fr(1)},{Fr(2),Fr(3),Fr(5),Fr(7),Fr(11)}};
 for(auto q:queries)need(sh::evaluate(secret,q)==r1::mle(*values,q),"single MLE differs from independent dense reference");
 Tape p(true);auto pc=sh::exchange(p,plan,&secret);sh::constrainFixed(p,pc,x,&secret,&rng);
 for(std::size_t k=0;k<queries.size();++k){auto h=sh::evaluateAndOpen(p,pc,queries[k],&secret,&rng,"unit/open/"+std::to_string(k));need(h.value==r1::mle(*values,queries[k]),"single hidden evaluation incorrect");}
 auto replay=[&](Tape&v,const std::vector<Fr>&public_x){auto c=sh::exchange(v,plan,nullptr);sh::constrainFixed(v,c,public_x,nullptr,nullptr);for(std::size_t k=0;k<queries.size();++k)sh::evaluateAndOpen(v,c,queries[k],nullptr,nullptr,"unit/open/"+std::to_string(k));need(v.at==v.data.size(),"single unconsumed proof frames");};
 Tape v(false);v.data=p.data;replay(v,x);
 for(unsigned attack=0;attack<7;++attack){Tape bad(false);bad.data=p.data;bool hit=false;
  if(attack<4)for(auto&m:bad.data){
   if(attack==0 && m.first=="SingleHyrax/rows/0"){Reader r{m.second};auto count=r.number();Writer w;w.number(count);for(std::size_t j=0;j<count;++j){auto a=r.point();if(!j)a+=crypto::parameters(1).scalar;w.point(a);}r.end();m.second=w.b;hit=true;break;}
   if(attack==1 && m.first.find("/evaluation/commitment")!=std::string::npos){auto h=decodeHandle(m.second);h.point+=crypto::parameters(1).scalar;m.second=crypto::encode(h);hit=true;break;}
   if(attack==2 && m.first.find("/A3/final/z1-z2")!=std::string::npos){m.second.back()^=1;hit=true;break;}
   if(attack==3 && m.first=="SingleHyrax/header"){m.second.back()^=1;hit=true;break;}
  }
  if(attack==4){std::swap(bad.data[1],bad.data[2]);hit=true;}
  if(attack==5){bad.data.pop_back();hit=true;}
  auto wrong=x;if(attack==6){wrong[0]+=Fr(1);hit=true;}
  need(hit,"single negative fixture missed target");rejects([&]{replay(bad,wrong);});
 }
 // Real IPA verifier rejects a wrong hidden y and an altered query.
 for(unsigned attack=0;attack<3;++attack){Tape tx(true);auto h=exchangeHidden(tx,"requested",r1::mle(*values,queries.back())+Fr(attack==0?1:0),&rng);auto q=queries.back();
  if(attack==2)values->at(plan.segments[1].offset)+=Fr(1);
  sh::open(tx,secret.pc,{q,h.handle},&secret,&h,&rng,"requested/open");
  if(attack==2)values->at(plan.segments[1].offset)-=Fr(1);
  Tape judge(false);judge.data=tx.data;rejects([&]{auto value=exchangeHidden(judge,"requested",Fr(0),nullptr);auto point=q;if(attack==1)point[0]+=Fr(1);sh::open(judge,secret.pc,{point,value.handle},nullptr,nullptr,nullptr,"requested/open");});
 }
}
void physicalPadding(){
 auto plan=fixture();SessionRandom rng;
 // Valid public X but malicious private tail/gap: verifier must reject the
 // fixed-cell Sigma equation, not a hand-coded check of the fixture's bytes.
 for(auto index:{plan.segments[1].offset+plan.segments[1].actual,std::size_t(4)}){
  auto values=std::make_shared<std::vector<Fr>>(dense(plan));values->at(index)=Fr(99);
  auto secret=sh::commit(plan,sources(plan,values),rng);if(index==4){secret.pc.rows[1]+=hyrax_a3::generators(plan.columns)[0];}
  Tape p(true);auto pc=sh::exchange(p,plan,&secret);sh::constrainFixed(p,pc,{Fr(1),Fr(2),Fr(3)},&secret,&rng);
  Tape v(false);v.data=p.data;rejects([&]{auto c=sh::exchange(v,plan,nullptr);sh::constrainFixed(v,c,{Fr(1),Fr(2),Fr(3)},nullptr,nullptr);});
 }
}
void wireBoundaries(){
 SessionRandom rng;const auto H=parameters(1).hiding;
 // More than 4 MiB of PUBLIC row points must use bounded codec chunks rather
 // than the small per-message crypto::Writer's aggregate size limit.
 auto big=sh::makePlan(unified::pack({{1,std::size_t(1)<<27}},4),{3,std::size_t(1)<<27},std::string(64,'b'));
 sh::Public pc{big,std::vector<G1>(big.rows,H)};auto bytes=sh::encode(pc);need(bytes.size()>4u*1024*1024,"large PC boundary fixture");need(sh::decode(bytes,big).rows==pc.rows,"large PC codec roundtrip");
 std::vector<Fr>x(131073,Fr(0));auto plan=sh::makePlan(unified::pack({{1,1}},262144),{x.size(),1},std::string(64,'b'));
 sh::Secret secret;secret.pc={plan,std::vector<G1>(plan.rows,H)};secret.blinds.assign(plan.rows,Fr(1));secret.sources={[](std::size_t){return Fr(0);},[](std::size_t){return Fr(0);}};
 Tape p(true);auto pub=sh::exchange(p,plan,&secret);sh::constrainFixed(p,pub,x,&secret,&rng);
 std::size_t chunks=0;for(auto&m:p.data){need(m.second.size()<=4u*1024*1024,"single event exceeds FS payload bound");chunks+=m.first.rfind("SingleHyrax/fixed-cells/public-X/",0)==0;}need(chunks==3,"large X must be bound in three bounded chunks");
 Tape v(false);v.data=p.data;auto expected=sh::exchange(v,plan,nullptr);sh::constrainFixed(v,expected,x,nullptr,nullptr);need(v.at==v.data.size(),"large X fixed-cell verification/EOF");
}
}
int main(){try{r1::init();shapeTests();openings();physicalPadding();wireBoundaries();std::cout<<"SINGLE_HYRAX_TESTED\n";return 0;}catch(const std::exception&e){std::cerr<<"SINGLE_HYRAX_FAILED "<<e.what()<<'\n';return 1;}}
