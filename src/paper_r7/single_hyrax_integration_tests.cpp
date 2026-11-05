#include "classified_commitments.hpp"
#include "fs_io.hpp"
#include <algorithm>
#include <iostream>
using namespace zkgpt::r7;using namespace oneblock;using namespace classified;
namespace {
struct Tape:IO {
 bool p;std::size_t at=0;std::vector<std::pair<std::string,Bytes>>data;
 fs::CanonicalTranscript transcript{{3,1,4},{1,5,9}};
 explicit Tape(bool b):p(b){}bool prover()const override{return p;}
 std::string session()const override{return std::string(64,'d');}Digest prefix()const override{return transcript.prefix();}
 Bytes message(const std::string&n,const Bytes&b={})override{Bytes out=b;if(p)data.emplace_back(n,b);else{need(at<data.size() && data[at].first==n,"single integrated frame order/EOF");out=data[at++].second;}transcript.message(n,out);return out;}
 Fr coin(const std::string&n)override{return transcript.coin(n);}
};
template<class F>void rejects(F&&f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}need(failed,"single integrated verifier accepted malformed relation");}
Layout fixture(){
 Layout p;p.native=std::string(64,'a');p.roots={{"C_W",3,4,true,true},{"C_aux",5,8,true,false},{"C_lookup/helpers/0",8,8,false,false}};
 for(unsigned j=0;j<8;++j)p.base.push_back(j<3?Coordinate{0,j}:Coordinate{1,j-3});
 View w;w.id="r7/W";w.actual=3;w.padded=4;w.mapped=true;w.spans={{0,3,Fr(0)}};w.kind=ObjectKind::ModelW;p.views.emplace(w.id,w);
 View v;v.id="r7/helper";v.actual=5;v.padded=8;v.start={2,0};v.kind=ObjectKind::ExpHelper;p.views.emplace(v.id,v);return p;
}
Layout partitionedFixture(){
 // Native W appears at indices 4,5,1, while logical W order is 5,6,2.
 // Helper storage is split across two source segments; its logical padded
 // cells are deliberately nonzero and must survive without sanitization.
 Layout p;p.native=std::string(64,'a');p.master_actual=8;
 p.roots={{"C_W/chunk/0",2,2,true,true},{"C_W/chunk/1",1,1,true,true},{"C_aux",5,8,true,false},{"C_lookup/helpers/0",4,4,false,false},{"C_lookup/helpers/1",4,4,false,false}};
 p.base_runs={{0,1,{2,0}},{1,1,{1,0}},{2,2,{2,1}},{4,2,{0,0}},{6,2,{2,3}}};
 View w;w.id="r7/W";w.actual=3;w.padded=4;w.mapped=true;w.spans={{4,2,Fr(0)},{1,1,Fr(0)}};w.kind=ObjectKind::ModelW;p.views.emplace(w.id,w);
 View h;h.id="r7/helper";h.actual=5;h.padded=8;h.start={3,0};h.pieces={{0,4,{3,0}},{4,4,{4,0}}};h.kind=ObjectKind::ExpHelper;p.views.emplace(h.id,h);return p;
}
void run(IO&io,const Layout&l,SessionRandom*rng,unsigned attack=0){
 Prepared prepared;if(io.prover())prepared["r7/helper"]=std::make_shared<std::vector<Fr>>(std::initializer_list<Fr>{Fr(2),Fr(3),Fr(4),Fr(5),Fr(6),Fr(7),Fr(8),Fr(9)});
 auto x=std::vector<Fr>{Fr(1)};if(attack==4 || attack==5)x[0]+=Fr(1);
 Value base;if(io.prover())base=[](std::size_t i){return Fr(i+1);};
 Commitments pcs(io,l,base,io.prover()?&prepared:nullptr,rng,x);need(pcs.logicalCommitments()==1,"single integrated commitment count");
 pcs.constrainBaseTails();
 const std::vector<Fr>q{Fr(2),Fr(3)};
 for(auto id:{"r7/W","r7/helper"}){auto v=pcs.view(id);std::vector<Fr>point=q;point.resize(r1::log2exact(v.padded),Fr(5));Fr y=0;if(io.prover()){std::vector<Fr>values(v.padded);for(std::size_t j=0;j<v.padded;++j)values[j]=v.value(j);
   auto expected=std::string(id)=="r7/helper"?std::vector<Fr>{Fr(2),Fr(3),Fr(4),Fr(5),Fr(6),Fr(7),Fr(8),Fr(9)}:(l.master_actual?std::vector<Fr>{Fr(5),Fr(6),Fr(2),Fr(0)}:std::vector<Fr>{Fr(1),Fr(2),Fr(3),Fr(0)});
   if(attack==5 && !l.master_actual && std::string(id)=="r7/W")expected[0]=Fr(2);
   need(values==expected,"single native W order/helper pieces changed witness values");y=r1::mle(values,point);}
  auto h=exchangeHidden(io,std::string(id)+"/value",y,rng);v.open(io,{point,h.handle},io.prover()?&h:nullptr,rng,std::string(id)+"/claim");}
 // Literal native values pin GKR and Lasso's cross-source coordinate mapping.
 auto gkr=exchangeHidden(io,"gkr/terminal",io.prover()?r1::mle({Fr(1),Fr(2),Fr(4),Fr(5)},q):Fr(0),rng);
 pcs.deferBase("gkr/terminal",4,4,[attack](std::size_t j){const std::size_t indices[]{0,1,3,4};return indices[(j+(attack==1?1:0))%4];},gkr,q);
 auto lookup=exchangeHidden(io,"lasso/source",io.prover()?r1::mle({Fr(2),Fr(3),Fr(4),Fr(5)},q):Fr(0),rng);
 const auto helper_root=std::uint32_t(l.master_actual?3:2);
 pcs.defer({"lasso/source","trusted/lasso-map",lookup,q,4,[attack,helper_root](std::size_t j){return Located{{helper_root,std::uint32_t((j+(attack==2?1:0))%4)},Fr(0),false};}});
 auto output=publicHidden(io,"public/Y",Fr(8+(attack==3?1:0)));
 pcs.deferBase("public/Y",1,1,[](std::size_t){return std::size_t(7);},output,{});
 pcs.authenticateUnifiedInput(x);need(pcs.authenticated(),"single unified input incomplete");
}
}
int main(){try{r1::init();SessionRandom rng;for(bool partitioned:{false,true}){auto l=partitioned?partitionedFixture():fixture();Tape p(true);run(p,l,&rng);Tape v(false);v.data=p.data;run(v,l,nullptr);need(v.at==v.data.size(),"single integrated trailing frames");
 std::size_t headers=0;for(auto&m:p.data){headers+=m.first=="SingleHyrax/header";need(m.first.find("Classified/root/")==std::string::npos && m.first.find("UnifiedInput/opening/root/")==std::string::npos && m.first.find("fixed-cells")==std::string::npos,"single path emitted independent root PC/opening or obsolete fixed-cell proof");}need(headers==1,"single header count");
 for(unsigned attack=1;attack<=4;++attack){Tape bad(false);bad.data=p.data;rejects([&]{run(bad,l,nullptr,attack);});}
 // Both peers agree on the changed map/Y declaration. Rejection must then be
 // the actual algebraic consistency equation, rather than statement mismatch.
 // The prover's shared Sigma code also checks this equation, so retain the
 // partial transcript through that response and replay it in a witness-free V.
 for(unsigned attack:{1u,2u,3u,5u}){Tape malicious(true);bool stopped=false;try{run(malicious,l,&rng,attack);}catch(const std::exception&e){need(std::string(e.what()).find("Sigma compressed linear relation failed")!=std::string::npos,e.what());stopped=true;}need(stopped,"malicious integrated claim did not reach expected Sigma failure");
  Tape judge(false);judge.data=malicious.data;bool rejected=false;try{run(judge,l,nullptr,attack);}catch(const std::exception&e){need(std::string(e.what()).find("Sigma compressed linear relation failed")!=std::string::npos,e.what());rejected=true;}need(rejected,"real verifier accepted consistently declared wrong map/Y");}
 for(unsigned attack=0;attack<3;++attack){Tape bad(false);bad.data=p.data;if(attack==0)std::swap(bad.data[0],bad.data[1]);if(attack==1)bad.data.pop_back();if(attack==2)bad.data[0].first="Classified/root/0/PC";rejects([&]{run(bad,l,nullptr);});}
 }
 std::cout<<"SINGLE_HYRAX_INTEGRATION_TESTED\n";return 0;
}catch(const std::exception&e){std::cerr<<"SINGLE_HYRAX_INTEGRATION_FAILED "<<e.what()<<'\n';return 1;}}
