#include "paper_r7_oneblock_sumcheck.hpp"
#include "paper_r7_oneblock_vectors.hpp"
#include "paper_r7_oneblock_gkr.hpp"
#include "paper_r7_fs_io.hpp"
#include <prover.hpp>
#include "paper_r1_wire.hpp"
#include "paper_r7_native_masked.hpp"
#include "paper_r7_hyrax_a3.hpp"
#include "paper_r7_stage2_opening.hpp"
#include <chrono>
#include <sys/resource.h>
#include <iostream>
#include <iomanip>
using namespace zkgpt::r7;using namespace oneblock;
struct Observe:IO{
 IO&base;bool masked;std::string attack;bool hit=false;std::size_t rounds=0,legacy=0;std::chrono::steady_clock::time_point sigma_start;std::string sigma_role;bool sigma_pending=false;std::map<std::string,double>recurrence;
 void endSigma(){if(sigma_pending){recurrence[sigma_role]+=std::chrono::duration<double>(std::chrono::steady_clock::now()-sigma_start).count();sigma_pending=false;}}
 Observe(IO&b,bool m,std::string a):base(b),masked(m),attack(std::move(a)){}
 bool prover()const override{return base.prover();}std::string session()const override{return base.session();}Digest prefix()const override{return base.prefix();}
 Fr coin(const std::string&s)override{endSigma();return base.coin(s);}
 Bytes message(const std::string&s,const Bytes&b={})override{
  endSigma();if(s.find("/recurrence/statement")!=std::string::npos){sigma_start=std::chrono::steady_clock::now();sigma_role=s.substr(0,s.find("/round/"));}
  Bytes sent=b;bool mutate=prover() && !hit && !attack.empty() && s.find(attack)!=std::string::npos;
  if(mutate){hit=true;std::cerr<<"ATTACK_REACHED "<<s<<"\n";
   if(s.find("/coefficients")!=std::string::npos){Reader r{b};auto c0=r.scalar(),c1=r.scalar(),c2=r.scalar();r.end();Writer w;w.scalar(c0);w.scalar(c1-Fr(1));w.scalar(c2+Fr(1));sent=w.b;}
   else if(s.find("/mask-vector")!=std::string::npos){Reader r{b};auto v=r.point();r.end();Writer w;w.point(v+scalarCommit(Fr(1),Fr(0)));sent=w.b;}
   else if(s.find("/commitment")!=std::string::npos){auto h=decodeHandle(b);h.point+=scalarCommit(Fr(1),Fr(0));sent=encode(h);}
   else if(s.find("/PC")!=std::string::npos){auto pc=decodeVector(b);pc.points[0]+=scalarCommit(Fr(1),Fr(0));sent=encodeVector(pc);}
   else if(s.find("/derived")!=std::string::npos || s.find("/carry")!=std::string::npos || s.find("/original-terminal")!=std::string::npos){auto h=decodeHandle(b);h.point+=scalarCommit(Fr(1),Fr(0));sent=encode(h);}
   else {Reader r{b};auto e=r1::encode(r.scalar()+Fr(1));std::copy(e.begin(),e.end(),sent.begin());}
  }
  auto out=base.message(s,sent);
  if(s.find("/masked/round/")!=std::string::npos && s.find("/coefficients")!=std::string::npos)++rounds;
  if(s.find("/round/")!=std::string::npos && s.find("/recurrence/responses")!=std::string::npos)++legacy;
  if(s.find("/recurrence/responses")!=std::string::npos)sigma_pending=true;
  return mutate?b:out;
 }
};
int main(int argc,char**argv){try{
 need(argc==7 || argc==8,"prove|verify matrix|native sigma|masked n proof fault [attack]");
 r1::init();std::string action=argv[1],kind=argv[2],mode=argv[3],path=argv[5],fault=argv[6];auto n=std::stoull(argv[4]);
 need(n && !(n&(n-1)) && n<=(1u<<20),"test domain");need(mode=="masked" || mode=="sigma","mode");
 bool isP=action=="prove";need(isP || action=="verify","action");
 bool selected=mode=="masked";native_masked::ScopedMode selection(selected?native_masked::Mode::MaskedResearch:native_masked::Mode::Sigma);
 fs::Binding binding{"Unit","NATIVE-GKR-MASKED-v1/"+kind+"/"+mode,fs::generatorIdentity(),{}};Writer instance;instance.text(kind);instance.text(mode);instance.number(n,8);binding.public_instance=instance.b;
 std::unique_ptr<fs::FsIO>wire;if(isP)wire=std::make_unique<fs::FsProverIO>(path,binding);else wire=std::make_unique<fs::FsVerifierIO>(path,binding);
 if(isP)r1::writeFile(path+".statement.bin",binding.public_instance);
 Observe io(*wire,selected,argc==8?argv[7]:"");SessionRandom rng;Random*random=isP?&rng:nullptr;
 auto started=std::chrono::steady_clock::now();
 Value a=isP?Value([](std::size_t i){return Fr(int(i%7)+2);}):Value{},b=isP?Value([](std::size_t i){return Fr(int(i%11)+1);}):Value{};
 if(kind=="matrix"){
  auto av=exchangeVector(io,"fixture/x",n,n,a,random),bv=exchangeVector(io,"fixture/w",n,n,b,random);
  Fr sum=0;if(isP)for(std::size_t j=0;j<n;++j)sum+=a(j)*b(j);if(isP && fault=="result")sum+=Fr(1);
  auto initial=exchangeHidden(io,"fixture/initial",sum,random);
  auto reduction=selected?native_masked::matrix(io,n,initial,isP?std::vector<Value>{a,b}:std::vector<Value>{},random,"fixture/matrix"):expressionSumcheck(io,n,initial,isP?std::vector<Value>{a,b}:std::vector<Value>{},Expression{2,{{Fr(1),{0,1}}}},random,"fixture/matrix");
  for(unsigned k=0;k<2;++k)(k?bv:av).open(io,{reduction.point,reduction.endpoints[k].handle},isP?&reduction.endpoints[k]:nullptr,random,"fixture/source/"+std::to_string(k));
 }else if(kind=="native" || kind=="fconn"){
  need(n>=2,"native phase must have variable");::prover p;layeredCircuit graph;graph.init(0,3);
  auto count=kind=="fconn"?2:n;
  graph.circuit[0].ty=layerType::INPUT;graph.circuit[0].size=kind=="fconn"?3*n+2:3*n;graph.circuit[0].bit_length=r1::log2exact(n)+2;
  for(unsigned id=1;id<3;++id){auto&l=graph.circuit[id];l.ty=id==1 && kind=="fconn"?layerType::FCONN:layerType::ADD_BIAS;l.size=count;l.bit_length=r1::log2exact(count);l.zero_start_id=id==1?count:0;l.need_phase2=id==1 || kind=="fconn";}
  if(kind=="native")for(std::size_t i=0;i<n;++i){graph.circuit[1].bin_gates.emplace_back(i,i,n+i,1,0);graph.circuit[2].uni_gates.emplace_back(i,i,1,1);graph.circuit[2].uni_gates.emplace_back(i,2*n+i,0,-1);}
  else for(unsigned i=0;i<2;++i){graph.circuit[2].bin_gates.emplace_back(i,i,i,1,1);graph.circuit[2].uni_gates.emplace_back(i,3*n+i,0,-1);}
  graph.initSubset();std::vector<Fr>master(4*n,Fr(0));if(isP)for(std::size_t i=0;i<n;++i){master[i]=a(i);master[n+i]=b(i);master[2*n+i]=a(i)*b(i);}
  if(kind=="fconn"){
   auto&l=graph.circuit[1];l.bit_length_u[0]=r1::log2exact(n);l.size_u[0]=n;l.bit_length_v[0]=r1::log2exact(n)+1;l.size_v[0]=2*n;l.updateSize();for(std::size_t j=0;j<n;++j)l.ori_id_u.push_back(j);for(std::size_t j=0;j<2*n;++j)l.ori_id_v.push_back(n+j);
   if(isP){for(std::size_t j=0;j<n;++j){master[j]=a(j);master[n+j]=b(j);master[2*n+j]=b(j)+Fr(2);}
   for(unsigned col=0;col<2;++col){Fr sum=0;for(std::size_t j=0;j<n;++j)sum+=master[j]*master[n+col*n+j];master[3*n+col]=sum*sum;}}
  }
  Value val=isP?Value([master](std::size_t i){return master[i];}):Value{};(void)val;Fr source_blind=isP?rng.scalar():Fr(0);std::vector<Fr>committed=master;if(isP && fault=="source")committed[0]+=Fr(1);
  Writer pcw;if(isP)pcw.point(hyrax_a3::vectorCommit(committed,source_blind));auto pcb=io.message("fixture/master/mask-vector",pcw.b);Reader pcr{pcb};auto source=pcr.point();pcr.end();
  if(isP){p.C=graph;p.val.resize(3);p.val[0]=master;p.val[1].resize(count,Fr(0));p.val[2].resize(count,Fr(0));
   if(kind=="native")for(std::size_t i=0;i<n;++i)p.val[1][i]=a(i)*b(i);
   else for(unsigned col=0;col<2;++col)for(std::size_t j=0;j<n;++j)p.val[1][col]+=master[j]*master[n+col*n+j];
   if(fault=="carry")p.val[1][0]+=Fr(1);
  }
  auto pending=runHiddenGKR(io,graph,isP?&p:nullptr,1,random);
  need(pending.layers==2 && (kind=="native"?pending.rounds>=3*r1::log2exact(n):pending.matrices==2),"native phase/matrix/carry missing");
  unsigned j=0;for(auto&t:pending.terminals){
   const auto&l=graph.circuit[t.layer];auto map=t.port?l.ori_id_v:l.ori_id_u;auto weights=r1::weights(t.point);std::vector<Fr>functional(4*n,Fr(0));for(std::size_t k=0;k<map.size();++k)functional[map[k]]+=weights[k];
   hyrax_a3::DotWitness witness;if(isP){witness.values=committed;witness.blind=source_blind;witness.evaluation=t.value;}
   stage2::ScopedIO scoped(io,"fixture/source/"+std::to_string(j++));hyrax_a3::dotProduct(scoped,source,t.value.handle,functional,isP?&witness:nullptr,random);
  }
 }else need(false,"fixture kind");
 io.endSigma();wire->finish();std::cerr<<wire->metadataJson()<<"\n";for(auto&[role,value]:io.recurrence)std::cerr<<"ROUND_SIGMA "<<std::quoted(role)<<" "<<std::setprecision(12)<<value<<"\n";if(selected && n>1)need(io.rounds>0 && io.legacy==0,"masked mode executed old per-round Sigma");if(!selected)need(io.rounds==0,"Sigma baseline changed");
 if(argc==8)need(io.hit,"attack target missing");
 rusage usage{};getrusage(RUSAGE_SELF,&usage);
 std::cout<<std::setprecision(12)<<"RESULT {\"seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<",\"proof_bytes\":"<<wire->proofBytes()<<",\"peak_rss_kib\":"<<usage.ru_maxrss<<",\"masked_rounds\":"<<io.rounds<<",\"legacy_rounds\":"<<io.legacy<<",\"complete\":true}\n";return 0;
 }catch(const std::exception&e){std::cerr<<"REJECT "<<e.what()<<"\n";return 1;}}
