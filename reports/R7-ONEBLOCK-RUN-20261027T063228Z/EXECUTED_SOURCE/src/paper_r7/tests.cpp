#include "framework.hpp"
#include "wire.hpp"
#include "r6_adapter.hpp"
#include <iostream>
#include <functional>
using namespace zkgpt::r7;
int main() { try {
    zkgpt::r1::init();
    auto context = buildR6Context(std::string(64, 'a'));
    auto s = makeStatement(context, std::vector<Fr>(32768, Fr(0)), std::vector<Fr>(32768, Fr(0)));
    unsigned checks=0;
    auto check=[&](bool ok,const char* name){if(!ok)throw std::runtime_error(name);++checks;std::cout<<"PASS "<<name<<'\n';};
    check(validateStatement(s,context).code==Code::Ok,"valid real R6 oneblock schema");
    auto reject=[&](const char* name,const std::function<void(R7Statement&)>& change){auto q=s;change(q);check(validateStatement(q,context).code==Code::Reject,name);};
    reject("wrong version",[](auto&q){++q.version;});
    reject("wrong circuit",[](auto&q){q.circuit_id[0]='0';});
    reject("wrong layout",[](auto&q){q.layout_id="flattened-2^30";});
    reject("wrong model",[](auto&q){q.model_manifest_sha256[0]='b';});
    reject("wrong profile",[](auto&q){q.profile_sha256[0]='0';});
    reject("Full12 forbidden",[](auto&q){q.blocks=12;});
    reject("wrong base master",[](auto&q){q.master_padded=std::uint64_t(1)<<30;});
    reject("wrong X size",[](auto&q){q.x.pop_back();});
    reject("wrong Y semantics",[](auto&q){q.output_semantics="logits";});
    reject("X padding",[](auto&q){q.x[768]=Fr(1);});
    reject("Y padding",[](auto&q){q.y[768]=Fr(1);});
    reject("unsigned signed64 overflow",[](auto&q){q.x[0]=Fr(1);for(unsigned i=0;i<63;++i)q.x[0]*=Fr(2);});
    auto boundary=s;Fr limit=Fr(1);for(unsigned i=0;i<63;++i)limit*=Fr(2);
    boundary.x[0]=-limit;boundary.y[0]=limit-Fr(1);
    check(validateStatement(boundary,context).code==Code::Ok,"signed64 endpoints allowed");
    boundary.x[0]=-limit-Fr(1);check(validateStatement(boundary,context).code==Code::Reject,"signed64 lower overflow rejected");
    auto b=R7Wire::encodeStatement(s);
    check(R7Wire::encodeStatement(R7Wire::decodeStatement(b))==b,"canonical statement roundtrip");
    auto badwire=[&](Bytes bytes,const char*name){bool bad=false;try{R7Wire::decodeStatement(bytes);}catch(...){bad=true;}check(bad,name);};
    auto trailing=b;trailing.push_back(0);badwire(trailing,"trailing bytes rejected");
    badwire(Bytes(b.begin(),b.end()-1),"truncated statement rejected");
    badwire(Bytes(3*1024*1024,0),"oversize statement rejected");
    R7Transcript t(s),u(s);check(t.digest()==u.digest(),"deterministic transcript");
    auto q=s;q.y[0]=Fr(-1);R7Transcript changed(q);check(t.digest()!=changed.digest(),"transcript binds public hidden Y");
    R7OneblockVerifier verifier;
    R7Proof proof;proof.statement_digest=zkgpt::r1::hash(b);
    for(auto component:allComponents()){
        auto p=proof;
        for(auto other:allComponents())if(component!=other)p.parts[static_cast<std::size_t>(other)]=Bytes{1};
        auto result=verifier.verify(s,p,context);
        check(result.code==Code::Incomplete && result.component==component,"missing component fails closed");
    }
    for(auto component:allComponents())proof.parts[static_cast<std::size_t>(component)]=Bytes{1};
    check(verifier.verify(s,proof,context).code==Code::Unimplemented,"forged complete envelope cannot accept");
    proof.statement_digest[0]^=1;
    check(verifier.verify(s,proof,context).code==Code::Reject,"cross statement proof rejected");
    check(context.plan.master.padded<(std::size_t(1)<<30),"base master never flattened to 2^30");
    check(!context.objects.empty(),"independent auxiliary object identities");
    auto parent=[&](const std::string&id){for(auto&o:context.objects)if(o.id==id)return o.parent;throw std::runtime_error("missing auxiliary object");};
    check(parent("r7/r2/model/Exp/range/t/digit/0")=="r7/r2/model/Exp-t","Exp index range binds original t source");
    check(parent("r7/r2/model/Exp/range/x/digit/0")=="r7/r2/model/Exp/x","Exp helper range binds correct helper");
    check(parent("r7/r2/model/Exp/indexed/read")=="r7/r2/model/Exp/x","Exp memory binds canonical index helper");
    check(R7Wire::encodeProof(R7Wire::decodeProof(R7Wire::encodeProof(proof)))==R7Wire::encodeProof(proof),"proof envelope roundtrip");
    auto envelope=R7Wire::encodeProof(proof);envelope.push_back(0);bool bad_envelope=false;
    try{R7Wire::decodeProof(envelope);}catch(...){bad_envelope=true;}
    check(bad_envelope,"trailing proof envelope rejected");
    bool sanitized=false;
    try{executePrivateWitness([]{throw std::range_error("private witness index=423 value=91234567890123456789");});}
    catch(const std::exception&e){sanitized=std::string(e.what())=="R7 private witness generation/validation failed; details suppressed";}
    check(sanitized,"private witness error values never exported");
    std::cout<<"TESTS_PASSED "<<checks<<" FRAMEWORK_CONNECTED production_gate=BLOCKED_CRYPTOGRAPHIC_DESIGN may_enter_stage2=false\n";
    return 0;
} catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;} }
