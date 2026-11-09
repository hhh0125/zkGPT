#include "classified_commitments.hpp"
#include "fs_io.hpp"
#include <iostream>
using namespace zkgpt::r7;using namespace oneblock;using namespace classified;

namespace zkgpt::r7::oneblock::classified {
struct AuthenticationTestAccess {
 static void mutableFallback(Commitments&pcs,const std::shared_ptr<unsigned>&calls){
  auto&claim=pcs.claims_.back();Commitments::Authentication auth(pcs.claims_.size(),false);*calls=0;
  pcs.prepareClaim(claim,auth,pcs.claims_.size()-1);
  auto&prepared=auth.claims.back();prepared.frozen=false;prepared.runs.clear();prepared.disk_roots.clear();
  *calls=0;bool rejected=false;std::vector<Fr>kernel;try{kernel=pcs.categoryKernel(1,0,4,std::vector<Fr>(pcs.claims_.size(),Fr(1)),auth);}catch(const std::exception&e){rejected=std::string(e.what()).find("source map changed")!=std::string::npos;}
  need(rejected || kernel==r1::weights(claim.point),"category fallback consumed altered map after a canonical precheck");
 }
};
}

namespace {
struct Tape:IO{
 bool p;std::size_t at=0;std::vector<std::pair<std::string,Bytes>>data;
 fs::CanonicalTranscript transcript{{3,1,4},{1,5,9}};
 explicit Tape(bool prover):p(prover){}
 bool prover()const override{return p;}
 std::string session()const override{return std::string(64,'a');}
 Digest prefix()const override{return transcript.prefix();}
 Bytes message(const std::string&n,const Bytes&b={})override{Bytes out=b;if(p)data.emplace_back(n,b);else{need(at<data.size() && data[at].first==n,"category test frame order/EOF");out=data[at++].second;}transcript.message(n,out);return out;}
 Fr coin(const std::string&n)override{return transcript.coin(n);}
};
Layout fixture(){
 R7Context c;c.layout_sha256=std::string(64,'c');c.plan.master=r1::PCSpec::make("master",24,32,r1::Layout::RowMajor);
 c.plan.sources.push_back({r1::PCSpec::make("query",9,16,r1::Layout::RowMajor),{{1,7,Fr(2)},{12,2,Fr(-1)}}});
 c.plan.sources.push_back({r1::PCSpec::make("W",5,8,r1::Layout::RowMajor),{{9,3,Fr(0)},{21,2,Fr(0)}}});
 c.objects={{"base","",ObjectKind::BaseMaster,24,32,{},false},{"query","base",ObjectKind::MappedChild,9,16,c.plan.sources[0].spans,false},{"W","base",ObjectKind::ModelW,5,8,c.plan.sources[1].spans,false},{"helper","",ObjectKind::ExpHelper,3,4,{},false}};
 return categoryLayout(c,{{2,3,Fr(0)}},4);
}
void run(Tape&io,const Layout&l,SessionRandom*rng,unsigned attack=0){
 Prepared helpers;if(io.prover())helpers["helper"]=std::make_shared<std::vector<Fr>>(std::initializer_list<Fr>{Fr(101),Fr(102),Fr(103),Fr(999)});
 auto native=std::make_shared<std::vector<Fr>>(24);for(std::size_t j=0;j<native->size();++j)native->at(j)=Fr(j+1);
 if(attack==1)native->at(2)+=Fr(1);
 Value base;if(io.prover())base=[native](std::size_t j){return native->at(j);};
 std::vector<Fr>x{Fr(3),Fr(4),Fr(5)};if(attack==2)x[0]+=Fr(1);
 need(unifiedInputIdentity(l,x).size()==64,"category independent FS binding");
 Commitments pcs(io,l,base,io.prover()?&helpers:nullptr,rng,x);pcs.constrainBaseTails();
 if(io.prover() && attack==3)native->at(2)+=Fr(1);
 need(pcs.singlePublic().plan.category_mode,"category initialization must use original X, not legacy public replacement");
 for(auto id:{"W","query","helper"}){
  auto v=pcs.view(id);std::vector<Fr>q(r1::log2exact(v.padded),Fr(7));Fr y=0;
  if(io.prover()){
   if(std::string(id)=="helper")need(v.value(3)==Fr(999),"nonzero materialized helper padding lost");
   std::vector<Fr>values(v.padded);for(std::size_t j=0;j<v.padded;++j)values[j]=v.value(j);y=r1::mle(values,q);
  }
  auto h=exchangeHidden(io,std::string("source/")+id,y,rng);v.open(io,{q,h.handle},io.prover()?&h:nullptr,rng,std::string("source/")+id+"/opening");
 }
 pcs.authenticateUnifiedInput(x);need(pcs.authenticated(),"category authentication mandatory");
}
}
int main(){try{
 r1::init();SessionRandom rng;auto l=fixture();
 Tape p(true);run(p,l,&rng);Tape v(false);v.data=p.data;run(v,l,nullptr);need(v.at==v.data.size(),"category trailing frames");
 // Both peers use the same well-formed public Statement. Only private X differs
 // (attack 1), or both agree on a public X inconsistent with the committed input
 // (attack 2). Neither attack can be rejected just by different frame headers.
 for(unsigned attack:{1u,2u,3u}){
  Tape malicious(true);bool rejected=false;try{run(malicious,l,&rng,attack);}catch(const std::exception&e){need(std::string(e.what()).find("Sigma compressed linear relation failed")!=std::string::npos,"X attack failed before algebraic verification");std::cerr<<"CATEGORY_X_P_REJECT "<<attack<<" "<<e.what()<<'\n';rejected=true;}
  need(rejected,"category accepted inconsistent original/statement X");
  Tape judge(false);judge.data=malicious.data;rejected=false;try{run(judge,l,nullptr,attack);}catch(const std::exception&e){need(std::string(e.what()).find("Sigma compressed linear relation failed")!=std::string::npos,"independent X attack did not reach Sigma verification");std::cerr<<"CATEGORY_X_V_REJECT "<<attack<<" "<<e.what()<<'\n';rejected=true;}
  need(rejected,"independent witness-free verifier accepted inconsistent X");
 }
 
 // Mandatory X claim: removing its declaration is rejected by the actual
 // verifier's protocol-completeness path; it cannot mark public/X verified.
 for(unsigned attack=0;attack<5;++attack){
  Tape bad(false);bad.data=p.data;bool target=false;
  for(auto it=bad.data.begin();it!=bad.data.end();++it){
   if(attack==0 && it->first=="Classified/defer/3"){bad.data.erase(it);target=true;break;}
   if(attack==1 && it->first=="Classified/defer/3"){it->second.back()^=1;target=true;break;}
   if(attack==2 && it->first=="CategoryInput/X-statement-and-seal"){it->second.back()^=1;target=true;break;}
   if(attack==3 && it->first=="UnifiedInput/sumcheck/input-evaluation/commitment"){auto h=decodeHandle(it->second);h.point+=parameters(1).scalar;it->second=encode(h);target=true;break;}
   if(attack==4 && it->first=="SingleHyrax/rows/0"){Reader r{it->second};auto count=r.number();Writer w;w.number(count);for(std::size_t j=0;j<count;++j){auto point=r.point();if(j==0)point+=parameters(1).scalar;w.point(point);}it->second=w.b;target=true;break;}
  }
  need(target,"category mutation missed intended verification frame");bool rejected=false;try{run(bad,l,nullptr);}catch(const std::exception&e){std::cerr<<"CATEGORY_MUTATION_V_REJECT "<<attack<<" "<<e.what()<<'\n';rejected=true;}need(rejected,"category verifier accepted omitted/modified X claim/binding/handle/commitment");
 }
 // P's source map cannot override V's independently rebuilt original layout.
 auto wrong=l;wrong.x_spans[0].offset+=1;Tape wrongp(true);bool rejected=false;try{run(wrongp,wrong,&rng);}catch(const std::exception&){rejected=true;}
 Tape correctv(false);correctv.data=wrongp.data;rejected=false;try{run(correctv,l,nullptr);}catch(const std::exception&e){std::cerr<<"CATEGORY_SOURCE_MAP_V_REJECT "<<e.what()<<'\n';rejected=true;}need(rejected,"verifier accepted prover-selected X map");
 auto duplicate=l;duplicate.categories[0].actual++;duplicate.categories[0].padded=4;Tape badp(true);rejected=false;try{run(badp,duplicate,&rng);}catch(const std::exception&e){std::cerr<<"CATEGORY_DUPLICATE_X_REJECT "<<e.what()<<'\n';rejected=true;}need(rejected,"duplicated X storage accepted");
 
 {
  Tape io(true);Prepared helpers;helpers["helper"]=std::make_shared<std::vector<Fr>>(4,Fr(2));std::vector<Fr>x{Fr(3),Fr(4),Fr(5)};
  Commitments pcs(io,l,[](std::size_t j){return Fr(j+1);},&helpers,&rng,x);pcs.constrainBaseTails();
  auto calls=std::make_shared<unsigned>(0);auto h=publicHidden(io,"mutable/handle",Fr(0));auto root=std::uint32_t(1);
  pcs.defer({"mutable","mutable",h,{Fr(2),Fr(3)},4,[calls,root](std::size_t j){bool alternate=((*calls)++/4)%2;return Located{{root,std::uint32_t(j^(alternate?1:0))},Fr(0),false};}});
  AuthenticationTestAccess::mutableFallback(pcs,calls);
 }
 std::cout<<"CATEGORY_CRYPTO_TESTED X_algebraic_negatives=3 verifier_mutations=5\n";return 0;
}catch(const std::exception&e){std::cerr<<"CATEGORY_CRYPTO_FAILED "<<e.what()<<'\n';return 1;}}
