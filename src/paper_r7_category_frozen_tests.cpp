#include "paper_r7_classified_commitments.hpp"
#include "paper_r7_fs_io.hpp"
#include <iostream>
using namespace zkgpt::r7;
using namespace crypto;
namespace cl=oneblock::classified;
namespace zkgpt::r7::oneblock::classified {
struct AuthenticationTestAccess {
 static void budget(Commitments&c,std::size_t ram,std::size_t file){need(c.claims_.empty(),"test budget before claims");c.category_auth_=std::make_unique<Commitments::Authentication>(0,ram,0,file);c.category_auth_->session=c.session_;c.category_auth_->transcript=c.io_.transcriptOwner();}
 static void add(Commitments&c,Claim claim){c.deferTrusted(std::move(claim));}
 static const mapping::Plan&plan(const Commitments&c,std::size_t i){return c.category_auth_->claims.at(i);}
 static Fr bias(Commitments&c,std::size_t i){return c.prepareClaim(c.claims_.at(i),*c.category_auth_,i);}
 static std::vector<Fr>k(Commitments&c,std::size_t root,const std::vector<Fr>&rho){return c.categoryKernel(root,0,c.roots_[root].spec.actual,rho,*c.category_auth_);}
 static std::optional<Fr>endpoint(Commitments&c,const std::vector<Fr>&q,const std::vector<Fr>&rho,bool profitable){return c.categoryKernelEndpoint(q,rho,*c.category_auth_,profitable);}
 static void check(Commitments&c,std::size_t i,std::size_t n){const auto&p=c.category_auth_->claims.at(i);need(p.frozen && p.ready && p.callback_calls==n && p.full_scans==1 && !c.claims_.at(i).map,"test single scan and released callback");}
 static void invalidate(Commitments&c){c.category_auth_->claims[0].frozen=false;}
 static void wrongSession(Commitments&c){c.category_auth_->session="other";}
};
}
namespace {
using Access=cl::AuthenticationTestAccess;
template<class F>std::string rejects(F f,const std::string&reason){try{f();}catch(const std::exception&e){need(std::string(e.what()).find(reason)!=std::string::npos,e.what());return e.what();}throw std::runtime_error("expected rejection: "+reason);}
class LocalIO:public IO {
 public:std::vector<std::string>labels;bool prover()const override{return true;}std::string session()const override{return std::string(64,'a');}Digest prefix()const override{return {};}
 Bytes message(const std::string&s,const Bytes&b={})override{labels.push_back(s);return b;}Fr coin(const std::string&s)override{labels.push_back(s);return Fr(2);}
};
cl::Layout layout(){cl::Layout l;l.category_mode=true;l.native="frozen-mapping-fixture";l.categories[0]={13,16,0,0};l.categories[1]={26,32,16,16};l.master_actual=39;l.x_spans={{0,13,Fr(0)}};
 for(unsigned k=0;k<3;++k){l.roots.push_back({"root/"+std::to_string(k),13,13,true,false,k?1u:0u,k==2?13u:0u});l.base_runs.push_back({k*13,13,{k,0}});}return l;}
struct Fixture {LocalIO io;OSRandom rng;cl::Layout l=layout();cl::Prepared prepared;std::vector<Fr>x;std::unique_ptr<cl::Commitments>c;
 Fixture(){for(unsigned j=0;j<13;++j)x.emplace_back(j+1);c=std::make_unique<cl::Commitments>(io,l,[](std::size_t j){return Fr(j+1);},&prepared,&rng,x);c->constrainBaseTails();}
};
cl::Located mapped(std::size_t j){
 if(j>=117)return {{},Fr(3),true};
 if(j<16)return {{0,std::uint32_t(j%13)},Fr(j%7),false};
 if(j<32)return {{1,std::uint32_t(12-j%13)},Fr(j%5),false};
 if(j<48)return {{2,4},Fr(0),false};
 if(j%11==0)return {{},Fr(5),true};
 return {{std::uint32_t(j%3),std::uint32_t((j*3)%13)},Fr(1),false};
}
cl::Claim claim(Fixture&f,std::string id,std::size_t n,std::function<cl::Located(std::size_t)>map){return {id,"test-trusted-local-rule",oneblock::publicHidden(f.io,id+"/value",Fr(0)),std::vector<Fr>(r1::log2exact(n),Fr(2)),n,std::move(map)};}
void compare(bool file){Fixture f;Access::budget(*f.c,file?480:1u<<20,4u<<20);std::size_t calls=0;bool changed=false;auto c=claim(f,"complex",128,[&](std::size_t j){++calls;return changed?cl::Located{{2,12},Fr(99),false}:mapped(j);});
 auto weights=r1::weights(c.point);Fr bias=0;std::vector<std::vector<Fr>>expected(3,std::vector<Fr>(13,Fr(0)));for(std::size_t j=0;j<c.padded;++j){auto l=mapped(j);bias+=weights[j]*l.bias;if(!l.constant)expected[l.coordinate.root][l.coordinate.index]+=Fr(7)*weights[j];}
 Access::add(*f.c,std::move(c));need(calls==128,"test exact initial callback count");Access::check(*f.c,0,128);need(std::string(Access::plan(*f.c,0).reason)==(file?"FROZEN_ROOT_FILE":"FROZEN_ROOT_RAM"),"test storage path");
 changed=true;need(Access::bias(*f.c,0)==bias,"test frozen bias");for(unsigned k=0;k<3;++k)need(Access::k(*f.c,k,{Fr(7)})==expected[k],"test K coefficient equality");
 std::vector<Fr>q(6,Fr(3));auto global=r1::weights(q);Fr endpoint=0;for(unsigned k=0;k<3;++k)for(unsigned j=0;j<13;++j)endpoint+=expected[k][j]*global[(k==0?0:k==1?16:29)+j];
 need(*Access::endpoint(*f.c,q,{Fr(7)},false)==endpoint,"test endpoint reference");need(!Access::endpoint(*f.c,q,{Fr(7)},true),"test unprofitable endpoint remains optional");need(calls==128,"test callback never revisited");
 Access::invalidate(*f.c);rejects([&]{Access::k(*f.c,0,{Fr(7)});},"requires frozen mapping");rejects([&]{Access::endpoint(*f.c,q,{Fr(7)},false);},"requires frozen mapping");
}
void fast(bool file){Fixture f;Access::budget(*f.c,file?480:1u<<20,4u<<20);std::size_t calls=0;auto c=claim(f,"fast",4096,[&](std::size_t j){++calls;if(file && j<64){auto at=(j*j+17*j)%39;return cl::Located{{std::uint32_t(at/13),std::uint32_t(at%13)},Fr(0),false};}return cl::Located{{std::uint32_t(j%2?2:0),4},Fr(0),false};});Access::add(*f.c,std::move(c));std::vector<Fr>q(6,Fr(3));Fr reference=0;auto w=r1::weights(std::vector<Fr>(12,Fr(2)));auto global=r1::weights(q);for(std::size_t j=0;j<4096;++j){auto at=(j*j+17*j)%39;auto z=file && j<64?(at/13==0?0:at/13==1?16:29)+at%13:(j%2?33u:4u);reference+=Fr(9)*w[j]*global[z];}need(std::string(Access::plan(*f.c,0).reason)==(file?"FROZEN_ROOT_FILE":"FROZEN_ROOT_RAM"),"fast endpoint storage path");auto endpoint=Access::endpoint(*f.c,q,{Fr(9)},true);need(endpoint && *endpoint==reference && calls==4096,"test carry/terminal fast blocks and no rescan");}
void negative(){
 for(bool file:{false,true}){Fixture f;Access::budget(*f.c,file?480:0,0);auto c=claim(f,"exhausted-claim",128,mapped);auto error=rejects([&]{Access::add(*f.c,std::move(c));},file?"FILE_BUDGET_EXHAUSTED":"RAM_METADATA_BUDGET_EXHAUSTED");for(auto text:{"exhausted-claim","RAM_used=","RAM_limit=","file_used=","file_limit="})need(error.find(text)!=std::string::npos,"test complete freeze failure diagnostic");rejects([&]{f.c->authenticateUnifiedInput(f.x);},"lifecycle/statement");need(f.c->claims()==0,"test failed claim not published");for(const auto&label:f.io.labels)need(label.find("CategoryInput/rho/")==std::string::npos && label.find("UnifiedInput/sumcheck")==std::string::npos && label.find("UnifiedInput/opening")==std::string::npos,"test resource failure before cryptography");}
 for(unsigned kind=0;kind<3;++kind){Fixture f;auto c=claim(f,"invalid",1,[kind](std::size_t){return cl::Located{{kind==0?99u:0u,kind==1?99u:0u},Fr(0),false};});if(kind==2)c.only_root=1;rejects([&]{Access::add(*f.c,std::move(c));},kind==2?"root-only":"root/coordinate bounds");}
 for(bool flag:{false,true}){Fixture f;auto c=claim(f,"untrusted",1,[](std::size_t){return cl::Located{{0,0},Fr(0),false};});c.static_locator=flag;rejects([&]{f.c->defer(std::move(c));},"untrusted raw Claim");}
 {Fixture f;Access::budget(*f.c,0,0);auto c=claim(f,"constant",8,[](std::size_t){return cl::Located{{},Fr(7),true};});Access::add(*f.c,std::move(c));Access::check(*f.c,0,8);need(Access::bias(*f.c,0)==Fr(7),"constant-only bias");for(unsigned k=0;k<3;++k)for(auto&v:Access::k(*f.c,k,{Fr(1)}))need(v.isZero(),"constant-only zero K");need(Access::endpoint(*f.c,std::vector<Fr>(6,Fr(3)),{Fr(1)},false)->isZero(),"constant-only endpoint");}
 {Fixture f;Access::budget(*f.c,200,4u<<20);Access::add(*f.c,claim(f,"first",128,mapped));rejects([&]{Access::add(*f.c,claim(f,"second",128,mapped));},"RAM_METADATA_BUDGET_EXHAUSTED");}
 {Fixture f;Access::add(*f.c,claim(f,"session",1,[](std::size_t){return cl::Located{{0,0},Fr(0),false};}));Access::wrongSession(*f.c);rejects([&]{Access::bias(*f.c,0);},"cross-session");}
 {cl::mapping::Store store(0);cl::mapping::Plan p;cl::mapping::Builder builder(p,store,0,3);builder.finish(true);need(p.frozen && p.run_count==0,"empty mapping frozen");}
}
void protocol(fs::FsIO&io,bool file){auto l=layout();cl::Prepared prepared;OSRandom rng;std::vector<Fr>x;for(unsigned j=0;j<13;++j)x.emplace_back(j+1);cl::Commitments c(io,l,io.prover()?oneblock::Value([](std::size_t j){return Fr(j+1);}):oneblock::Value{},io.prover()?&prepared:nullptr,io.prover()?&rng:nullptr,x);c.constrainBaseTails();Access::budget(c,file?480:1u<<20,4u<<20);
 auto point=std::vector<Fr>(7,Fr(2));auto w=r1::weights(point);Fr value=0;for(std::size_t j=0;j<128;++j)value+=w[j]*Fr((j*17+j*j)%39+1);
 auto h=oneblock::exchangeHidden(io,"claim/evaluation",value,io.prover()?&rng:nullptr);std::size_t calls=0;c.deferBase("claim",128,128,[&](std::size_t j){++calls;return (j*17+j*j)%39;},h,point);need(calls==128,"protocol mapping initial scan");c.authenticateUnifiedInput(x);need(c.authenticated() && calls==128,"protocol no callbacks after freeze");Access::check(c,0,128);Access::check(c,1,16);need(std::string(Access::plan(c,0).reason)==(file?"FROZEN_ROOT_FILE":"FROZEN_ROOT_RAM"),"protocol RAM/file frozen");}
}
int main(int argc,char**argv){try{r1::init();if(argc==2){compare(false);compare(true);fast(false);fast(true);negative();std::cout<<"PASS frozen RAM/file, coefficients/bias/endpoint, budget/lifecycle/source guards, exact single scan\n";return 0;}need(argc==4,"prove|verify ram|file PROOF");bool p=std::string(argv[1])=="prove";fs::Binding binding{"OneblockSingleHyrax","category-frozen-mapping-test",fs::generatorIdentity(),Bytes{1,2,3}};std::unique_ptr<fs::FsIO>io;if(p)io=std::make_unique<fs::FsProverIO>(argv[3],binding);else io=std::make_unique<fs::FsVerifierIO>(argv[3],binding);protocol(*io,std::string(argv[2])=="file");io->finish();std::cout<<io->metadataJson()<<'\n';return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
