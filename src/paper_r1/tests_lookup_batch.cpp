#include "lookup_batch.hpp"
#include "wire.hpp"
#include "lookup_attack_fixture.hpp"
#include <iostream>
#include <functional>
using namespace zkgpt::r1;
static void rejects(const std::function<void()> &f) {bool bad=false;try{f();}catch(const std::exception&){bad=true;}require(bad,"lookup attack accepted");}
int main(int argc,char **argv){try{
 init();std::vector<Fr>w(32,Fr(0));w[0]=Fr(-7);w[1]=Fr(4);w[2]=Fr(0);w[3]=Fr(1);w[4]=Fr(8191);
 w[8]=Fr(1048576);w[9]=Fr(1046530);w[10]=Fr(0);w[20]=Fr(17);w[28]=Fr(1);
 auto root=PCSpec::make("r2/lookup-master",29,32,Layout::RowMajor);
 auto q=PCSpec::make("r2/range",2,2,Layout::ColumnMajor);
 auto a=PCSpec::make("r2/exp-t",3,4,Layout::RowMajor);
 auto e=PCSpec::make("r2/exp-E",3,4,Layout::ColumnMajor);
 std::vector<std::vector<Fr>>v{{Fr(121),Fr(132)},{Fr(0),Fr(1),Fr(8191),Fr(0)}, {Fr(1048576),Fr(1046530),Fr(0),Fr(0)}};
 LookupBatchSpec s;
 s.input={"R2-LOOKUP-FIXTURE-not-GPT2",commit(root,w),
  {{commit(q,v[0]),{{0,2,Fr(128)}}},{commit(a,v[1]),{{2,3,Fr(0)}}},{commit(e,v[2]),{{8,3,Fr(0)}}}},
  {{20,Fr(17)},{28,Fr(1)}}};
 auto extra=PCSpec::make("r2/model-aux",1,2,Layout::ColumnMajor);v.push_back({Fr(17),Fr(0)});
 s.input.children.push_back({commit(extra,v.back()),{{20,1,Fr(0)}}});
 s.ranges={{0,"r2/range-proof",8}};s.exps={{1,2,"r2/exp-proof"}};
 Fs pt("lookup"),vt("lookup");auto p=proveLookupBatch(s,w,v,pt);verifyLookupBatch(s,p,vt);
 require(pt.digest()==vt.digest(),"lookup transcript");
 auto b=p;b.exps[0].memory.roots[0]+=Fr(1);rejects([&]{Fs t("lookup");verifyLookupBatch(s,b,t);});
 b=p;b.exps[0].helper[1].rows[0]+=generators(expHelperSpec(expSpec(s,0),1)).u;
 rejects([&]{Fs t("lookup");verifyLookupBatch(s,b,t);});
 b=p;b.input.master.value+=Fr(1);rejects([&]{Fs t("lookup");verifyLookupBatch(s,b,t);});
 auto ss=s;auto vv=v;vv[2][0]=Fr(1046530);ss.input.children[2].commitment=commit(e,vv[2]);
 rejects([&]{Fs t("lookup");proveLookupBatch(ss,w,vv,t);});
 ss=s;ss.exps.clear();rejects([&]{Fs t("lookup");verifyLookupBatch(ss,p,t);});
 ss=s;ss.input.public_values[0].value+=Fr(1);rejects([&]{Fs t("lookup");verifyLookupBatch(ss,p,t);});
 auto mb=serializeMappedInput(s,p.input);require(serializeMappedInput(s,parseMappedInput(s,mb))==mb,"mapped source codec");
 auto bytes=serializeLookup(s,p);auto decoded=parseLookupProof(s,bytes);
 Fs wiret("lookup");verifyLookupBatch(s,decoded,wiret);
 require(serializeLookup(parseLookupSpec(serializeLookup(s)))==serializeLookup(s),"lookup canonical statement");
 auto broken=bytes;broken.pop_back();rejects([&]{parseLookupProof(s,broken);});
 broken=bytes;broken.push_back(0);rejects([&]{parseLookupProof(s,broken);});
 if(argc==2){
  const std::string dir=argv[1];
  auto save=[&](const std::string &n,const LookupBatchSpec &ss,const LookupBatchProof &pp){writeFile(dir+"/"+n+".statement",serializeLookup(ss));writeFile(dir+"/"+n+".proof",serializeLookup(ss,pp));};
  save("valid",s,p);
  auto bad=p;bad.exps[0].memory.roots[0]+=Fr(1);save("memory-root",s,bad);
  bad=p;bad.exps[0].memory.products[0].leaf[2].value+=Fr(1);save("read-counter",s,bad);
  bad=p;bad.exps[0].helper[1].rows[0]+=generators(expHelperSpec(expSpec(s,0),1)).u;save("selector",s,bad);
  bad=p;bad.input.master.value+=Fr(1);save("master-opening",s,bad);
  bad=p;bad.input.sumcheck.rounds[0][0]+=Fr(1);save("sumcheck",s,bad);
  auto changed=s;changed.input.public_values[0].value+=Fr(1);save("public-Y",changed,p);
  changed=s;changed.input.public_values[1].value+=Fr(1);save("constant",changed,p);
  changed=s;changed.input.context+="other-model";save("model",changed,p);
  changed=s;changed.input.children[0].spans[0].offset+=1;save("source-map",changed,p);
  changed=s;changed.input.children[0].commitment.rows[0]+=generators(q).u;save("query-commitment",changed,p);
  changed=s;changed.exps[0].id+="other-table";save("table-instance",changed,p);
  auto detached=v;detached[0][0]+=Fr(1);changed=s;changed.input.children[0].commitment=commit(q,detached[0]);
  auto forged=hostileLookupBatch(changed,w,detached);save("fresh-local-different-witness",changed,forged);
  {Fs t("lookup");rejects([&]{verifyLookupBatch(changed,forged,t);});}
  detached=v;detached[3][1]=Fr(1);changed=s;changed.input.children[3].commitment=commit(extra,detached[3]);
  forged=hostileLookupBatch(changed,w,detached);save("fresh-local-child-padding",changed,forged);
  auto bad_master=w;bad_master[31]=Fr(1);changed=s;changed.input.master=commit(root,bad_master);
  forged=hostileLookupBatch(changed,bad_master,v);save("fresh-local-master-padding",changed,forged);
  auto bytes2=serializeLookup(s,p);bytes2.pop_back();writeFile(dir+"/truncated.statement",serializeLookup(s));writeFile(dir+"/truncated.proof",bytes2);
  bytes2=serializeLookup(s,p);bytes2.push_back(0);writeFile(dir+"/trailing.statement",serializeLookup(s));writeFile(dir+"/trailing.proof",bytes2);
 }
 auto singleton=s;singleton.exps.clear();singleton.ranges={{0,"r2/single-range",9}};
 auto single=PCSpec::make("r2/single",1,1,Layout::ColumnMajor);
 singleton.input.children={{Commitment{single,{}},{{0,1,Fr(0)}}}};
 auto single_resource=planLookupBatch(singleton);
 require(single_resource.estimated_prover_bytes>=512*sizeof(Fr),"Range table workspace missing from budget");
 require(single_resource.max_sumcheck_rounds>=8,"Range table rounds missing from budget");
 auto resources=planLookupBatch(s);
 require(resources.query_elements==12 && resources.max_generators==128 && resources.estimated_prover_bytes>0,"resource shape");
 rejects([&]{requireLookupBudget(s,1);});requireLookupBudget(s,resources.estimated_prover_bytes);
 std::cout<<"PASS Range+indexed Exp+mapped master; wrong address pair, selector, roots, Y, resources\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
