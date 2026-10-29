import subprocess
stage=Path('reports/R7-FULL12-UNRESTRICTED-20261029-v1')
Path('src/paper_r7/full12_runtime_tests.cpp').write_text(r'''#include "classified_commitments.hpp"
#include "stage2_opening.hpp"
#include <iostream>
using namespace zkgpt::r7;using namespace oneblock;using namespace classified;
struct Tape:IO{bool p;std::size_t at=0,draw=0;std::vector<std::pair<std::string,Bytes>>data;explicit Tape(bool b):p(b){}bool prover()const override{return p;}std::string session()const override{return std::string(64,'b');}Digest prefix()const override{return {};}Bytes message(const std::string&n,const Bytes&b={})override{if(p){data.push_back({n,b});return b;}need(at<data.size() && data[at].first==n,"partition crypto order");return data[at++].second;}Fr coin(const std::string&)override{return Fr(3+draw++%71);}};
int main(){try{zkgpt::r1::init();R7Context c;c.layout_sha256=std::string(64,'a');c.plan.shape.layer_count=12;c.plan.master=zkgpt::r1::PCSpec::make("master",31,32,zkgpt::r1::Layout::RowMajor);c.plan.sources={{zkgpt::r1::PCSpec::make("query",31,32,zkgpt::r1::Layout::RowMajor),{{0,31,Fr(2)}}},{zkgpt::r1::PCSpec::make("W",11,16,zkgpt::r1::Layout::RowMajor),{{3,5,Fr(0)},{16,6,Fr(0)}}}};c.objects={{"r7/master","",ObjectKind::BaseMaster,31,32,{},false},{"r7/query","r7/master",ObjectKind::MappedChild,31,32,{{0,31,Fr(2)}},false},{"r7/W","r7/master",ObjectKind::ModelW,11,16,{{3,5,Fr(0)},{16,6,Fr(0)}},false},{"r7/helper","r7/query",ObjectKind::RangeDigit,20,32,{},false}};
 auto l=makeLayout(c,8);Value base=[](std::size_t j){return Fr(j+1);};std::vector<model::Trust>trust;std::vector<model::Secret>secrets;SessionRandom rng;Tape tx(true);std::vector<Fr>W;for(std::size_t j=0;j<11;++j)W.push_back(base(j<5?3+j:16+j-5));std::size_t off=0;
 for(auto&r:l.roots)if(r.model){model::Trust t;t.anchor.profile="unit";t.anchor.profile_hash=std::string(64,'1');t.anchor.manifest_hash=std::string(64,'2');t.anchor.circuit=std::string(64,'3');t.anchor.model_id="unit-full12-model";auto s=zkgpt::r1::PCSpec::make("unit/W/"+std::to_string(trust.size()),r.actual,r.padded,zkgpt::r1::Layout::RowMajor);std::vector<Fr>values(W.begin()+off,W.begin()+off+r.actual);auto padded=values;padded.resize(r.padded,Fr(0));t.anchor.w=zkgpt::r1::commit(s,padded);t.anchor_hash=zkgpt::r5b::digestHex(zkgpt::r6::encodeAnchor(t.anchor));t.manifest_hash=t.anchor.manifest_hash;trust.push_back(t);secrets.push_back(model::rerandomize(trust.back(),tx.session(),std::move(values),rng));off+=r.actual;}
 Prepared prepared;prepared["r7/helper"]=std::make_shared<std::vector<Fr>>(32);for(std::size_t j=0;j<32;++j)prepared["r7/helper"]->at(j)=Fr(j<20?j+7:0);
 Commitments p(tx,l,base,&prepared,&rng,nullptr,nullptr,&trust,&secrets);p.registerBase();std::vector<Fr>q{Fr(2),Fr(3),Fr(5),Fr(7),Fr(11)};
 for(auto id:{"r7/query","r7/helper","r7/W"}){auto v=p.view(id);auto point=q;point.resize(zkgpt::r1::log2exact(v.padded));std::vector<Fr>values(v.padded);for(std::size_t j=0;j<v.padded;++j)values[j]=v.value(j);auto h=exchangeHidden(tx,std::string(id)+"/value",zkgpt::r1::mle(values,point),&rng);v.open(tx,{point,h.handle},&h,&rng,std::string(id)+"/open");}p.authenticate();
 Tape rx(false);rx.data=tx.data;Commitments v(rx,l,{},nullptr,nullptr,nullptr,nullptr,&trust,nullptr);v.registerBase();for(auto id:{"r7/query","r7/helper","r7/W"}){auto a=v.view(id);auto point=q;point.resize(zkgpt::r1::log2exact(a.padded));auto h=exchangeHidden(rx,std::string(id)+"/value",Fr(0),nullptr);a.open(rx,{point,h.handle},nullptr,nullptr,std::string(id)+"/open");}v.authenticate();need(v.authenticated() && rx.at==rx.data.size(),"independent cross-root mandatory authentication");std::cout<<"FULL12_PARTITION_CRYPTO_TESTED roots="<<l.roots.size()<<" registered_rows="<<v.registered_rows<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
''')
Path('src/paper_r7/full12_fs_tests.cpp').write_text(r'''#include "fs_io.hpp"
#include <iostream>
#include <unistd.h>
int main(){try{using namespace zkgpt::r7::crypto;namespace fs=zkgpt::r7::fs;r1::init();char d[]="build/full12-fs-unit-XXXXXX";need(mkdtemp(d),"Full12 FS temp");auto path=std::string(d)+"/proof";fs::Binding b{"Full12","unit-full12",std::string(64,'a'),{1,2,3}};{fs::FsProverIO p(path,b);p.message("Full12/first",{7});p.coin("Full12/challenge");p.finish();}{fs::FsVerifierIO v(path,b);need(v.message("Full12/first")==Bytes{7},"Full12 FS public message");v.coin("Full12/challenge");v.finish();}bool rejected=false;auto wrong=b;wrong.proof_type="Oneblock";try{fs::FsVerifierIO v(path,wrong);}catch(...){rejected=true;}need(rejected,"Oneblock/Full12 suite mixing must reject");std::cout<<"FULL12_FS_FRAMING_TESTED\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
''')
p=Path('src/paper_r7/CMakeLists.txt');p.write_text(p.read_text()+'\nadd_executable(r7_full12_runtime_tests full12_runtime_tests.cpp)\ntarget_link_libraries(r7_full12_runtime_tests PRIVATE paper_r7)\nadd_executable(r7_full12_fs_tests full12_fs_tests.cpp)\ntarget_link_libraries(r7_full12_fs_tests PRIVATE paper_r7)\n')
subprocess.run(['cmake','-S','.','-B','build/release'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)
for name in ['r7_full12_runtime_tests','r7_full12_fs_tests']:
 with (stage/(name+'-red-build.log')).open('xb') as out:r=subprocess.run(['cmake','--build','build/release','--target',name,'-j4'],stdout=out,stderr=subprocess.STDOUT)
 if name=='r7_full12_runtime_tests':assert r.returncode!=0
 else:
  assert r.returncode==0
  with (stage/(name+'-red.log')).open('xb') as out:r=subprocess.run(['build/release/src/paper_r7/'+name],stdout=out,stderr=subprocess.STDOUT)
  assert r.returncode!=0
 print('EXPECTED_RED',name,r.returncode,flush=True)
