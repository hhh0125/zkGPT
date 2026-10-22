#include "protocol.hpp"
#include "gkr_serialization.hpp"
#include "hyrax_opening.hpp"
#include "generator_cache.hpp"
#ifndef R1_VERIFY_ONLY
#include "gkr_proof_builder.hpp"
#endif
namespace zkgpt::r2 {
namespace {
void context(const layeredCircuit &circuit,const GKRPublicStatement &g,
    const r1::LookupBatchSpec &s,r1::Fs &t) {
    s.validate();r1::require(g.val0_log_size>=0 && g.val0_log_size<=28 &&
        s.input.master.spec.padded==(std::size_t(1)<<g.val0_log_size),"R2 master/GKR shape");
    // Old GKR input binding uses row-major PC. New master may have either
    // layout; each opening separately authenticates the same logical MLE.
    const auto fp=fingerprintCircuit(circuit);
    t.bytes("R2.circuit",r1::Bytes(fp.begin(),fp.end()));
    t.integer("R2.gkr.version",g.protocol_version);
    auto metadata=g;metadata.integration_context={};
    const auto initial=initializeGKRTranscript(metadata,fp).stateDigest();
    t.bytes("R2.gkr.statement",r1::Bytes(initial.begin(),initial.end()));
    t.scalar("R2.gkr.expected-output",g.output_evaluation);
    // prove/verifyLookupLocals appends every lookup identity before challenges.
}
Transcript openingTranscript(r1::Fs &t) {
    Transcript out("zkGPT.paper-r2.cross-domain-opening.v1");
    const auto digest=t.digest();out.appendBytes("R2.parent",digest.data(),digest.size());return out;
}
void finishOpening(r1::Fs &t,const Transcript &opening) {
    const auto digest=opening.stateDigest();t.bytes("R2.GKR-master-opening",r1::Bytes(digest.begin(),digest.end()));
}
std::vector<GKRInputTerminalClaim> checkedGkr(const layeredCircuit &c,const GKRPublicStatement &s,const GKRProof &p) {
    std::vector<GKRInputTerminalClaim> claims;std::string error;
    const bool valid=verifyGKRWithClaims(c,s,p,claims,&error);
    r1::require(valid,"R2 GKR: "+error);return claims;
}
std::vector<r1::CheckedInputClaim> mappedClaims(const layeredCircuit &c,const std::vector<GKRInputTerminalClaim> &claims) {
    std::vector<r1::CheckedInputClaim> out;
    for(const auto &q:claims){
        const auto &l=c.circuit.at(q.layer_index);const auto &source=q.port?l.ori_id_v:l.ori_id_u;
        r1::require(q.actual_size<=source.size() && q.padded_size==(std::size_t(1)<<q.point.size()),"R2 terminal mapping shape");
        r1::CheckedInputClaim a{q.point,q.value,{}};
        for(std::size_t j=0;j<q.actual_size;++j){
            const std::size_t index=source[j];
            if(!a.spans.empty() && a.spans.back().offset+a.spans.back().count==index)++a.spans.back().count;
            else a.spans.push_back({index,1,Fr(0)});
        }
        out.push_back(std::move(a));
    }return out;
}

}
#ifndef R1_VERIFY_ONLY
Proof proveR2(::prover &pr,GKRPublicStatement &g,
    const r1::LookupBatchSpec &s,const std::vector<Fr> &w,const std::vector<std::vector<Fr>> &children) {
    r1::Fs t("zkGPT.paper-r2.GKR-Lasso-consistency.v3");context(pr.C,g,s,t);
    Proof p;p.lookup=r1::proveLookupLocals(s,children,t);
    g.integration_context=t.digest();
    r1::require(g.integration_context!=std::array<std::uint8_t,32>{},"R2 zero GKR context");
    GKRProofBuilder builder(&pr,pr.C,g.model_shape,g.val0_generator_domain,nullptr,&g.integration_context);
    builder.build();p.gkr=builder.proof();
    const auto &actual=builder.publicStatement();
    r1::require(initializeGKRTranscript(g,g.circuit_fingerprint).stateDigest()==
        initializeGKRTranscript(actual,actual.circuit_fingerprint).stateDigest() &&
        g.output_evaluation==actual.output_evaluation,"R2 GKR prover statement mismatch");
    const auto claims=checkedGkr(pr.C,g,p.gkr);
    const auto digest=r1::hash(serializeGKRProof(p.gkr));
    t.bytes("R2.gkr.verified-proof",r1::Bytes(digest.begin(),digest.end()));
    const auto r=r1::point(t,g.val0_log_size,"R2.common-master.point");
    p.master_binding=r1::open(s.input.master.spec,s.input.master,w,r,t,"R2.new-master");
    auto old=openingTranscript(t);const auto &gens=getGeneratorSet(g.val0_generator_domain);
    p.gkr_input_binding=hyraxMleOpenProveFrRowMajor(w,g.val0_commitment,r,gens.generators,gens.u,
        p.master_binding.value,old,"R2.GKR-master");
    finishOpening(t,old);p.lookup.input=r1::proveMappedInput(s.input,w,children,t,mappedClaims(pr.C,claims));return p;
}
#endif
std::vector<GKRInputTerminalClaim> verifyR2(const layeredCircuit &circuit,const GKRPublicStatement &g,
    const r1::LookupBatchSpec &s,const Proof &p) {
    r1::Fs t("zkGPT.paper-r2.GKR-Lasso-consistency.v3");context(circuit,g,s,t);
    r1::verifyLookupLocals(s,p.lookup,t);
    r1::require(g.integration_context==t.digest() &&
        g.integration_context!=std::array<std::uint8_t,32>{},"R2 GKR challenge context");
    auto claims=checkedGkr(circuit,g,p.gkr);
    const auto digest=r1::hash(serializeGKRProof(p.gkr));
    t.bytes("R2.gkr.verified-proof",r1::Bytes(digest.begin(),digest.end()));
    const auto r=r1::point(t,g.val0_log_size,"R2.common-master.point");
    r1::verify(s.input.master.spec,s.input.master,r,p.master_binding,t,"R2.new-master");
    auto old=openingTranscript(t);const auto &gens=getGeneratorSet(g.val0_generator_domain);std::string error;
    const bool valid=hyraxMleOpenVerifyRowMajor(g.val0_commitment,r,gens.generators,gens.u,
        p.master_binding.value,p.gkr_input_binding,old,"R2.GKR-master",&error);
    r1::require(valid,"R2 GKR master bridge: "+error);
    finishOpening(t,old);r1::verifyMappedInput(s.input,p.lookup.input,t,mappedClaims(circuit,claims));return claims;
}
} // namespace zkgpt::r2
