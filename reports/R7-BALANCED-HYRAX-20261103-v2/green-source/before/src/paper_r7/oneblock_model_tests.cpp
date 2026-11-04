#include "oneblock_model.hpp"
#include <algorithm>
#include <functional>
#include <iostream>
using namespace zkgpt::r7::crypto;
namespace model=zkgpt::r7::oneblock::model;
namespace a3=zkgpt::r7::hyrax_a3;
namespace {
struct RandomSequence:Random {unsigned next=1;void fill(std::uint8_t*p,std::size_t n)override{auto b=r1::encode(Fr(next++));need(n==b.size(),"test entropy size");std::copy(b.begin(),b.end(),p);}};
struct Tape:IO {
 bool p,zeroCoin=false;std::vector<std::pair<std::string,Bytes>> records;std::size_t at=0;unsigned draws=0;
 explicit Tape(bool prover):p(prover){}
 bool prover()const override{return p;}std::string session()const override{return std::string(64,'c');}Digest prefix()const override{return {};}
 Bytes message(const std::string&label,const Bytes&b={})override{if(p){records.push_back({label,b});return b;}need(at<records.size() && records[at].first==label,"test message order");return records[at++].second;}
 Fr coin(const std::string&)override{return zeroCoin?Fr(0):Fr(++draws+2);}
};
unsigned checks=0;
void check(bool ok,const char*why){++checks;need(ok,why);}
void reject(const std::function<void()>&f,const char*why){++checks;try{f();}catch(const std::exception&e){need(std::string(e.what()).find(why)!=std::string::npos,e.what());return;}throw std::runtime_error("expected rejection");}
model::Trust trust(){
 model::Trust t;t.anchor.profile="test-operator";t.anchor.profile_hash=std::string(64,'1');t.anchor.manifest_hash=std::string(64,'2');t.anchor.circuit=std::string(64,'3');t.anchor.model_id="test-r6-operator";
 t.anchor.w.spec=r1::PCSpec::make("test/R6/W",4,8,r1::Layout::RowMajor);
 const auto&g=r1::generators(t.anchor.w.spec).g;
 // Independent row-major rows for W=(2,3,5,7), no legacy commit/open call.
 G1 zero;zero.clear();t.anchor.w.rows={g[0]*Fr(2)+g[1]*Fr(3)+g[2]*Fr(5)+g[3]*Fr(7),zero};
 t.anchor_hash=zkgpt::r5b::digestHex(zkgpt::r6::encodeAnchor(t.anchor));t.manifest_hash=t.anchor.manifest_hash;return t;
}
}
int main(){try{
 r1::init();auto t=trust();RandomSequence rng;Tape p(true);
 auto s=model::rerandomize(t,p.session(),{Fr(2),Fr(3),Fr(5),Fr(7)},rng);
 check(s.model.rows[0]!=t.anchor.w.rows[0],"model rows must hide R6 anchor");
 auto registered=model::registerModel(p,t,&s,&rng);Tape v(false);v.records=p.records;auto verified=model::registerModel(v,t,nullptr,nullptr);
 check(model::encode(registered)==model::encode(verified) && v.at==v.records.size(),"verifier independently registers hidden R6 rows");
 auto tampered=p.records;Reader response{tampered[3].second};Writer change;change.scalar(response.scalar()+Fr(1));change.scalar(response.scalar());response.end();tampered[3].second=change.b;
 Tape rowbad(false);rowbad.records=tampered;reject([&]{model::registerModel(rowbad,t,nullptr,nullptr);},"row response equation");
 auto old=p.records;old[0].second[4]^=1;Tape oldv(false);oldv.records=old;reject([&]{model::registerModel(oldv,t,nullptr,nullptr);},"unknown suite/version");
 Tape zero(false);zero.records=p.records;zero.zeroCoin=true;reject([&]{model::registerModel(zero,t,nullptr,nullptr);},"zero verifier challenge");
 auto h=hide(p.session(),"model/query",Fr(47),rng);Query q{{Fr(2),Fr(3),Fr(0)},h.handle};
 // W(r0=2,r1=3)=2*(-1)*(-2)+3*2*(-2)+5*(-1)*3+7*2*3=19.
 h.value=19;h.handle.point=scalarCommit(h.value,h.blind);q.evaluation=h.handle;
 Tape op(true);model::opening(op,t,s.model,q,&s,&h,&rng,"test");Tape ov(false);ov.records=op.records;model::opening(ov,t,s.model,q,nullptr,nullptr,nullptr,"test");check(ov.at==ov.records.size(),"hidden opening with R6 role-specific bases");
 auto falseH=h;falseH.value+=Fr(1);falseH.handle.point=scalarCommit(falseH.value,falseH.blind);auto falseQ=q;falseQ.evaluation=falseH.handle;Tape fp(true);model::opening(fp,t,s.model,falseQ,&s,&falseH,&rng,"test");Tape fv(false);fv.records=fp.records;reject([&]{model::opening(fv,t,s.model,falseQ,nullptr,nullptr,nullptr,"test");},"Schnorr equation");
 auto wrong=t;wrong.anchor.w.spec.role="substituted/R6/W";wrong.anchor_hash=zkgpt::r5b::digestHex(zkgpt::r6::encodeAnchor(wrong.anchor));Tape bv(false);bv.records=op.records;reject([&]{model::opening(bv,wrong,s.model,q,nullptr,nullptr,nullptr,"test");},"trust/session/spec mismatch");
 // Replacing the trusted R1 basis with the R7 basis must fail the separate
 // vector-knowledge equation, even when the scalar claim itself is true.
 a3::DotWitness basisW{{Fr(4),Fr(9),Fr(0),Fr(0)},Fr(3),h};auto wrongXi=a3::vectorCommit(basisW.values,basisW.blind);std::vector<Fr>coeff{Fr(-2),Fr(3),Fr(0),Fr(0)};
 Tape bp(true);a3::dotProductTrustedR6Basis(bp,t.anchor.w.spec,wrongXi,h.handle,coeff,&basisW,&rng);Tape judge(false);judge.records=bp.records;reject([&]{a3::dotProductTrustedR6Basis(judge,t.anchor.w.spec,wrongXi,h.handle,coeff,nullptr,nullptr);},"vector knowledge equation");
 Tape legacy(false);legacy.records=bp.records;reject([&]{a3::dotProduct(legacy,wrongXi,h.handle,coeff,nullptr,nullptr);},"test message order");
 reject([&]{a3::generators(2048);},"dimension");check(a3::maxDot==1024,"old A3 cap retained");
 reject([&]{model::rerandomize(t,p.session(),{Fr(2),Fr(3),Fr(5),Fr(7),Fr(1),Fr(0),Fr(0),Fr(0)},rng);},"nonzero padded W tail");
 check(s.value(4).isZero(),"model zero padding oracle");
 // The second 64-row chunk must be verified too, including zero-padded rows.
 auto chunks=t;chunks.anchor.w.spec=r1::PCSpec::make("test/R6/chunks",4,16384,r1::Layout::RowMajor);G1 identity;identity.clear();chunks.anchor.w.rows.assign(128,identity);const auto&cg=r1::generators(chunks.anchor.w.spec).g;chunks.anchor.w.rows[0]=cg[0]*Fr(2)+cg[1]*Fr(3)+cg[2]*Fr(5)+cg[3]*Fr(7);chunks.anchor_hash=zkgpt::r5b::digestHex(zkgpt::r6::encodeAnchor(chunks.anchor));
 Tape cp(true);auto cs=model::rerandomize(chunks,cp.session(),{Fr(2),Fr(3),Fr(5),Fr(7)},rng);model::registerModel(cp,chunks,&cs,&rng);check(cp.records.size()==8,"two 64-row registration chunks");Tape cv(false);cv.records=cp.records;model::registerModel(cv,chunks,nullptr,nullptr);check(cv.at==cv.records.size(),"all rows of both chunks verified");
 auto last=cp.records;Reader lastResponse{last[6].second};Writer badResponse;for(unsigned i=0;i<64;++i){auto z=lastResponse.scalar();badResponse.scalar(i==63?z+Fr(1):z);}lastResponse.end();last[6].second=badResponse.b;Tape lastV(false);lastV.records=last;reject([&]{model::registerModel(lastV,chunks,nullptr,nullptr);},"row response equation");
 std::cout<<"ONEBLOCK_MODEL_UNIT_PASSED "<<checks<<"\n";return 0;
}catch(const std::exception&e){std::cerr<<"ONEBLOCK_MODEL_UNIT_FAILED "<<e.what()<<'\n';return 1;}}
