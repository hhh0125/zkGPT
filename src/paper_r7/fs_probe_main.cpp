#include "classified_commitments.hpp"
#include "fs_io.hpp"
#include <sys/prctl.h>
#include <sys/resource.h>
#include <unistd.h>
#include <chrono>
#include <iostream>
using namespace zkgpt::r7;using namespace oneblock;using namespace classified;
class ProbeIO final:public IO {
 zkgpt::r7::fs::FsIO&base_;bool alter_;
 public:ProbeIO(zkgpt::r7::fs::FsIO&b,bool a):base_(b),alter_(a){}
 bool prover()const override{return base_.prover();}std::string session()const override{return base_.session();}Digest prefix()const override{return base_.prefix();}
 Bytes message(const std::string&s,const Bytes&b={})override{return base_.message(s,b);}
 Fr coin(const std::string&s)override{auto c=base_.coin(s);if(alter_){alter_=false;return c+Fr(1);}return c;}
 std::uint64_t wireBytes()const{return base_.wireBytes();}std::uint64_t proofBytes()const{return base_.proofBytes();}
};
int main(int argc,char**argv){try{
 need(prctl(PR_SET_DUMPABLE,0)==0,"classified probe isolation");alarm(180);r1::init();auto start=std::chrono::steady_clock::now();
#ifdef R7_CLASSIFIED_PROBE_V
 const bool isP=false;need(argc==5,"classified V cannot import challenges/options");
#else
 const bool isP=true;need(argc==5 || argc==6,"classified P probe arguments");
#endif
 auto n=std::stoull(argv[1]);std::string grouping=argv[2],attack=argc==6?argv[5]:"";need(n>=16 && n<=1024 && !(n&(n-1)) && (grouping=="joint" || grouping=="grouped"),"classified bounded probe dimension/grouping");
 // Exercise actual physical zero tails in the independent FS component probe.
 Layout p;p.native=std::string(64,'a');p.roots={{"C_aux",n-3,n,true,false},{"C_lookup/queries",n-3,n,true,false}};for(std::size_t i=0;i<2*(n-3);++i)p.base.push_back({std::uint32_t(i%2),std::uint32_t(i/2)});
 auto slab=grouping=="joint"?4*n:2*n;for(std::size_t j=0;j<4*n/slab;++j)p.roots.push_back({"C_lookup/helpers/"+std::to_string(j),slab,slab,false,false});
 Prepared prepared;
 for(unsigned j=0;j<4;++j){View v;v.id="unit/helper/"+std::to_string(j);v.actual=n-3;v.padded=n;v.kind=ObjectKind::ExpHelper;v.start={std::uint32_t(2+j*n/slab),std::uint32_t(j*n%slab)};p.views.emplace(v.id,v);if(isP){auto x=std::make_shared<std::vector<Fr>>(n,Fr(1048576));for(std::size_t i=0;i<n-3;++i)x->at(i)=Fr(1+j+i%13);prepared.emplace(v.id,x);}}
 View mapped;mapped.id="unit/L1";mapped.actual=mapped.padded=n;mapped.mapped=true;mapped.kind=ObjectKind::MappedChild;mapped.spans={{0,n/2,Fr(-3)},{0,n/2,Fr(5)}};p.views.emplace(mapped.id,mapped);
 Writer publicInstance;publicInstance.text(classified::identity(p));publicInstance.number(n,8);publicInstance.text(grouping);
 zkgpt::r7::fs::Binding binding{"ClassifiedProbe",argv[4],zkgpt::r7::fs::generatorIdentity(),publicInstance.b};
#ifdef R7_CLASSIFIED_PROBE_V
 zkgpt::r7::fs::FsVerifierIO transport(argv[3],binding);
#else
 zkgpt::r7::fs::FsProverIO transport(argv[3],binding);
#endif
 ProbeIO io(transport,isP && attack=="challenge");SessionRandom random;Random*rng=isP?&random:nullptr;Value base;if(isP)base=[](std::size_t i){return Fr(int(i%29)-7);};
 Commitments pcs(io,p,base,isP?&prepared:nullptr,rng);pcs.constrainBaseTails();
 if(isP && attack=="late-change")pcs.roots().at(2).secret->values->at(0)+=Fr(1);
 for(auto&[id,v]:p.views){auto oracle=pcs.view(id);std::vector<Fr>q;for(unsigned k=0;k<r1::log2exact(n);++k)q.push_back(io.coin("probe/"+id+"/query/"+std::to_string(k)));Fr y=0;if(isP){auto weights=r1::weights(q);for(std::size_t i=0;i<n;++i)y+=oracle.value(i)*weights[i];if(attack=="value" && id=="unit/L1")y+=Fr(1);}auto h=exchangeHidden(io,"probe/"+id+"/value",y,rng);oracle.open(io,{q,h.handle},isP?&h:nullptr,rng,"probe/"+id+"/open");}
 std::vector<Fr>q;for(unsigned j=0;j<r1::log2exact(n);++j)q.push_back(io.coin("probe/GKR-point/"+std::to_string(j)));auto mapping=[=](std::size_t j){return (j+(isP && attack=="map"?1:0))%(2*n);};Fr y=0;if(isP){auto w=r1::weights(q);for(std::size_t j=0;j<n;++j)y+=w[j]*base(mapping(j));}auto h=exchangeHidden(io,"probe/GKR-value",y,rng);pcs.deferBase("probe/GKR-terminal",n,n,mapping,h,q);
 need(!pcs.authenticated(),"classified pending claim false acceptance");need(!(isP && attack=="missing-barrier"),"test attack: missing authentication barrier");pcs.authenticate();need(pcs.authenticated(),"classified incomplete barrier");transport.finish();std::cerr<<transport.metadataJson()<<'\n';
 struct rusage r{};getrusage(RUSAGE_SELF,&r);auto m=metrics();std::cerr<<"{\"status\":\""<<(isP?"FS_PROBE_CREATED_CANDIDATE":zkgpt::r7::fs::verifiedStatus)<<"\",\"n\":"<<n<<",\"grouping\":\""<<grouping<<"\",\"roots\":"<<p.roots.size()<<",\"claims\":"<<pcs.claims()<<",\"registered_rows\":"<<pcs.registered_rows<<",\"registration_response_bytes\":"<<pcs.registration_bytes<<",\"tail_rows\":"<<pcs.tail_rows<<",\"tail_response_bytes\":"<<pcs.tail_response_bytes<<",\"seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<",\"wire_bytes\":"<<io.wireBytes()<<",\"P_wire_bytes\":"<<io.proofBytes()<<",\"peak_rss_kib\":"<<r.ru_maxrss<<",\"MSM_calls\":"<<m.msm_calls<<",\"MSM_terms\":"<<m.msm_terms<<"}\n";return 0;
 }catch(const std::exception&e){std::cerr<<"REJECT_OR_INCOMPLETE "<<e.what()<<'\n';return 2;}}
