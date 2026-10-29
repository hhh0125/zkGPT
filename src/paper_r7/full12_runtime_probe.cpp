#include "classified_commitments.hpp"
#include "stage2_opening.hpp"
#include "fs_io.hpp"
#include <iostream>
using namespace zkgpt::r7;using namespace oneblock;using namespace classified;
struct Tape:IO{bool p;std::size_t at=0,draw=0;std::vector<std::pair<std::string,Bytes>>data;explicit Tape(bool b):p(b){}bool prover()const override{return p;}std::string session()const override{return std::string(64,'b');}Digest prefix()const override{return {};}Bytes message(const std::string&n,const Bytes&b={})override{if(p){data.push_back({n,b});return b;}need(at<data.size() && data[at].first==n,"partition crypto order");return data[at++].second;}Fr coin(const std::string&)override{return Fr(3+draw++%71);}};
int main(int argc,char**argv){try{need(argc==2,"component probe requires proof path");zkgpt::r1::init();R7Context c;c.layout_sha256=std::string(64,'a');c.plan.shape.layer_count=12;c.plan.master=zkgpt::r1::PCSpec::make("master",31,32,zkgpt::r1::Layout::RowMajor);c.plan.sources={{zkgpt::r1::PCSpec::make("query",31,32,zkgpt::r1::Layout::RowMajor),{{0,31,Fr(2)}}},{zkgpt::r1::PCSpec::make("W",11,16,zkgpt::r1::Layout::RowMajor),{{3,5,Fr(0)},{16,6,Fr(0)}}}};c.objects={{"r7/master","",ObjectKind::BaseMaster,31,32,{},false},{"r7/query","r7/master",ObjectKind::MappedChild,31,32,{{0,31,Fr(2)}},false},{"r7/W","r7/master",ObjectKind::ModelW,11,16,{{3,5,Fr(0)},{16,6,Fr(0)}},false},{"r7/helper","r7/query",ObjectKind::RangeDigit,20,32,{},false}};
 auto l=makeLayout(c,8);Value base=[](std::size_t j){return Fr(j+1);};std::vector<model::Trust>trust;std::vector<model::Secret>secrets;SessionRandom rng;namespace fs=zkgpt::r7::fs;fs::Binding binding{"Full12","synthetic-partition-component-not-native-Full12",std::string(64,'a'),{3,1,4,1,5}};
#ifdef R7_FULL12_PROBE_VERIFIER
 constexpr bool isP=false;fs::FsVerifierIO tx(argv[1],binding);
#else
 constexpr bool isP=true;fs::FsProverIO tx(argv[1],binding);
#endif
std::vector<Fr>W;for(std::size_t j=0;j<11;++j)W.push_back(base(j<5?3+j:16+j-5));std::size_t off=0;
 for(auto&r:l.roots)if(r.model){model::Trust t;t.anchor.profile="unit";t.anchor.profile_hash=std::string(64,'1');t.anchor.manifest_hash=std::string(64,'2');t.anchor.circuit=std::string(64,'3');t.anchor.model_id="unit-full12-model";auto s=zkgpt::r1::PCSpec::make("unit/W/"+std::to_string(trust.size()),r.actual,r.padded,zkgpt::r1::Layout::RowMajor);std::vector<Fr>values(W.begin()+off,W.begin()+off+r.actual);auto padded=values;padded.resize(r.padded,Fr(0));t.anchor.w=zkgpt::r1::commit(s,padded);t.anchor_hash=zkgpt::r5b::digestHex(zkgpt::r6::encodeAnchor(t.anchor));t.manifest_hash=t.anchor.manifest_hash;trust.push_back(t);if(isP)secrets.push_back(model::rerandomize(trust.back(),tx.session(),std::move(values),rng));off+=r.actual;}
 Prepared prepared;prepared["r7/helper"]=std::make_shared<std::vector<Fr>>(32);for(std::size_t j=0;j<32;++j)prepared["r7/helper"]->at(j)=Fr(j<20?j+7:0);
 Commitments p(tx,l,isP?base:Value{},isP?&prepared:nullptr,isP?&rng:nullptr,nullptr,nullptr,&trust,isP?&secrets:nullptr);p.constrainBaseTails();std::vector<Fr>q{Fr(2),Fr(3),Fr(5),Fr(7),Fr(11)};
 for(auto id:{"r7/query","r7/helper","r7/W"}){auto v=p.view(id);auto point=q;point.resize(zkgpt::r1::log2exact(v.padded));Fr value=0;if(isP){std::vector<Fr>values(v.padded);for(std::size_t j=0;j<v.padded;++j)values[j]=v.value(j);value=zkgpt::r1::mle(values,point);}auto h=exchangeHidden(tx,std::string(id)+"/value",value,isP?&rng:nullptr);v.open(tx,{point,h.handle},isP?&h:nullptr,isP?&rng:nullptr,std::string(id)+"/open");}p.authenticate();
 need(p.authenticated(),"component authentication barrier");tx.finish();std::cerr<<tx.metadataJson()<<'\n';std::cout<<(isP?"PARTITION_COMPONENT_PROOF_CREATED":"PARTITION_COMPONENT_INDEPENDENT_VERIFIED")<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
