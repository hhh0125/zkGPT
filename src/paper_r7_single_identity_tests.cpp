#include "paper_r7_single_hyrax.hpp"
#include "paper_r7_fs_io.hpp"
#include "paper_r7_stage2_opening.hpp"
#include <openssl/evp.h>
#include <iostream>
#include <type_traits>
#include <optional>
#include <algorithm>
using namespace zkgpt::r7;
using namespace crypto;
namespace sh=oneblock::single;
namespace {
template<class F>void rejects(F f,const char*reason){try{f();}catch(const std::exception&e){need(std::string(e.what()).find(reason)!=std::string::npos,e.what());return;}throw std::runtime_error("expected rejection");}
std::string reference(const Bytes&b){Digest d{};unsigned n=0;need(b.size()>=4 && EVP_Digest(b.data()+4,b.size()-4,d.data(),&n,EVP_sha256(),nullptr)==1 && n==32,"independent SHA256");return hex(d);}
void identities(){
 static_assert(std::is_const_v<std::remove_reference_t<decltype(std::declval<sh::Public&>().rows())>>);
 static_assert(std::is_const_v<std::remove_reference_t<decltype(std::declval<sh::Public&>().plan())>>);
 const auto g=parameters(1).scalar;
 for(std::size_t n:{1,4,128,256,512}){
  auto plan=sh::makeCategoryPlan({n*n,0,0,0,0},std::string(64,'a'));
  need(plan.rows==n,"test matrix rows");std::vector<G1>rows(n,g);sh::Public pc(plan,rows);
  auto encoded=sh::encode(pc);auto id=sh::identity(pc);need(id==reference(encoded) && id==sh::identity(pc),"identity reference/determinism");
  auto decoded=sh::decode(encoded,plan);need(sh::identity(decoded)==id,"identity canonical roundtrip");
  for(std::size_t at:{std::size_t(0),n/2,n-1}){auto changed=rows;changed[at]+=g;need(sh::identity(sh::Public(plan,changed))!=id,"identity row mutation");}
  auto other=sh::makeCategoryPlan({n*n,0,0,0,0},std::string(64,'b'));need(sh::identity(sh::Public(other,rows))!=id,"identity Plan mutation");
  auto truncated=encoded;truncated.pop_back();rejects([&]{sh::decode(truncated,plan);},"single encoded row bytes/EOF");
  rows.pop_back();rejects([&]{sh::encode(sh::Public(plan,rows));},"single row commitment count");
  std::cout<<"VECTOR rows="<<n<<" identity="<<id<<" bytes="<<encoded.size()<<'\n';
 }
 // Canonical matrices have power-of-two row counts. A 257-row serialization
 // fixture checks chunk-tail hashing only; decode must reject this Plan.
 auto plan=sh::makeCategoryPlan({65536,0,0,0,0},std::string(64,'a'));plan.rows=257;
 sh::Public tail(plan,std::vector<G1>(257,g));need(sh::identity(tail)==reference(sh::encode(tail)),"257-row serialization tail");
 rejects([&]{sh::decode(sh::encode(tail),plan);},"category global shape");
}
class GuardIO:public IO {
 public:std::string sid="guard";bool prover()const override{return false;}std::string session()const override{return sid;}
 Bytes message(const std::string&,const Bytes& = {})override{throw std::runtime_error("unexpected IO");}Fr coin(const std::string&)override{throw std::runtime_error("unexpected IO");}Digest prefix()const override{return {};}
};
void guards(){GuardIO io;auto plan=sh::makeCategoryPlan({4,0,0,0,0},std::string(64,'a'));sh::Public pc(plan,std::vector<G1>(plan.rows,parameters(1).scalar));
 rejects([&]{sh::open(io,pc,{}, {},nullptr,nullptr,nullptr,"unsealed");},"requires exchange");
 rejects([&]{sh::evaluateAndOpen(io,pc,{}, {},nullptr,nullptr,"unsealed");},"requires exchange");
}
class AttackIO:public IO {
 IO&io_;std::string attack_;const sh::Public*pc_;
 public:AttackIO(IO&io,std::string attack,const sh::Public*pc):io_(io),attack_(std::move(attack)),pc_(pc){}
 std::shared_ptr<const void>transcriptOwner()const override{return io_.transcriptOwner();}
 bool prover()const override{return io_.prover();}std::string session()const override{return io_.session();}Fr coin(const std::string&s)override{return io_.coin(s);}Digest prefix()const override{return io_.prefix();}
 Bytes message(const std::string&label,const Bytes&bytes={})override{
  auto data=bytes;
  if(prover() && attack_=="row" && label=="SingleHyrax/rows/0"){
   Reader r{data};auto count=r.number();Writer w;w.number(count);for(std::size_t i=0;i<count;++i){auto point=r.point();if(i==0)point+=parameters(1).scalar;w.point(point);}r.end();data=std::move(w.b);
  }
  if(prover() && attack_=="old-identity" && label=="SingleHyrax/sealed"){
   Writer legacy;legacy.raw(sh::descriptor(pc_->plan()));legacy.number(256);
   for(std::size_t at=0;at<pc_->rows().size();at+=256){Writer chunk;auto n=std::min<std::size_t>(256,pc_->rows().size()-at);chunk.number(n);for(std::size_t j=0;j<n;++j)chunk.point(pc_->rows()[at+j]);auto d=r1::hash(chunk.b);legacy.raw(Bytes(d.begin(),d.end()));}
   Writer seal;seal.text(sh::suite);seal.text(hex(r1::hash(legacy.b)));seal.text(session());data=std::move(seal.b);
  }
  auto result=io_.message(label,data);return prover()?bytes:result;
 }
};
void protocol(fs::FsIO&base,const std::string&mode){
 auto plan=sh::makeCategoryPlan({8,0,0,0,0},std::string(64,'a'));if(!base.prover() && mode=="plan")plan.source_identity=std::string(64,'b');
 OSRandom rng;std::optional<sh::Secret>secret;
 if(base.prover())secret=sh::commit(plan,{[](std::size_t j){return Fr(j+1);}},rng,2);
 AttackIO io(base,mode,secret?&secret->pc:nullptr);auto pc=sh::exchange(io,plan,secret?&*secret:nullptr);
 if(mode=="lifecycle"){
  GuardIO other;other.sid=io.session();rejects([&]{sh::open(other,pc,{}, {},nullptr,nullptr,nullptr,"cross");},"same live transcript");
  rejects([&]{sh::evaluateAndOpen(other,pc,{}, {},nullptr,nullptr,"cross");},"same live transcript");
 }
 auto point=std::vector<Fr>(sh::bits(plan.domain),Fr(2));auto h=oneblock::exchangeHidden(io,"test/evaluation",secret?sh::evaluate(*secret,point):Fr(0),secret?&rng:nullptr);
 std::string role="test/opening";
 if(!base.prover()){
  if(mode=="point")point[0]+=Fr(1);
  if(mode=="evaluation")h.handle.point+=parameters(1).scalar;
  if(mode=="role")role="test/changed";
 }
 sh::open(io,pc,{point,h.handle},{},secret?&*secret:nullptr,secret?&h:nullptr,secret?&rng:nullptr,role);
 // A second opening must reuse the initial seal without another identity.
 stage2::ScopedIO scoped(io,"second");sh::evaluateAndOpen(scoped,pc,point,{},secret?&*secret:nullptr,secret?&rng:nullptr,"test/opening2");
}
}
int main(int argc,char**argv){try{r1::init();if(argc==2){identities();guards();return 0;}
 need(argc==4,"prove|verify MODE PROOF");bool p=std::string(argv[1])=="prove";std::string mode=argv[2];
 fs::Binding binding{"OneblockSingleHyrax",mode=="session" && !p?"identity-test-other":"identity-test",fs::generatorIdentity(),Bytes{1,2,3}};
 std::unique_ptr<fs::FsIO>io;if(p)io=std::make_unique<fs::FsProverIO>(argv[3],binding);else io=std::make_unique<fs::FsVerifierIO>(argv[3],binding);
 protocol(*io,mode);io->finish();std::cout<<io->metadataJson()<<'\n';return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
