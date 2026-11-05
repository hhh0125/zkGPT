#include "oneblock_vectors.hpp"
#include <iostream>
#include <functional>
using namespace zkgpt::r7;
using namespace zkgpt::r7::crypto;
using namespace zkgpt::r7::oneblock;
namespace {
struct Tape final:IO {
 bool p;std::size_t at=0,ci=0;int changed=-1;
 std::vector<std::pair<std::string,Bytes>>records;
 r1::Fs fs{"R7-BALANCED-HYRAX-TEST-v1"};
 explicit Tape(bool prover):p(prover){}
 bool prover()const override{return p;}
 std::string session()const override{return std::string(64,'b');}
 Digest prefix()const override{return fs.digest();}
 Bytes message(const std::string&label,const Bytes&b={})override{
  Bytes out;if(p){records.emplace_back(label,b);out=b;}else{need(at<records.size() && records[at].first==label,"balanced replay order");out=records[at++].second;}
  fs.bytes(label,out);return out;
 }
 Fr coin(const std::string&label)override{auto c=Fr(2+ci%59+(int(ci)==changed?1:0));++ci;fs.scalar(label,c);return c;}
};
void reject(const std::function<void()>&f,const std::string&name){try{f();}catch(const std::exception&e){std::cout<<"REJECT "<<name<<": "<<e.what()<<'\n';return;}throw std::runtime_error("accepted "+name);}
void shapes(){
 struct Case{std::size_t padded,rows,columns;};
 for(auto c:{Case{1,1,1},Case{2,1,2},Case{8,2,4},Case{2048,32,64},Case{1u<<20,1024,1024},Case{1u<<21,1024,2048},Case{1u<<22,2048,2048},Case{1u<<23,2048,4096},Case{1u<<24,4096,4096}}){
  VectorPC pc;pc.id="balanced/shape";pc.layout=r1::Layout::RowMajor;pc.actual=c.padded==1?1:c.padded/2+1;pc.padded=c.padded;pc.rows=c.rows;pc.columns=c.columns;pc.points.resize(pc.rows);for(auto&p:pc.points)p.clear();
  auto raw=encodeVector(pc);auto decoded=decodeVector(raw);need(decoded.rows==c.rows && decoded.columns==c.columns,"balanced shape roundtrip");
  auto wrong=pc;wrong.rows*=2;wrong.columns=std::max<std::size_t>(1,wrong.columns/2);wrong.points.resize(wrong.rows);reject([&]{encodeVector(wrong);},"noncanonical matrix");
  Writer legacy;legacy.text("R7-ONEBLOCK-HIDING-VECTOR-PC-ROWMAJOR-v2");legacy.text(pc.id);legacy.text("hyrax-row-major-low-column-bits-v1");legacy.number(unsigned(pc.layout));for(auto n:{pc.actual,pc.padded,pc.rows,pc.columns})legacy.number(n,8);for(auto&p:pc.points)legacy.point(p);reject([&]{decodeVector(legacy.b);},"old ordinary wire suite");
  std::cout<<"SHAPE "<<c.padded<<' '<<decoded.rows<<' '<<decoded.columns<<'\n';
 }
 SessionRandom rng;
 for(auto n:{0u,3u,(1u<<24)+1})reject([&]{commitVectorOwned("bad",1,n,std::make_shared<std::vector<Fr>>(),rng);},"invalid padded before matrix allocation");
 std::size_t reads=0;
 reject([&]{commitVector("invalid/read",1,3,[&](std::size_t){++reads;return Fr(0);},rng);},"invalid padded callback");
 need(reads==0,"invalid padded accessed witness before shape validation");
}
Fr matrixMLE(const std::vector<Fr>&x,const std::vector<Fr>&q,std::size_t columns){
 auto d=r1::log2exact(columns);auto a=r1::weights({q.begin(),q.begin()+d});auto L=r1::weights({q.begin()+d,q.end()});Fr out=0;
 for(std::size_t row=0;row<L.size();++row){Fr dot=0;for(std::size_t col=0;col<columns;++col)dot+=a[col]*x[row*columns+col];out+=L[row]*dot;}return out;
}
void mle(){
 SessionRandom rng;
 for(bool zero:{false,true})for(auto n:{2048u,8192u,16384u}){
  auto actual=n-5;auto s=commitVector("balanced/MLE",actual,n,[&](std::size_t i){return i<actual?Fr(3+i%19):Fr(zero?0:1048576);},rng);
  const auto want=n==2048?64u:128u;need(s.pc.columns==want && s.pc.rows==n/want,"commit shape uses padded not actual");
  for(bool boolean:{false,true}){
   std::vector<Fr>q;for(unsigned j=0;j<r1::log2exact(n);++j)q.push_back(boolean?Fr(j%2):rng.scalar());
   auto flat=r1::mle(*s.values,q);need(flat==matrixMLE(*s.values,q,1024) && flat==matrixMLE(*s.values,q,s.pc.columns),"old/new RowMajor MLE mismatch");
   Tape p(true);auto h=exchangeHidden(p,"balanced/evaluation",flat,&rng);openVector(p,s.pc,{q,h.handle},&s,&h,&rng,"balanced/open");
   Tape v(false);v.records=p.records;auto vh=exchangeHidden(v,"balanced/evaluation",Fr(0),nullptr);openVector(v,s.pc,{q,vh.handle},nullptr,nullptr,nullptr,"balanced/open");need(v.at==v.records.size(),"balanced opening incomplete");
   auto badq=q;badq[0]+=Fr(1);Tape wrong(false);wrong.records=p.records;auto wh=exchangeHidden(wrong,"balanced/evaluation",Fr(0),nullptr);reject([&]{openVector(wrong,s.pc,{badq,wh.handle},nullptr,nullptr,nullptr,"balanced/open");},"changed opening point");
  }
 }
 std::cout<<"MLE_OLD_NEW_IDENTICAL_AND_OPENINGS_ACCEPT\n";
}
void largeA3(){
 SessionRandom rng;
 auto small=hyrax_a3::generators(1024);
 for(auto n:{2048u,4096u}){
  auto g=hyrax_a3::generators(n);need(g.size()==n && std::equal(small.begin(),small.end(),g.begin()),"ordinary generator prefix changed");
  G1 expected;mcl::bn::hashAndMapToG1(expected,std::string(crypto::suite)+"/mcl-hashAndMapToG1/MCL_MAP_TO_MODE_ORIGINAL/column/"+std::to_string(n-1));need(g.back()==expected,"ordinary generator derivation changed");
  std::vector<Fr>a(n);hyrax_a3::DotWitness w;w.values.resize(n);w.blind=rng.scalar();for(unsigned i=0;i<n;++i){a[i]=Fr(1+i%7);w.values[i]=rng.scalar();}
  w.evaluation.value=r1::dot(w.values,a);w.evaluation.blind=rng.scalar();w.evaluation.handle.session=std::string(64,'b');w.evaluation.handle.role="balanced/dot";w.evaluation.handle.point=scalarCommit(w.evaluation.value,w.evaluation.blind);auto xi=hyrax_a3::vectorCommit(w.values,w.blind);
  Tape p(true);hyrax_a3::dotProduct(p,xi,w.evaluation.handle,a,&w,&rng);Tape v(false);v.records=p.records;hyrax_a3::dotProduct(v,xi,w.evaluation.handle,a,nullptr,nullptr);need(v.at==v.records.size(),"large A3 incomplete independent verifier");
  need(p.ci==r1::log2exact(n)+2,"large A3 knowledge/core challenge coverage");
  for(const auto&part:{std::string("knowledge/responses"),std::string("final/z1-z2"),std::string("reduce/0/cross")}){
   Tape bad(false);bad.records=p.records;bool found=false;for(auto&r:bad.records)if(r.first.find(part)!=std::string::npos){r.second.back()^=1;found=true;break;}need(found,"mutation target absent");reject([&]{hyrax_a3::dotProduct(bad,xi,w.evaluation.handle,a,nullptr,nullptr);},"large A3 "+part);
  }
  for(int at:{0,1,int(p.ci-1)}){Tape bad(false);bad.records=p.records;bad.changed=at;reject([&]{hyrax_a3::dotProduct(bad,xi,w.evaluation.handle,a,nullptr,nullptr);},"large changed challenge");}
  Tape badxi(false);badxi.records=p.records;reject([&]{hyrax_a3::dotProduct(badxi,xi+parameters(1).scalar,w.evaluation.handle,a,nullptr,nullptr);},"large changed commitment");
  Tape badtau(false);badtau.records=p.records;auto tau=w.evaluation.handle;tau.point+=parameters(1).scalar;reject([&]{hyrax_a3::dotProduct(badtau,xi,tau,a,nullptr,nullptr);},"large changed evaluation");
  std::cout<<"A3_DIMENSION_PASS "<<n<<" knowledge_and_inner_product\n";
 }
 reject([]{hyrax_a3::generators(131072);},"overbound A3");reject([]{hyrax_a3::generators(3072);},"nonpower A3");
}
}
int main(int argc,char**argv){try{r1::init();std::string mode=argc>1?argv[1]:"all";if(mode=="all"||mode=="shape")shapes();if(mode=="all"||mode=="a3")largeA3();if(mode=="all"||mode=="mle")mle();std::cout<<"BALANCED_HYRAX_PASS "<<mode<<'\n';return 0;}catch(const std::exception&e){std::cerr<<"BALANCED_HYRAX_FAIL "<<e.what()<<'\n';return 1;}}
