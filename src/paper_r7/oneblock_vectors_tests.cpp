#include "oneblock_vectors.hpp"
#include "streaming_master.hpp"
#include <algorithm>
#include <iostream>
using namespace zkgpt::r7;
using namespace zkgpt::r7::crypto;
using namespace zkgpt::r7::oneblock;
namespace {
struct Tape final:IO {
 bool p;std::size_t at=0,ci=0;std::vector<std::pair<std::string,Bytes>>records;
 r1::Fs transcript{"R7-ONEBLOCK-VECTOR-UNIT-LIVE-REPLAY-v1"};
 explicit Tape(bool prover):p(prover){}
 bool prover()const override{return p;}
 std::string session()const override{return std::string(64,'c');}
 Digest prefix()const override{return transcript.digest();}
 Bytes message(const std::string&label,const Bytes&b={})override{
  Bytes out;if(p){records.push_back({label,b});out=b;}else{need(at<records.size() && records[at].first==label,"vector test message order");out=records[at++].second;}
  transcript.bytes(label,out);return out;
 }
 Fr coin(const std::string&label)override{auto c=Fr(2+(ci++%59));transcript.scalar(label,c);return c;}
};
Fr evaluate(const VectorOracle&v,const std::vector<Fr>&q){auto w=r1::weights(q);Fr value=0;for(std::size_t i=0;i<v.padded;++i)value+=v.value(i)*w[i];return value;}
void query(IO&io,const VectorOracle&v,Random*rng,bool wrongClaim=false){
 std::vector<Fr>point;for(unsigned i=0;i<r1::log2exact(v.padded);++i)point.push_back(io.coin("vector/query/"+std::to_string(i)));
 auto value=io.prover()?evaluate(v,point)+Fr(wrongClaim?1:0):Fr(0);
 auto h=exchangeHidden(io,"vector/evaluation",value,rng);
 v.open(io,{point,h.handle},io.prover()?&h:nullptr,rng,"vector/open");
}
void replay(std::size_t n,const std::string&attack){
 OSRandom random;Tape p(true);auto actual=n==1?1:n-3;std::size_t reads=0,bound=0;
 auto source=[&](std::size_t i){++reads;return i<actual?Fr(1+i%17):Fr(1048576);};
 auto oracle=exchangeVector(p,"vectors/native-E",actual,n,source,&random,[&](const VectorPC&pc){++bound;need(pc.actual==actual && pc.padded==n,"vector bound shape");});
 need(reads==n && bound==1,"exchangeVector materializes all padded coordinates once");
 for(std::size_t i=actual;i<n;++i)need(oracle.value(i)==Fr(1048576),"nonzero native-E logical tail preserved");
 query(p,oracle,&random,attack=="claim");
 for(auto&r:p.records)need(r.first.find("/register/")==std::string::npos,"new vector opening does not mutate old registration flow");
 Tape v(false);v.records=p.records;
 if(attack=="identity"){auto pc=decodeVector(v.records.front().second);pc.id="vectors/substituted";v.records.front().second=encodeVector(pc);}
 if(attack=="truncated"){bool found=false;for(auto&r:v.records)if(r.first.find("A3/knowledge/responses")!=std::string::npos){need(!r.second.empty(),"truncation target exists");r.second.pop_back();found=true;break;}need(found,"test locates actual A3 response");}
 auto forbidden=[](std::size_t)->Fr{throw std::runtime_error("verifier attempted private vector read");};
 std::size_t verifierBound=0;auto publicOracle=exchangeVector(v,"vectors/native-E",actual,n,forbidden,nullptr,[&](const VectorPC&){++verifierBound;});
 query(v,publicOracle,nullptr,attack=="claim");
 need(v.at==v.records.size() && verifierBound==1,"independent vector verifier consumes all proof and commitment bytes");
 std::cout<<"PASS exchangeVector n="<<n<<" actual="<<actual<<" nonzero_tail="<<n-actual<<'\n';
}
void inconsistentRow(){
 // A canonical, correctly identified PC whose first row disagrees with the
 // prover's vector. The live A.3 knowledge equation must reject independently.
 OSRandom random;Tape p(true);auto secret=commitVector("vectors/bad-row",13,16,[](std::size_t i){return Fr(i+3);},random);
 secret.pc.points[0]+=parameters(1).scalar;
 p.message("bad-row/PC",encodeVector(secret.pc));std::vector<Fr>point;
 for(unsigned i=0;i<4;++i)point.push_back(p.coin("bad-row/query/"+std::to_string(i)));
 auto h=exchangeHidden(p,"bad-row/value",r1::mle(*secret.values,point),&random);
 openVector(p,secret.pc,{point,h.handle},&secret,&h,&random,"bad-row/open");
 Tape v(false);v.records=p.records;auto pc=decodeVector(v.message("bad-row/PC"));std::vector<Fr>vp;
 for(unsigned i=0;i<4;++i)vp.push_back(v.coin("bad-row/query/"+std::to_string(i)));
 auto vh=exchangeHidden(v,"bad-row/value",Fr(0),nullptr);openVector(v,pc,{vp,vh.handle},nullptr,nullptr,nullptr,"bad-row/open");
}
void ownedBuffer(){
 // A full slab copy here would regress the intended peak-memory bound.
 SessionRandom rng;auto values=std::make_shared<std::vector<Fr>>(16,Fr(1048576));
 for(std::size_t i=0;i<13;++i)values->at(i)=Fr(i+3);
 auto address=values->data();auto secret=commitVectorOwned("vectors/owned",13,16,std::move(values),rng);
 need(!values && secret.values->data()==address,"owned commitment copied a slab");
 for(std::size_t i=13;i<16;++i)need(secret.values->at(i)==Fr(1048576),"owned nonzero logical padding lost");
 Tape p(true);std::vector<Fr>q{Fr(3),Fr(5),Fr(7),Fr(11)};
 auto h=exchangeHidden(p,"owned/value",r1::mle(*secret.values,q),&rng);
 openVector(p,secret.pc,{q,h.handle},&secret,&h,&rng,"owned/open");
 Tape v(false);v.records=p.records;auto vh=exchangeHidden(v,"owned/value",Fr(0),nullptr);
 openVector(v,secret.pc,{q,vh.handle},nullptr,nullptr,nullptr,"owned/open");
 need(v.at==v.records.size(),"owned independent opening incomplete");
 bool rejected=false;try{commitVectorOwned("vectors/short",13,16,std::make_shared<std::vector<Fr>>(15),rng);}catch(...){rejected=true;}
 need(rejected,"owned malformed buffer accepted");
}
void msmSpan(){
 auto g=hyrax_a3::generators(16);std::vector<Fr>x(16);G1 expected;expected.clear();
 for(std::size_t i=0;i<x.size();++i){x[i]=Fr(i+3);if(i>1 && i<15)expected+=g[i]*x[i];}
 auto original=g;auto scalars=x;
 need(master3::masterMSM(g.data()+2,x.data()+2,13)==expected,"MSM span differs from independent scalar sum");
 need(g==original && x==scalars,"MSM changed caller buffers");
 bool rejected=false;try{master3::masterMSM(g.data(),x.data(),0);}catch(...){rejected=true;}
 need(rejected,"MSM empty span accepted");
}
void rejects(const std::function<void()>&f,const char*name,const char*why){try{f();}catch(const std::exception&e){need(std::string(e.what()).find(why)!=std::string::npos,e.what());std::cout<<"REJECT "<<name<<": "<<e.what()<<'\n';return;}throw std::runtime_error(std::string("expected vector rejection: ")+name);}
}
int main(){try{
 r1::init();msmSpan();ownedBuffer();for(auto n:{std::size_t(1),std::size_t(16),std::size_t(4096)})replay(n,"");
 rejects([]{replay(16,"identity");},"wrong PC identity","wrong object identity");
 rejects([]{inconsistentRow();},"wrong committed row","vector knowledge equation");
 rejects([]{replay(16,"claim");},"wrong hidden evaluation","Schnorr equation");
 rejects([]{replay(16,"truncated");},"truncated A3 response","truncated wire");
 std::cout<<"PASS real exchangeVector+A3 independent replay; old registration untouched\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
