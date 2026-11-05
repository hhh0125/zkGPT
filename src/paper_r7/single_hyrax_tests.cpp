#include "single_hyrax.hpp"
#include "fs_io.hpp"
#include <algorithm>
#include <iostream>
#include <random>
using namespace zkgpt::r7;
using namespace oneblock;
namespace sh=oneblock::single;
namespace {
// Only transport is replayed; challenges and all cryptographic equations use
// production code. The verifier has no witness callbacks or scalar values.
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
sh::Plan fixture(){return sh::makePlan(unified::pack({{1,8},{2,4},{3,2}},4),{3,5,4,2},std::string(64,'b'));}
std::vector<Fr>dense(const sh::Plan&p){std::vector<Fr>v(p.domain,Fr(0));for(std::size_t k=0;k<p.segments.size();++k)for(std::size_t j=0;j<p.segments[k].actual;++j)v[p.segments[k].offset+j]=Fr(10*k+j+1);return v;}
std::vector<Value>sources(const sh::Plan&p,const std::shared_ptr<std::vector<Fr>>&v){std::vector<Value>out;for(auto s:p.segments)out.push_back([v,s](std::size_t j){return v->at(s.offset+j);});return out;}
// Independent bit-by-bit reference, never the production split weights.
Fr chi(std::size_t j,const std::vector<Fr>&q){Fr f=1;for(unsigned b=0;b<q.size();++b)f*=((j>>b)&1)?q[b]:Fr(1)-q[b];return f;}
void shapeTests(){
 auto p=fixture();need(p.domain==32 && p.rows==4 && p.columns==4 && p.logical_values==14 && p.physical_values==11 && p.physical_domain==16,"compact private actual data must exclude X and logical gaps");
 struct Case{unsigned bits;std::size_t rows,columns;};
 for(auto c:{Case{10,32,32},Case{11,32,64},Case{12,64,64},Case{24,4096,4096},Case{26,8192,8192},Case{30,32768,32768},Case{32,65536,65536}}){
  auto shape=sh::matrixShape(std::size_t(1)<<c.bits);need(shape.rows==c.rows && shape.columns==c.columns && shape.rows*shape.columns==shape.physical_domain,"uncapped balanced matrix metadata");
 }
 auto big=sh::makePlan(unified::pack({{1,std::size_t(1)<<28}},4),{3,std::size_t(1)<<28},std::string(64,'b'));
 need(big.domain==(std::size_t(1)<<29) && big.rows==16384 && big.columns==16384,"compact physical domain must use uncapped balanced metadata");
 rejects([&]{sh::makePlan(unified::pack({{1,8}},4),{5,8},std::string(64,'b'));});
 auto changed=p;changed.rows++;rejects([&]{sh::descriptor(changed);});
 changed=p;changed.segments[1].offset=0;rejects([&]{sh::descriptor(changed);});
 changed=p;changed.logical_values++;rejects([&]{sh::descriptor(changed);});
 changed=p;changed.segments[2].physical_offset--;rejects([&]{sh::descriptor(changed);});
 changed=p;std::swap(changed.segments[1],changed.segments[2]);rejects([&]{sh::descriptor(changed);});
 changed=p;changed.segments[2].physical_length++;rejects([&]{sh::descriptor(changed);});
 rejects([]{sh::matrixShape(0);});rejects([]{sh::matrixShape((std::size_t(1)<<32)+1);});
 // A sparse private witness must not allocate 2^32 coefficient weights.
 auto sparse=sh::makePlan(unified::pack({{1,std::size_t(1)<<30},{3,std::size_t(1)<<30}},1),{1,1,1},std::string(64,'b'));
 std::size_t count=0;sh::mappedQueryTerms(sparse,std::vector<Fr>(32,Fr(2)),[&](const sh::QueryTerm&t){++count;need(t.left.size()==1 && t.right.size()==2,"sparse metadata query bound");});need(count==2,"sparse mapped term");
}
void coefficientTests(){
 std::mt19937 random(20261104);
 for(unsigned trial=0;trial<120;++trial){
  std::vector<unified::Block>blocks;std::vector<std::size_t>actual{1+random()%3};
  for(unsigned k=0;k<1+trial%7;++k){auto n=std::size_t(1)<<(random()%6);blocks.push_back({1+unsigned(random()%3),n});actual.push_back(1+random()%n);}
  auto p=sh::makePlan(unified::pack(blocks,4),actual,std::string(64,'b'));
  std::vector<Fr>q(sh::bits(p.domain));for(auto&v:q)v=trial%3?Fr(2+random()%13):Fr(random()%2);
  std::vector<Fr>coefficients(p.physical_domain,Fr(0)),expected(p.physical_domain,Fr(0));
  std::vector<Fr>physical(p.physical_domain,Fr(99));Fr expected_value=0;
  std::size_t cursor=0;for(std::size_t k=1;k<p.segments.size();++k){auto s=p.segments[k];need(s.physical_offset==cursor,"mapping lost/repeated source element");for(std::size_t j=0;j<s.actual;++j){expected[cursor]=chi(s.offset+j,q);physical[cursor]=Fr(cursor+1);expected_value+=expected[cursor]*physical[cursor];++cursor;}}
  std::size_t terms=0;sh::mappedQueryTerms(p,q,[&](const sh::QueryTerm&t){++terms;for(std::size_t i=0;i<t.left.size();++i)for(std::size_t j=0;j<t.right.size();++j)coefficients[(t.first_row+i)*p.columns+j]+=t.left[i]*t.right[j];});
  need(terms<=4*blocks.size() && coefficients==expected,"mapped coefficient/source boundary/carry differs from independent logical basis");
  need(r1::dot(physical,coefficients)==expected_value,"malicious physical padding affected logical evaluation");
  for(std::size_t j=cursor;j<p.physical_domain;++j)need(coefficients[j].isZero(),"physical padding query must be zero");
 }
 // Native X is a public coordinate inside a source, not extra private data.
 // Noncontiguous public cells split source runs; all holes remain exact.
 auto p=sh::makePlan(unified::pack({{1,8},{2,4}},4),{3,7,4},std::string(64,'b'),{},{{1,0,0},{1,3,1},{2,2,2}});
 need(p.physical_values==8 && p.physical_domain==8,"native public X copies must not be privately committed");
 std::vector<Fr>q(sh::bits(p.domain),Fr(2)),x{Fr(5),Fr(7),Fr(11)};Fr reference=0;
 for(std::size_t j=0;j<x.size();++j)reference+=chi(j,q)*x[j];
 for(const auto&c:p.public_cells)reference+=chi(p.segments[c.source].offset+c.index,q)*x[c.x_index];
 need(sh::publicContribution(p,q,x)==reference,"native public X contribution at original logical addresses");
 std::vector<Fr>coeff(p.physical_domain,Fr(0)),expected(p.physical_domain,Fr(0));
 sh::mappedQueryTerms(p,q,[&](const sh::QueryTerm&t){for(std::size_t i=0;i<t.left.size();++i)for(std::size_t j=0;j<t.right.size();++j)coeff[(t.first_row+i)*p.columns+j]+=t.left[i]*t.right[j];});
 for(const auto&run:p.runs)for(std::size_t j=0;j<run.count;++j)expected[run.physical_offset+j]=chi(p.segments[run.source].offset+run.source_offset+j,q);
 need(coeff==expected,"native public holes corrupt compact query mapping");
}
void openings(){
 auto plan=fixture();auto values=std::make_shared<std::vector<Fr>>(dense(plan));SessionRandom rng;
 auto secret=sh::commit(plan,sources(plan,values),rng,2);std::vector<Fr>x{Fr(1),Fr(2),Fr(3)};
 auto encoded=sh::encode(secret.pc);need(sh::encode(sh::decode(encoded,plan))==encoded,"single canonical PC roundtrip");
 auto malformed=encoded;malformed.push_back(0);rejects([&]{sh::decode(malformed,plan);});
 auto other=plan;other.source_identity=std::string(64,'c');rejects([&]{sh::decode(encoded,other);});
 other=plan;other.segments[1].source_id="swapped-source";rejects([&]{sh::decode(encoded,other);});
 std::vector<std::vector<Fr>>queries{{Fr(0),Fr(0),Fr(0),Fr(0),Fr(0)},{Fr(1),Fr(1),Fr(1),Fr(1),Fr(1)},{Fr(2),Fr(3),Fr(5),Fr(7),Fr(11)}};
 for(auto q:queries)need(sh::evaluate(secret,q)==r1::mle(*values,q),"mapped MLE differs from independent dense reference");
 Tape p(true);auto pc=sh::exchange(p,plan,&secret);
 for(std::size_t k=0;k<queries.size();++k){auto h=sh::evaluateAndOpen(p,pc,queries[k],x,&secret,&rng,"unit/open/"+std::to_string(k));need(h.value==r1::mle(*values,queries[k]),"single hidden evaluation incorrect");}
 auto replay=[&](Tape&v,const std::vector<Fr>&public_x){auto c=sh::exchange(v,plan,nullptr);for(std::size_t k=0;k<queries.size();++k)sh::evaluateAndOpen(v,c,queries[k],public_x,nullptr,nullptr,"unit/open/"+std::to_string(k));need(v.at==v.data.size(),"single unconsumed proof frames");};
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
 for(unsigned attack=0;attack<3;++attack){Tape tx(true);auto h=exchangeHidden(tx,"requested",r1::mle(*values,queries.back())+Fr(attack==0?1:0),&rng);auto q=queries.back();
  if(attack==2)values->at(plan.segments[1].offset)+=Fr(1);
  bool failed=false;try{sh::open(tx,secret.pc,{q,h.handle},x,&secret,&h,&rng,"requested/open");}catch(const std::exception&){failed=true;}if(attack==1)need(!failed,"valid prover opening unexpectedly failed");
  if(attack==2)values->at(plan.segments[1].offset)-=Fr(1);
  Tape judge(false);judge.data=tx.data;rejects([&]{auto value=exchangeHidden(judge,"requested",Fr(0),nullptr);auto point=q;if(attack==1)point[0]+=Fr(1);sh::open(judge,secret.pc,{point,value.handle},x,nullptr,nullptr,nullptr,"requested/open");});
 }
 // Mutating omitted logical gap/tail bytes cannot affect commitments/evaluation.
 {
  auto old_values=std::vector<Fr>(32,Fr(0));const std::size_t old_offsets[]{0,8,16,24};
  for(std::size_t k=0;k<plan.segments.size();++k)for(std::size_t j=0;j<plan.segments[k].actual;++j)old_values[old_offsets[k]+j]=values->at(plan.segments[k].offset+j);
  auto old_mle=r1::mle(old_values,queries.back());need(old_mle!=sh::evaluate(secret,queries.back()),"old/new offset fixture must have distinct MLEs");
  Tape tx(true);auto h=exchangeHidden(tx,"old-coefficient",old_mle,&rng);rejects([&]{sh::open(tx,secret.pc,{queries.back(),h.handle},x,&secret,&h,&rng,"old-query");});
  Tape judge(false);judge.data=tx.data;rejects([&]{auto y=exchangeHidden(judge,"old-coefficient",Fr(0),nullptr);sh::open(judge,secret.pc,{queries.back(),y.handle},x,nullptr,nullptr,nullptr,"old-query");});
 }
 auto reference=sh::evaluate(secret,queries.back());values->at(plan.segments[1].offset+plan.segments[1].actual)=Fr(99);values->at(4)=Fr(88);
 need(sh::evaluate(secret,queries.back())==reference,"omitted logical zero accepted prover data");
 Tape omitted(true);sh::evaluateAndOpen(omitted,secret.pc,queries.back(),x,&secret,&rng,"omitted");
 Tape judge(false);judge.data=omitted.data;sh::evaluateAndOpen(judge,secret.pc,queries.back(),x,nullptr,nullptr,"omitted");
}
void wireBoundaries(){
 auto big=sh::makePlan(unified::pack({{1,std::size_t(1)<<26}},4),{3,std::size_t(1)<<26},std::string(64,'b'));
 sh::Public pc{big,std::vector<G1>(big.rows,parameters(1).hiding)};auto bytes=sh::encode(pc);need(sh::decode(bytes,big).rows==pc.rows,"chunked compact PC codec roundtrip");
 SessionRandom rng;std::vector<Fr>x(131073,Fr(0));auto plan=sh::makePlan(unified::pack({{1,1}},262144),{x.size(),1},std::string(64,'b'));
 auto secret=sh::commit(plan,{[](std::size_t){return Fr(0);},[](std::size_t){return Fr(7);}},rng);
 std::vector<Fr>q(sh::bits(plan.domain),Fr(2));Tape p(true);sh::evaluateAndOpen(p,secret.pc,q,x,&secret,&rng,"large-X");
 std::size_t chunks=0;for(const auto&m:p.data){need(m.second.size()<=4u*1024*1024,"single FS payload bound");chunks+=m.first.rfind("large-X/public-X/",0)==0;}need(chunks==3,"public X must use bounded wire chunks");
 Tape v(false);v.data=p.data;sh::evaluateAndOpen(v,secret.pc,q,x,nullptr,nullptr,"large-X");need(v.at==v.data.size(),"large X opening EOF");
 // Real A.3 above the old cap, with a sparse vector and genuine verification.
 const std::size_t n=8192;std::vector<Fr>a(n,Fr(1));hyrax_a3::DotWitness w;w.values.assign(n,Fr(0));w.values.back()=Fr(13);w.blind=rng.scalar();auto C=hyrax_a3::vectorCommit(w.values,w.blind);
 Tape p2(true);w.evaluation=exchangeHidden(p2,"wide/evaluation",Fr(13),&rng);hyrax_a3::dotProduct(p2,C,w.evaluation.handle,a,&w,&rng);
 Tape v2(false);v2.data=p2.data;auto h=exchangeHidden(v2,"wide/evaluation",Fr(0),nullptr);hyrax_a3::dotProduct(v2,C,h.handle,a,nullptr,nullptr);need(v2.at==v2.data.size(),"wide A3 EOF");
}
}
int main(){try{r1::init();shapeTests();coefficientTests();openings();wireBoundaries();std::cout<<"SINGLE_HYRAX_COMPACT_TESTED\n";return 0;}catch(const std::exception&e){std::cerr<<"SINGLE_HYRAX_FAILED "<<e.what()<<'\n';return 1;}}
