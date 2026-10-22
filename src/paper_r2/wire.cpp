#include "wire.hpp"
#include "gkr_serialization.hpp"
namespace zkgpt::r2 {
namespace {
using r1::Bytes;using r1::require;
struct Writer {
    Bytes b;
    void u(std::uint64_t x){for(unsigned i=0;i<8;++i)b.push_back(std::uint8_t(x>>(8*i)));}
    void raw(const Bytes &x){b.insert(b.end(),x.begin(),x.end());require(b.size()<=r1::kMaxWireBytes,"R2 wire budget");}
    void blob(const Bytes &x){u(x.size());raw(x);}
    void text(const std::string &s){blob(Bytes(s.begin(),s.end()));}
    void f(const Fr &x){raw(r1::encode(x));}void g(const G1 &x){raw(r1::encode(x));}
    void opening(const r1::OpenProof &p){f(p.value);u(p.rounds.size());for(const auto &q:p.rounds){g(q.left);g(q.right);}f(p.final);}
};
struct Reader {
    const Bytes &b;std::size_t at=0;
    explicit Reader(const Bytes &bytes):b(bytes){require(b.size()<=r1::kMaxWireBytes,"R2 wire budget");}
    void room(std::size_t n){require(n<=b.size()-at,"R2 truncated artifact");}
    std::uint64_t u(){room(8);std::uint64_t v=0;for(unsigned i=0;i<8;++i)v|=std::uint64_t(b[at++])<<(8*i);return v;}
    void count(std::size_t n){require(u()==n,"R2 exact count");}
    Bytes raw(std::size_t n){room(n);Bytes v(b.begin()+at,b.begin()+at+n);at+=n;return v;}
    Bytes blob(){auto n=u();require(n<=r1::kMaxWireBytes,"R2 blob budget");return raw(n);}
    std::string text(){auto v=blob();require(v.size()<=128,"R2 text size");return {v.begin(),v.end()};}
    Fr f(){return r1::decodeFr(raw(32));}G1 g(){return r1::decodeG1(raw(G1::getSerializedByteSize()));}
    r1::OpenProof opening(const r1::PCSpec &s){r1::OpenProof p;p.value=f();auto n=r1::log2exact(s.columns);count(n);room(n*2*G1::getSerializedByteSize()+32);for(unsigned i=0;i<n;++i)p.rounds.push_back({g(),g()});p.final=f();return p;}
    void done(){require(at==b.size(),"R2 trailing bytes");}
};
void header(Writer &w,const std::string &kind){w.text("ZKGPTR2");w.u(3);w.text(kind);}
void header(Reader &r,const std::string &kind){require(r.text()=="ZKGPTR2" && r.u()==3 && r.text()==kind,"R2 magic/version/kind");}
}
r1::Bytes serializeStatement(const GKRPublicStatement &g,const r1::LookupBatchSpec &s){
    Writer w;header(w,"statement");w.u(g.protocol_version);
    const auto &m=g.model_shape;
    for(auto x:{m.sequence_length,m.layer_count,m.head_count,m.head_dimension,m.hidden_dimension,m.mlp_dimension})w.u(x);
    require(g.val0_log_size>=0 && g.val0_log_size<=28,"R2 master dimension");w.u(g.val0_log_size);
    w.text(g.val0_generator_domain.domain);w.u(g.val0_generator_domain.count);
    w.raw(Bytes(g.circuit_fingerprint.begin(),g.circuit_fingerprint.end()));w.f(g.output_evaluation);
    w.raw(Bytes(g.integration_context.begin(),g.integration_context.end()));
    w.u(g.val0_commitment.size());for(const auto &x:g.val0_commitment)w.g(x);
    w.blob(r1::serializeLookup(s));return w.b;
}
std::pair<GKRPublicStatement,r1::LookupBatchSpec> parseStatement(const r1::Bytes &b){
    Reader r(b);header(r,"statement");GKRPublicStatement g;auto version=r.u();require(version==kZkGPTProtocolVersion,"R2 GKR version");g.protocol_version=version;
    auto &m=g.model_shape;m.sequence_length=r.u();m.layer_count=r.u();m.head_count=r.u();m.head_dimension=r.u();m.hidden_dimension=r.u();m.mlp_dimension=r.u();
    auto d=r.u();require(d<=28,"R2 dimension budget");g.val0_log_size=d;g.val0_generator_domain.domain=r.text();
    auto count=r.u();require(count==(std::size_t(1)<<(d-d/2)),"R2 generator shape");g.val0_generator_domain.count=count;
    auto fp=r.raw(32);std::copy(fp.begin(),fp.end(),g.circuit_fingerprint.begin());g.output_evaluation=r.f();
    auto context=r.raw(32);std::copy(context.begin(),context.end(),g.integration_context.begin());
    count=std::size_t(1)<<(d/2);r.count(count);r.room(count*G1::getSerializedByteSize());
    for(std::size_t i=0;i<count;++i)g.val0_commitment.push_back(r.g());
    auto s=r1::parseLookupSpec(r.blob());r.done();require(s.input.master.spec.padded==(std::size_t(1)<<d),"R2 master shape");return {std::move(g),std::move(s)};
}
r1::Bytes serializeProof(const GKRPublicStatement &g,const r1::LookupBatchSpec &s,const Proof &p){
    Writer w;header(w,"proof");auto digest=r1::hash(serializeStatement(g,s));w.raw(Bytes(digest.begin(),digest.end()));
    w.blob(serializeGKRProof(p.gkr));w.opening(p.master_binding);w.u(p.gkr_input_binding.ipa.rounds.size());
    for(const auto &q:p.gkr_input_binding.ipa.rounds){w.g(q.left);w.g(q.right);}w.f(p.gkr_input_binding.ipa.final_witness);
    w.blob(r1::serializeLookup(s,p.lookup));return w.b;
}
Proof parseProof(const GKRPublicStatement &g,const r1::LookupBatchSpec &s,const r1::Bytes &b){
    Reader r(b);header(r,"proof");auto digest=r1::hash(serializeStatement(g,s));require(r.raw(32)==Bytes(digest.begin(),digest.end()),"R2 statement binding");
    Proof p;std::string error;const bool valid=deserializeGKRProof(r.blob(),p.gkr,&error);
    require(valid,"R2 GKR decoder: "+error);
    p.master_binding=r.opening(s.input.master.spec);auto rounds=r1::log2exact(g.val0_generator_domain.count);r.count(rounds);
    r.room(rounds*2*G1::getSerializedByteSize()+32);
    for(unsigned i=0;i<rounds;++i) p.gkr_input_binding.ipa.rounds.push_back({r.g(),r.g()});
    p.gkr_input_binding.ipa.final_witness=r.f();
    p.lookup=r1::parseLookupProof(s,r.blob());r.done();return p;
}
} // namespace zkgpt::r2
