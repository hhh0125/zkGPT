#include "stage2_driver.hpp"
#include <chrono>
#include <iostream>
#include <algorithm>
namespace zkgpt::r7::stage2 {
namespace {
using Clock=std::chrono::steady_clock;
class ReplayMasks final:public Random {
 OSRandom source_;std::vector<Bytes>tape_;std::size_t at_=0,opens_=0;bool enabled_,capture_=false,replay_=false;
 public:explicit ReplayMasks(bool enabled):enabled_(enabled){}
 void begin(){if(!enabled_)return;if(++opens_==1){capture_=true;tape_.clear();}else{capture_=false;replay_=true;at_=0;}}
 void fill(std::uint8_t*out,std::size_t n)override{if(replay_){need(at_<tape_.size() && tape_[at_].size()==n,"test replay entropy exhausted");std::copy(tape_[at_].begin(),tape_[at_].end(),out);++at_;}else{source_.fill(out,n);if(capture_)tape_.emplace_back(out,out+n);}}
};
class AttackIO final:public IO {
 IO&base_;std::string attack_;ReplayMasks&rng_;const Registry&alternate_;bool done_=false;Bytes firstBatch_;
 public:AttackIO(IO&io,const std::string&a,ReplayMasks&rng,const Registry&alt):base_(io),attack_(a),rng_(rng),alternate_(alt){}
 bool prover()const override{return base_.prover();}std::string session()const override{return base_.session();}Digest prefix()const override{return base_.prefix();}
 Bytes message(const std::string&label,const Bytes&bytes={})override{
  auto out=bytes;auto wirelabel=label;
  if(prover() && label.find("/A3/PC-statement")!=std::string::npos)rng_.begin();
  if(prover() && !done_){bool changed=false;
   if(label=="S2/registry/approved"){
    if(attack_=="wrong-registry"){out=encodeRegistry(alternate_);changed=true;}
    if(attack_=="rebind"){auto r=decodeRegistry(bytes);r.pc.rows=alternate_.pc.rows;out=encodeRegistry(r);changed=true;}
    if(attack_=="commitment-bytes"){out.back()^=1;changed=true;}
    if(attack_=="wrong-object" || attack_=="wrong-generators" || attack_=="wrong-suite" || attack_=="wrong-shape"){
     Reader r{out};std::size_t at=0;unsigned index=attack_=="wrong-object"?6:attack_=="wrong-generators"?4:attack_=="wrong-suite"?2:8;
     for(unsigned i=0;i<index;++i)r.text();at=r.at+(attack_=="wrong-shape"?4:4);out.at(at)^=1;changed=true;
    }
   }
   if(label.rfind("S2/batch/",0)==0){
    if(firstBatch_.empty())firstBatch_=out;else if(attack_=="cross-batch"){out=firstBatch_;changed=true;}
    if(attack_=="registry-digest"){Reader r{out};r.text();r.text();out.at(r.at+4)^=1;changed=true;}
    if(attack_=="incomplete")throw std::runtime_error("test deliberately incomplete opening");
   }
   if(label.find("/A3/statement")!=std::string::npos && attack_=="old-csipa"){Reader r{out};r.text();Writer w;w.text(crypto::suite);w.raw(Bytes(out.begin()+r.at,out.end()));out=w.b;changed=true;}
   if(label.find("/A3/reduce/0/cross")!=std::string::npos && attack_=="truncate"){out.pop_back();changed=true;}
   if(label=="S2/committed-sumcheck/terminal/commitment"){
    if(attack_=="terminal-handle"){auto h=decodeHandle(out);h.point+=parameters(1).scalar;out=encode(h);changed=true;}
    if(attack_=="cross-session-terminal"){auto h=decodeHandle(out);h.session=std::string(64,'0');out=encode(h);changed=true;}
   }
   if(label=="S2/committed-sumcheck/round/0/c/0/commitment" && attack_=="sc-order"){wirelabel+="/wrong-order";changed=true;}
   if(label=="S2/committed-sumcheck/opening-backend" && attack_=="old-sc-backend"){Reader r{out};r.text();Writer w;w.text("CSIPA-RESEARCH-v1");w.raw(Bytes(out.begin()+r.at,out.end()));out=w.b;changed=true;}
   if(label=="S2/activation/0/PC" && attack_=="factor-substitution"){auto p=decodePC(out);p.rows[0]+=parameters(1).scalar;out=encode(p);changed=true;}
   done_|=changed;
  }
  auto incoming=base_.message(wirelabel,out);return prover()?bytes:incoming;
 }
 Fr coin(const std::string&label)override{auto c=base_.coin(label);if(prover() && !done_ && ((attack_=="opening-challenge" && label.find("/A3/reduce/0/c")!=std::string::npos) || (attack_=="sc-challenge" && label=="S2/committed-sumcheck/round/0/r"))){c+=Fr(1);done_=true;}return c;}
};
Hidden hiddenMessage(IO&io,const std::string&role,const Fr&value,Random*rng){Hidden h;Bytes b;if(io.prover()){h=hide(io.session(),role,value,*rng);b=encode(h.handle);}b=io.message(role+"/commitment",b);h.handle=decodeHandle(b);validate(h.handle,io.session(),role);return h;}
PC pcMessage(IO&io,const std::string&role,const PCSecret*s,std::size_t n){auto b=io.message(role+"/PC",io.prover()?encode(s->public_pc):Bytes{});auto p=decodePC(b);need(p.spec.role==role && p.spec.actual==n && p.spec.padded==n,"S2 operand PC identity/shape mismatch");return p;}
}
ExperimentStats experiment(PipeIO&pipe,const TrustedRegistry*trusted,PrivateFixture*fixture,const std::string&mode,std::size_t k,const std::string&attack){
 need(k==1 || k==2 || k==4,"S2 K must be1/2/4");need(mode=="s2-registry" || mode=="s2-multi" || mode=="s2-sc2" || mode=="s2-sc3" || mode=="s2-sc2-preflight" || mode=="s2-sc3-preflight" || mode=="s2-real" || mode=="s2-zero-a","S2 unsupported experiment");
 ReplayMasks rng(attack=="repeat-masks");Registry dummy;if(!pipe.prover())dummy=trusted->value();const Registry&other=pipe.prover()?fixture->alternate:dummy;AttackIO io(pipe,attack,rng,other);auto random=io.prover()?&rng:nullptr;ExperimentStats stats;
 auto timed=[&](const std::string&name,const auto&fn){auto start=Clock::now();auto bytes=pipe.proofBytes(),calls=metrics().msm_calls;fn();stats.phases.push_back({name,std::chrono::duration<double>(Clock::now()-start).count(),pipe.proofBytes()-bytes,metrics().msm_calls-calls});};
 auto start=Clock::now();auto before=pipe.proofBytes(),calls=metrics().msm_calls;
 if(io.prover() && attack=="padding-attack"){fixture->primary=fixture->bad_tail;fixture->weight=fixture->bad_tail_weight;}
 if(io.prover() && attack=="bad-registration")fixture->weight.values[0]+=Fr(1);
 RegisteredCW cw=io.prover()?RegisteredCW::prover(io,fixture->primary,fixture->weight,rng):RegisteredCW::verifier(io,*trusted);
 stats.phases.push_back({"registration",std::chrono::duration<double>(Clock::now()-start).count(),pipe.proofBytes()-before,metrics().msm_calls-calls});stats.registry_digest=registryDigest(cw.approved());
 if(mode=="s2-multi"){
  for(unsigned batch=0;batch<2;++batch){auto id="batch"+std::to_string(batch);std::vector<Query>q;std::vector<Hidden>h;
   auto batchStart=Clock::now();auto batchBytes=pipe.proofBytes(),batchMsm=metrics().msm_calls;
   for(std::size_t i=0;i<k;++i){std::vector<Fr>point;for(unsigned bit=0;bit<10;++bit)point.push_back(io.coin("S2/query/"+id+"/"+std::to_string(i)+"/"+std::to_string(bit)));
    Fr value=io.prover()?r1::mle(fixture->weight.values,point):Fr(0);if(io.prover() && attack=="bad-evaluation" && i==0)value+=Fr(1);auto scalar=hiddenMessage(io,"S2/hidden/"+id+"/"+std::to_string(i),value,random);q.push_back({point,scalar.handle});h.push_back(scalar);
   }
   if(io.prover() && attack=="swap-handles" && k>=2){std::swap(h[0],h[1]);q[0].evaluation=h[0].handle;q[1].evaluation=h[1].handle;}
   if(io.prover() && attack=="swap-query" && k>=2)std::swap(q[0].point,q[1].point);
   if(io.prover() && attack=="other-opening-commitment")fixture->weight.values=fixture->alternate_weight.values;
   cw.openFixed(id,q,io.prover()?&fixture->weight:nullptr,io.prover()?&h:nullptr,random);stats.phases.push_back({id,std::chrono::duration<double>(Clock::now()-batchStart).count(),pipe.proofBytes()-batchBytes,metrics().msm_calls-batchMsm});
  }
 }else if(mode=="s2-zero-a"){
  auto h=hiddenMessage(io,"S2/zero-a",Fr(0),random);hyrax_a3::dotProduct(io,cw.approved().pc.rows[0],h.handle,std::vector<Fr>(32,Fr(0)),nullptr,random);
 }else if(mode!="s2-registry"){
  const bool real=mode=="s2-real";const std::size_t degree=(mode=="s2-sc3" || mode=="s2-sc3-preflight")?3:2,n=1024;SumcheckWitness witness;witness.factors.assign(degree,std::vector<Fr>(n,Fr(0)));std::vector<PCSecret>secrets(degree);std::vector<PC>pcs;
  if(io.prover()){
   witness.factors[1]=fixture->weight.values;
   for(std::size_t factor=0;factor<degree;++factor)if(factor!=1){for(std::size_t j=0;j<n;++j)witness.factors[factor][j]=real?fixture->activation[j]:Fr((j+3*factor)%13)-Fr(6);secrets[factor]=commit(PCSpec::make("S2/activation/"+std::to_string(factor),n,n),witness.factors[factor],rng);}
   if(attack=="wrong-weight")fixture->weight.values[0]+=Fr(1);
   if(attack=="wrong-weight")witness.factors[1]=fixture->weight.values;
   if(attack=="wrong-activation")witness.factors[0][0]+=Fr(1);
   for(std::size_t j=0;j<n;++j){Fr product=1;for(auto&f:witness.factors)product*=f[j];witness.claim+=product;}
   if(real && attack!="wrong-weight" && attack!="wrong-activation")witness.claim=fixture->output;
   if(attack=="wrong-output" || attack=="wrong-claim" || attack=="opening-valid-bad-sumcheck")witness.claim+=Fr(1);
  }
  for(std::size_t factor=0;factor<degree;++factor){if(factor==1)pcs.push_back(cw.approved().pc);else pcs.push_back(pcMessage(io,"S2/activation/"+std::to_string(factor),io.prover()?&secrets[factor]:nullptr,n));}
  PCSecret outputSecret;if(io.prover())outputSecret=commit(PCSpec::make("S2/output",1,1),{witness.claim},rng);auto output=pcMessage(io,"S2/output",io.prover()?&outputSecret:nullptr,1);
  if(mode=="s2-sc2-preflight" || mode=="s2-sc3-preflight"){
   auto scalar=hiddenMessage(io,"S2/preflight/claim",io.prover()?witness.claim:Fr(0),random);std::vector<Hidden>h{scalar};cw.openInference(output,"preflight",{{{},scalar.handle}},io.prover()?&outputSecret:nullptr,io.prover()?&h:nullptr,random);std::cerr<<"PREFLIGHT_A3_VERIFIED\n";
  }
  std::vector<const PCSecret*>pointers;if(io.prover())for(std::size_t factor=0;factor<degree;++factor)pointers.push_back(factor==1?&fixture->weight:&secrets[factor]);
  timed(real?"real-FC-proof":"committed-sumcheck",[&]{committedA3Sumcheck(io,cw,pcs,output,pointers,io.prover()?&outputSecret:nullptr,io.prover()?&witness:nullptr,random,attack=="bad-recursion"?"bad-recursion":attack=="bad-mul"?"bad-mul":attack=="bad-endpoint"?"bad-endpoint":"");});
 }
 bindPublic(io,"S2/LOCAL_CRYPTO_TESTED/complete",Bytes{'d','o','n','e'});return stats;
}
}
