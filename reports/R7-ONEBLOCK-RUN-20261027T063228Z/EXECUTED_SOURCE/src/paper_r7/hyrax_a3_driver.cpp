#include "hyrax_a3_driver.hpp"
#include "hyrax_a3.hpp"
#include <algorithm>
namespace zkgpt::r7::hyrax_a3 {
namespace {
class RecordingRandom final:public Random{
 OSRandom source_;
 public:Fr previous=0,last=0;
 void fill(std::uint8_t*out,std::size_t n)override{source_.fill(out,n);Fr f;if(f.deserialize(out,n)==n && f.isValid() && r1::encode(f)==Bytes(out,out+n)){previous=last;last=f;}}
};
class AttackIO final:public IO{
 IO&base_;std::string attack_;RecordingRandom&rng_;bool done_=false;
 public:AttackIO(IO&base,const std::string&a,RecordingRandom&rng):base_(base),attack_(a),rng_(rng){}
 bool prover()const override{return base_.prover();}std::string session()const override{return base_.session();}Digest prefix()const override{return base_.prefix();}
 Bytes message(const std::string&label,const Bytes&b={})override{
  auto out=b;
  if(prover() && !done_){bool change=false;
   if(label=="A3/statement" && (attack_=="old-suite" || attack_=="unknown-version" || attack_=="swap-handle")){
    if(attack_=="old-suite"){Reader r{out};r.text();Writer w;w.text(crypto::suite);w.raw(Bytes(out.begin()+r.at,out.end()));out=w.b;}
    else if(attack_=="unknown-version"){Reader r{out};r.text();out.at(r.at+4)^=1;}
    else{Reader r{out};for(unsigned k=0;k<4;++k)r.text();r.raw(32);r.number();r.point();r.text();r.text();r.text();auto at=r.at;auto point=r.point();auto changed=r1::encode(point+parameters(1).scalar);std::copy(changed.begin(),changed.end(),out.begin()+at);}
    change=true;
   }
   if(label=="A3/knowledge/responses" && (attack_=="bad-vector-knowledge" || attack_=="bad-scalar-knowledge")){
    auto at=attack_=="bad-vector-knowledge"?std::size_t(0):out.size()-32;auto f=r1::decodeFr(Bytes(out.begin()+at,out.begin()+at+32));auto bytes=r1::encode(f+Fr(1));std::copy(bytes.begin(),bytes.end(),out.begin()+at);change=true;
   }
   if(label=="A3/reduce/0/cross" && (attack_=="bad-cross-blind" || attack_=="unblinded-cross")){
    Reader r{out};auto l=r.point(),right=r.point();Writer w;w.point(attack_=="unblinded-cross"?l-parameters(1).hiding*rng_.previous:l+parameters(1).hiding);w.point(right);out=w.b;change=true;
   }
   if(label=="A3/final/z1-z2" && attack_=="bad-final-blind"){Reader r{out};auto z1=r.scalar(),z2=r.scalar();Writer w;w.scalar(z1);w.scalar(z2+Fr(1));out=w.b;change=true;}
   if(label=="A3/reduce/0/cross" && attack_=="truncate"){out.pop_back();change=true;}
   if(label=="A3/reduce/0/cross" && attack_=="noncanonical-point"){std::fill(out.begin(),out.begin()+32,255);change=true;}
   if(label=="A3/final/z1-z2" && attack_=="noncanonical-scalar"){std::fill(out.begin(),out.begin()+32,255);change=true;}
   if(label=="A3/final/z1-z2" && attack_=="trailing"){out.push_back(0);change=true;}
   done_|=change;
  }
  auto received=base_.message(label,out);
  // Malicious prover keeps its own genuine state while corrupting wire messages.
  return prover()?b:received;
 }
 Fr coin(const std::string&label)override{auto c=base_.coin(label);if(prover() && !done_ && ((attack_=="fold-challenge" && label=="A3/reduce/0/c") || (attack_=="final-challenge" && label=="A3/final/c"))){c+=Fr(1);done_=true;}return c;}
};
}
void experiment(PipeIO&pipe,const std::string&mode,std::size_t n,const std::string&attack){
 need(n && n<=1024 && !(n&(n-1)),"A3 experiment dimension");
 need(mode=="a3-dot" || mode=="a3-zero" || mode=="a3-pc" || mode=="a3-pc-zero" || mode=="a3-pc-padded","A3 experiment unknown mode");
 RecordingRandom rng;AttackIO io(pipe,attack,rng);auto random=io.prover()?&rng:nullptr;const bool pcMode=mode=="a3-pc" || mode=="a3-pc-zero" || mode=="a3-pc-padded",zero=mode=="a3-zero" || mode=="a3-pc-zero";
 const auto actual=mode=="a3-pc-padded"?n-3:n;need(actual>0 && actual<=n,"A3 padded fixture dimension");
 std::vector<Fr>x(n,Fr(0));if(io.prover() && !zero && attack!="cancellation")for(std::size_t i=0;i<n;++i)x[i]=Fr(i%19)-Fr(9);
 DotWitness w;PCSecret secret;PC pc;G1 xi;Bytes b;
 if(io.prover()){
  if(pcMode){secret=commit(PCSpec::make("local-opening/W",actual,n),std::vector<Fr>(x.begin(),x.begin()+actual),rng);x=secret.values;if(attack=="wrong-commitment")secret.public_pc.rows[0]+=parameters(1).scalar;b=encode(secret.public_pc);}
  else{w.values=x;w.blind=rng.scalar();xi=vectorCommit(x,w.blind);if(attack=="wrong-commitment")xi+=parameters(1).scalar;if(attack=="generator-shift")xi+=parameters(1).scalar*Fr(17);if(attack=="cancellation"){w.blind=-Fr(13);xi=scalarCommit(Fr(17),-Fr(13));}Writer out;out.point(xi);b=out.b;}
 }
 b=io.message(pcMode?"A3/input-PC":"A3/input-xi",b);
 if(pcMode){pc=decodePC(b);need(pc.spec.role=="local-opening/W" && pc.spec.actual==actual && pc.spec.padded==n,"A3 expected PC identity/layout");}
 else{Reader r{b};xi=r.point();r.end();}
 std::vector<Fr>point;for(unsigned i=0;i<r1::log2exact(n);++i)point.push_back(io.coin("local-opening/query/0/"+std::to_string(i)));
 auto a=r1::weights(point);Hidden evaluation;
 if(io.prover()){
  auto value=r1::mle(x,point)+Fr(attack=="bad-evaluation"?1:0);if(attack=="generator-shift")value-=Fr(17);evaluation=hide(io.session(),"local-opening/hidden-evaluation/0",value,rng);if(attack=="cancellation"){evaluation.value=-Fr(17);evaluation.blind=13;evaluation.handle.point=scalarCommit(evaluation.value,evaluation.blind);}b=encode(evaluation.handle);
 }
 b=io.message("A3/input-hidden-evaluation",io.prover()?b:Bytes{});evaluation.handle=decodeHandle(b);validate(evaluation.handle,io.session(),"local-opening/hidden-evaluation/0");
 if(pcMode)opening(io,pc,{point,evaluation.handle},io.prover()?&secret:nullptr,io.prover()?&evaluation:nullptr,random);
 else{w.evaluation=evaluation;dotProduct(io,xi,evaluation.handle,a,io.prover()?&w:nullptr,random);}
 bindPublic(io,"A3/LOCAL_CRYPTO_TESTED/complete",Bytes{'d','o','n','e'});
}
}
