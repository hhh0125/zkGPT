#include "framework.hpp"
#include "r6_adapter.hpp"
#include "sigma.hpp"
#include "wire.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
namespace zkgpt::r7 {
namespace {
namespace c=crypto;
struct SilenceStdout{int saved=-1;SilenceStdout(){std::fflush(stdout);saved=dup(1);int sink=open("/dev/null",O_WRONLY);c::need(saved>=0 && sink>=0 && dup2(sink,1)>=0,"R7 private computation output guard");close(sink);}~SilenceStdout(){std::fflush(stdout);if(saved>=0){dup2(saved,1);close(saved);}}};
c::Hidden scalarMessage(c::IO&io,const std::string&role,const Fr&v,c::Random*rng){c::Hidden h;Bytes b;if(io.prover()){h=c::hide(io.session(),role,v,*rng);b=c::encode(h.handle);}b=io.message(role,b);h.handle=c::decodeHandle(b);c::validate(h.handle,io.session(),role);return h;}
Result research(c::IO&io,const std::string&mode,std::size_t n,const std::string&attack){
 c::OSRandom rng;auto random=io.prover()?&rng:nullptr;
 if(mode=="sigma" || mode=="sigma-zero"){
  std::vector<Fr>v=mode=="sigma-zero"?std::vector<Fr>(4,Fr(0)):std::vector<Fr>{Fr(3),Fr(5),Fr(8),Fr(15)};
  if(attack=="bad-linear")v[2]+=Fr(1);
  if(attack=="bad-mul")v[3]+=Fr(1);
  std::vector<c::Hidden>w;for(unsigned k=0;k<4;++k)w.push_back(scalarMessage(io,"linear-product/h/"+std::to_string(k),v[k],random));
  c::Relations rel;rel.role="local-linear-product";for(auto&h:w)rel.handles.push_back(h.handle);rel.linear={{Fr(1),Fr(1),Fr(-1),Fr(0)}};rel.products={{0,1,3}};c::sigma(io,rel,io.prover()?&w:nullptr,random);
 }else if(mode=="opening" || mode=="opening-zero"){
  c::need(n>=16 && n<=1024,"R7 bounded opening size");c::PCSecret secret;
  if(io.prover()){std::vector<Fr>w(n,Fr(0));if(mode!="opening-zero")for(std::size_t j=0;j<n;++j)w[j]=Fr(j%19)-Fr(9);secret=c::commit(c::PCSpec::make("local-opening/W",n,n),w,rng);}
  if(io.prover() && attack=="wrong-pc")secret.public_pc.rows[0]+=c::parameters(1).scalar;
  c::openingExample(io,n,io.prover()?&secret:nullptr,random,attack=="bad-opening");
 }else{
  c::need(mode=="sumcheck2" || mode=="sumcheck3" || mode=="sumcheck-zero" || mode=="real-FC0","R7 unsupported local interactive mode");
  bool real=mode=="real-FC0";std::size_t degree=mode=="sumcheck3"?3:2;if(real)n=1024;else c::need(n && n<=256 && !(n&(n-1)),"R7 bounded Sumcheck size");
  std::vector<Fr>w(degree*n+1,Fr(0));std::string role=real?"real-oneblock-FC0-layer4-row0-col0":"bounded-"+mode;
  if(real){const std::string manifest="reports/R6A/oneblock-model-manifest-v1.txt",pin="3ab43263570f08a2fe314bca99f2f95c11020abae17eca587f1717f52d298073";R7Context context;R7WitnessStore store;R7Statement statement;
   {SilenceStdout quiet;checkManifest(manifest,pin);context=buildR6Context(pin);if(io.prover()){auto output=R7OneblockProver{}.run(context,manifest,store);c::need(output.result.code==Code::Incomplete,"R7 real witness framework result");statement=output.statement;auto values=store.firstFCContraction(context);for(std::size_t j=0;j<n;++j){w[j]=values.activation[j];w[n+j]=values.weight[j];}w[2*n]=values.output;}}
   auto b=io.message("real-FC0/R7Statement",io.prover()?R7Wire::encodeStatement(statement):Bytes{});auto actual=R7Wire::decodeStatement(b);c::need(validateStatement(actual,context).code==Code::Ok,"R7 real FC statement/profile/model/layout rejected");
  }else if(io.prover()){
   for(std::size_t k=0;k<degree;++k)for(std::size_t j=0;j<n;++j)w[k*n+j]=mode=="sumcheck-zero"?Fr(0):Fr((j+3*k)%13)-Fr(6);
   for(std::size_t j=0;j<n;++j){Fr product=1;for(std::size_t k=0;k<degree;++k)product*=w[k*n+j];w[degree*n]+=product;}
  }
  c::PCSecret secret;Bytes bytes;if(io.prover()){secret=c::commit(c::PCSpec::make(role,w.size(),4*n),w,rng);bytes=c::encode(secret.public_pc);}
  bytes=io.message(role+"/PC",bytes);auto pc=c::decodePC(bytes);c::need(pc.spec.role==role && pc.spec.actual==degree*n+1 && pc.spec.padded==4*n,"R7 contraction PC identity/layout mismatch");
  c::committedSumcheck(io,{pc,n,degree,degree*n,role},io.prover()?&secret:nullptr,random,attack);
 }
 c::bindPublic(io,"LOCAL_CRYPTO_TESTED/complete",Bytes{'d','o','n','e'});
 return {Code::Ok,Component::CommittedSumcheck,"LOCAL_CRYPTO_TESTED honest interactive subprotocol only; Oneblock INCOMPLETE"};
}
}
Result R7OneblockProver::runLocalInteractive(crypto::IO&io,const std::string&m,std::size_t n,const std::string&a)const{crypto::need(io.prover(),"R7 prover role required");return research(io,m,n,a);}
Result R7OneblockVerifier::verifyLocalInteractive(crypto::IO&io,const std::string&m,std::size_t n)const{crypto::need(!io.prover(),"R7 independent verifier role required");return research(io,m,n,"");}
}
