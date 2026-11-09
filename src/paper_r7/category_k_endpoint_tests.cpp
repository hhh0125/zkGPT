#include "classified_commitments.hpp"
#include "fs_io.hpp"
#include <iostream>
#include <random>
using namespace zkgpt::r7;using namespace oneblock;using namespace classified;

namespace zkgpt::r7::oneblock::classified {
struct AuthenticationTestAccess {

 static void requireFast(const Commitments&pcs){need(pcs.auth_metrics_.endpoint_fast_coordinates>0,"real verifier did not use admitted endpoint");}
 static void endpoints(Commitments&pcs,Layout&layout){std::mt19937 random(20261108);
  auto chi=[](std::size_t z,const std::vector<Fr>&q){Fr w=1;for(std::size_t b=0;b<q.size();++b)w*=((z>>b)&1)?q[b]:Fr(1)-q[b];return w;};
  for(unsigned trial=0;trial<220;++trial){
   std::array<std::size_t,5>a{};for(auto&n:a)n=1+random()%256;if(trial<5)a={64,256,129,33,65};auto plan=single::makeCategoryPlan(a,std::string(64,'a'));const char*names[]{"X","W","Aux","LookupQueries","LookupHelpers"};
   layout.roots.clear();pcs.roots_.resize(5);for(auto s:plan.segments){unsigned k=0;while(s.source_id!=names[k])++k;layout.categories[k]={s.actual,s.padded,s.offset,s.physical_offset};}
   for(unsigned k=0;k<5;++k){RootSpec spec{names[k],a[k],a[k],k!=4,k==1,k,0};layout.roots.push_back(spec);pcs.roots_[k].spec=spec;}
   pcs.claims_.clear();pcs.names_.clear();std::vector<std::shared_ptr<std::vector<Located>>>maps;
   for(unsigned claim=0;claim<2;++claim){auto locations=std::make_shared<std::vector<Located>>(256);maps.push_back(locations);
    for(std::size_t j=0;j<256;++j){Located l;l.constant=true;l.bias=Fr(int(j%3)-1);unsigned root=1;std::size_t index=0;
     switch(trial%5){case 0:index=j;l.constant=j>=a[root];break;case 1:index=j<a[root]?a[root]-1-j:0;l.constant=j>=a[root];break;case 2:root=trial%5;index=std::min<std::size_t>(3,a[root]-1);l.constant=false;break;case 3:index=j/2;l.constant=(j%2 || index>=a[root]);break;default:root=(j/16)%5;index=j%a[root];l.constant=false;}
     l.coordinate={root,std::uint32_t(index)};(*locations)[j]=l;
    }
    Claim c;c.id="endpoint/"+std::to_string(trial)+"/"+std::to_string(claim);c.map_id=c.id;c.padded=256;for(unsigned j=0;j<8;++j)c.point.emplace_back(trial%3?int(random()%17)-8:int(random()%2));c.value=publicHidden(pcs.io_,c.id,Fr(0));c.map=[locations](std::size_t j){return locations->at(j);};c.map_digest=pcs.mapDigest(c);pcs.claims_.push_back(std::move(c));
   }
   Commitments::Authentication auth(2,false);for(unsigned j=0;j<2;++j)pcs.prepareClaim(pcs.claims_[j],auth,j);
   std::vector<Fr>rho{Fr(3),Fr(7)},q(single::bits(plan.domain));for(auto&v:q)v=trial%3?Fr(int(random()%19)-9):Fr(random()%2);
   Fr reference=0;for(unsigned i=0;i<2;++i)for(std::size_t j=0;j<256;++j){auto l=maps[i]->at(j);if(l.constant)continue;auto z=layout.categories[l.coordinate.root].logical_offset+l.coordinate.index;reference+=rho[i]*chi(j,pcs.claims_[i].point)*chi(z,q);}
   auto direct=pcs.categoryKernelEndpoint(q,rho,auth,false);need(direct && *direct==reference,"direct global K endpoint != independent source/global bit products");
   Fr dense=0;for(unsigned root=0;root<5;++root){auto values=pcs.categoryKernel(root,0,a[root],rho,auth);for(std::size_t j=0;j<values.size();++j)dense+=values[j]*chi(layout.categories[root].logical_offset+j,q);}need(dense==reference,"dense K reference mismatch");
   // The callback fallback must retain the exact post-rho map digest check.
   for(auto&p:auth.claims){p.frozen=false;p.runs.clear();p.disk_roots.clear();}
   direct=pcs.categoryKernelEndpoint(q,rho,auth,false);need(direct && *direct==reference,"fallback K endpoint differs");
   auto&location=maps[0]->at(0);location.bias+=Fr(1);bool rejected=false;try{pcs.categoryKernelEndpoint(q,rho,auth,false);}catch(const std::exception&e){rejected=std::string(e.what()).find("source map changed")!=std::string::npos;}need(rejected,"endpoint fallback ignored mutated map");
  }
  std::cout<<"CATEGORY_GLOBAL_K_ENDPOINT_220_STRIDES_STEPS_REPEATS_BIASES_CROSS_CATEGORY_AND_MAP_MUTATION_PASS\n";
 }

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
Layout largeFixture(){R7Context c;c.layout_sha256=std::string(64,'d');c.plan.master=r1::PCSpec::make("master",4096,4096,r1::Layout::RowMajor);
 c.plan.sources.push_back({r1::PCSpec::make("query",64,64,r1::Layout::RowMajor),{{2240,64,Fr(2)}}});c.plan.sources.push_back({r1::PCSpec::make("W",2048,2048,r1::Layout::RowMajor),{{128,2048,Fr(0)}}});
 c.objects={{"base","",ObjectKind::BaseMaster,4096,4096,{},false},{"query","base",ObjectKind::MappedChild,64,64,c.plan.sources[0].spans,false},{"W","base",ObjectKind::ModelW,2048,2048,c.plan.sources[1].spans,false},{"helper","",ObjectKind::ExpHelper,2047,2048,{},false}};
 return categoryLayout(c,{{2,64,Fr(0)}},256);
}
void largeRun(Tape&io,const Layout&layout,SessionRandom*rng,bool require_fast){Prepared prepared;auto native=std::make_shared<std::vector<Fr>>(4096);for(std::size_t j=0;j<native->size();++j)native->at(j)=Fr(j+1);Value base;if(io.prover()){base=[native](std::size_t j){return native->at(j);};prepared["helper"]=std::make_shared<std::vector<Fr>>(2048,Fr(13));prepared["helper"]->back()=Fr(999);}
 std::vector<Fr>x;for(std::size_t j=2;j<66;++j)x.push_back(native->at(j));Commitments pcs(io,layout,base,io.prover()?&prepared:nullptr,rng,x);pcs.constrainBaseTails();
 for(auto id:{"W","query","helper"}){auto source=pcs.view(id);std::vector<Fr>point;for(unsigned j=0;j<r1::log2exact(source.padded);++j)point.push_back(io.coin(std::string("large/")+id+"/point/"+std::to_string(j)));Fr y=0;if(io.prover()){std::vector<Fr>values(source.padded);for(std::size_t j=0;j<values.size();++j)values[j]=source.value(j);y=r1::mle(values,point);}auto h=exchangeHidden(io,std::string("large/")+id,y,rng);source.open(io,{point,h.handle},io.prover()?&h:nullptr,rng,std::string("large/")+id+"/opening");}
 pcs.authenticateUnifiedInput(x);need(pcs.authenticated(),"large endpoint protocol not authenticated");if(!io.prover() && require_fast)AuthenticationTestAccess::requireFast(pcs);
}

}
int main(){try{
 r1::init();SessionRandom rng;auto l=fixture();
 {auto large=largeFixture();Tape producer(true);largeRun(producer,large,&rng,false);setenv("ZKGPT_CATEGORY_K_ENDPOINT","0",1);Tape dense(false);dense.data=producer.data;largeRun(dense,large,nullptr,false);setenv("ZKGPT_CATEGORY_K_ENDPOINT","1",1);Tape direct(false);direct.data=producer.data;largeRun(direct,large,nullptr,true);need(dense.at==direct.at && direct.at==producer.data.size() && dense.prefix()==direct.prefix(),"endpoint changed proof framing or challenges");unsetenv("ZKGPT_CATEGORY_K_ENDPOINT");}

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
 
 {Tape io(true);Prepared helpers;helpers["helper"]=std::make_shared<std::vector<Fr>>(4,Fr(2));std::vector<Fr>x{Fr(3),Fr(4),Fr(5)};Commitments pcs(io,l,[](std::size_t j){return Fr(j+1);},&helpers,&rng,x);pcs.constrainBaseTails();AuthenticationTestAccess::endpoints(pcs,l);}
 std::cout<<"CATEGORY_CRYPTO_TESTED X_algebraic_negatives=3 verifier_mutations=5\n";return 0;
}catch(const std::exception&e){std::cerr<<"CATEGORY_CRYPTO_FAILED "<<e.what()<<'\n';return 1;}}
