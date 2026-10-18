#include "zk_committed_linear_opening.hpp"
#include <cybozu/sha2.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>

namespace zkproto::committed {
namespace {
void need(bool ok,const char* message) { if(!ok)throw std::invalid_argument(message); }
void dimension(std::size_t n) { need(n==2||n==4,"committed prototype length must be 2 or 4"); }
void pointValid(const G1& p) { need(p.isValid()&&p.isValidOrder(),"invalid committed G1 subgroup element"); }
void generatorValid(const G1& g) { pointValid(g);need(!g.isZero(),"identity committed generator"); }
bool collision(const G1& a,const G1& b) { return a==b||a==-b; }
struct Encoder {
    Bytes bytes;
    void raw(const void* data,std::size_t n) {
        need(n<=4096-bytes.size(),"committed encoding exceeds bound");
        auto b=static_cast<const std::uint8_t*>(data);bytes.insert(bytes.end(),b,b+n);
    }
    void u32(std::uint32_t x) { for(unsigned j=0;j<4;j++){std::uint8_t b=static_cast<std::uint8_t>(x>>(8*j));raw(&b,1);} }
    void text(const std::string& s) { need(s.size()<=128,"committed domain/context exceeds bound");u32(static_cast<std::uint32_t>(s.size()));raw(s.data(),s.size()); }
    template<class T> void element(const T& x) {
        need(x.isValid(),"invalid committed scalar/point");std::array<std::uint8_t,32> b{};
        need(x.serialize(b.data(),b.size())==32,"unsupported committed encoding");raw(b.data(),b.size());
    }
    void point(const G1& p) { pointValid(p);element(p); }
};
struct Decoder {
    const Bytes& bytes;std::size_t cursor=0;
    explicit Decoder(const Bytes& b):bytes(b) { need(b.size()<=4096,"committed encoding exceeds bound"); }
    const std::uint8_t* take(std::size_t n) { need(n<=bytes.size()-cursor,"truncated committed encoding");auto b=bytes.data()+cursor;cursor+=n;return b; }
    std::uint32_t u32() { auto b=take(4);std::uint32_t x=0;for(unsigned j=0;j<4;j++)x|=std::uint32_t(b[j])<<(8*j);return x; }
    std::string text() { auto n=u32();need(n<=128,"committed domain/context exceeds bound");return std::string(reinterpret_cast<const char*>(take(n)),n); }
    void magic(const char* m) { need(std::memcmp(take(4),m,4)==0,"committed magic mismatch"); }
    template<class T> T element() {
        auto b=take(32);T x;need(x.deserialize(b,32)==32&&x.isValid(),"invalid committed element encoding");
        std::array<std::uint8_t,32> canonical{};need(x.serialize(canonical.data(),32)==32&&std::equal(canonical.begin(),canonical.end(),b),"noncanonical committed encoding");return x;
    }
    G1 point() { auto p=element<G1>();pointValid(p);return p; }
    void finish() { need(cursor==bytes.size(),"trailing committed bytes"); }
};
void statementShape(const Statement& s) {
    need(s.version==kVersion,"unsupported committed statement version");
    need(s.vector_domain==kGeneratorDomain,"unsupported vector generator domain");
    need(s.scalar_domain==kScalarDomain,"unsupported scalar generator domain");
    need(s.context.size()<=128,"committed context exceeds bound");dimension(s.coefficients.size());
    pointValid(s.commitment);pointValid(s.evaluation_commitment);
    for(const auto& b:s.coefficients)need(b.isValid(),"invalid committed coefficient");
}
void proofShape(const Proof& p) {
    need(p.version==kVersion,"unsupported committed proof version");need(p.domain==kProofDomain,"unsupported committed transcript domain");
    dimension(p.responses.size());pointValid(p.nonce_commitment);pointValid(p.nonce_evaluation_commitment);
    need(p.blind_response.isValid()&&p.evaluation_blind_response.isValid(),"invalid committed blind response");
    for(const auto& z:p.responses)need(z.isValid(),"invalid committed response");
}
void statementValid(const Parameters& pp,const Statement& s) {
    validateParameters(pp);statementShape(s);need(s.coefficients.size()==pp.vector_parameters.generators.size(),"committed statement dimension mismatch");
}
}
Parameters makeParameters(std::size_t n) {
    Parameters pp;pp.vector_parameters=zkproto::makeParameters(n);
    mcl::bn::hashAndMapToG1(pp.value_generator,pp.scalar_domain+"/u");
    mcl::bn::hashAndMapToG1(pp.hiding_generator,pp.scalar_domain+"/h-y");
    generatorValid(pp.value_generator);generatorValid(pp.hiding_generator);
    need(!collision(pp.value_generator,pp.hiding_generator),"scalar generator collision");
    auto bases=pp.vector_parameters.generators;bases.push_back(pp.vector_parameters.hiding_generator);
    for(const auto& g:bases)need(!collision(g,pp.value_generator)&&!collision(g,pp.hiding_generator),"cross-domain generator collision");
    return pp;
}
void validateParameters(const Parameters& pp) {
    need(pp.scalar_domain==kScalarDomain,"unsupported scalar parameter domain");
    zkproto::validateParameters(pp.vector_parameters);
    auto expected=makeParameters(pp.vector_parameters.generators.size());
    need(pp.value_generator==expected.value_generator&&pp.hiding_generator==expected.hiding_generator,"scalar parameters differ from verifier-derived bases");
}
G1 scalarCommit(const Parameters& pp,const Fr& value,const Fr& blind) {
    validateParameters(pp);need(value.isValid()&&blind.isValid(),"invalid scalar commitment opening");
    return pp.value_generator*value+pp.hiding_generator*blind;
}
ScalarCommitment randomScalarCommit(const Parameters& pp,const Fr& value) {
    validateParameters(pp);need(value.isValid(),"invalid hidden scalar");
    ScalarCommitment D;D.blind=sampleUniformScalar();D.point=scalarCommit(pp,value,D.blind);return D;
}
Bytes encodeStatement(const Statement& s) {
    statementShape(s);Encoder e;e.raw("ZKCS",4);e.u32(s.version);e.u32(static_cast<std::uint32_t>(s.coefficients.size()));
    e.text(s.vector_domain);e.text(s.scalar_domain);e.text(s.context);e.point(s.commitment);e.point(s.evaluation_commitment);
    for(const auto& b:s.coefficients)e.element(b);
    return e.bytes;
}
Statement decodeStatement(const Bytes& b) {
    Decoder d(b);d.magic("ZKCS");Statement s;s.version=d.u32();need(s.version==kVersion,"unsupported committed statement version");
    auto n=d.u32();dimension(n);s.vector_domain=d.text();need(s.vector_domain==kGeneratorDomain,"unsupported vector generator domain");
    s.scalar_domain=d.text();need(s.scalar_domain==kScalarDomain,"unsupported scalar generator domain");s.context=d.text();
    s.commitment=d.point();s.evaluation_commitment=d.point();s.coefficients.resize(n,Fr(0));for(auto& x:s.coefficients)x=d.element<Fr>();
    d.finish();statementShape(s);return s;
}
Bytes encodeProof(const Proof& p) {
    proofShape(p);Encoder e;e.raw("ZKCP",4);e.u32(p.version);e.u32(static_cast<std::uint32_t>(p.responses.size()));e.text(p.domain);
    e.point(p.nonce_commitment);e.point(p.nonce_evaluation_commitment);for(const auto& z:p.responses)e.element(z);
    e.element(p.blind_response);e.element(p.evaluation_blind_response);return e.bytes;
}
Proof decodeProof(const Bytes& b) {
    Decoder d(b);d.magic("ZKCP");Proof p;p.version=d.u32();need(p.version==kVersion,"unsupported committed proof version");auto n=d.u32();dimension(n);
    p.domain=d.text();need(p.domain==kProofDomain,"unsupported committed transcript domain");p.nonce_commitment=d.point();p.nonce_evaluation_commitment=d.point();
    p.responses.resize(n,Fr(0));for(auto& z:p.responses)z=d.element<Fr>();p.blind_response=d.element<Fr>();p.evaluation_blind_response=d.element<Fr>();
    d.finish();proofShape(p);return p;
}
Fr challenge(const Parameters& pp,const Statement& s,const Proof& p) {
    statementValid(pp,s);proofShape(p);need(p.responses.size()==s.coefficients.size(),"committed response dimension mismatch");
    Encoder t;t.text("zkGPT.zkproto.committed-linear-map.MCL-BN254.fs.sha256.v1");
    auto x=encodeStatement(s);t.u32(static_cast<std::uint32_t>(x.size()));t.raw(x.data(),x.size());
    t.u32(static_cast<std::uint32_t>(pp.vector_parameters.generators.size()));
    for(const auto& g:pp.vector_parameters.generators)t.point(g);
    t.point(pp.vector_parameters.hiding_generator);t.point(pp.value_generator);t.point(pp.hiding_generator);
    t.u32(p.version);t.text(p.domain);t.point(p.nonce_commitment);t.point(p.nonce_evaluation_commitment);
    // Reuse Task10 canonical rejection; never reduce/mask a hash to Fr.
    // Responses are checked for shape, not absorbed before the challenge.
    for(std::uint32_t counter=0;counter<4096;counter++) {
        auto trial=t;trial.u32(counter);cybozu::Sha256 sha;sha.update(trial.bytes.data(),trial.bytes.size());
        std::array<std::uint8_t,32> h{};sha.digest(h.data(),h.size(),nullptr,0);Fr c=0;
        if(detail::scalarCandidate(h,c)&&!c.isZero())return c;
    }
    throw std::runtime_error("committed challenge rejection sampling exhausted");
}
Proof prove(const Parameters& pp,const Statement& s,const SecretWitness& witness,const Fr& eta) {
    statementValid(pp,s);need(witness.values.size()==s.coefficients.size(),"committed witness dimension mismatch");
    need(commit(pp.vector_parameters,witness.values,witness.blind)==s.commitment,"committed prover C opening mismatch");
    need(scalarCommit(pp,innerProduct(s.coefficients,witness.values),eta)==s.evaluation_commitment,"committed prover D opening mismatch");
    auto d=std::vector<Fr>(witness.values.size(),Fr(0));for(auto& mask:d)mask=sampleUniformScalar();
    Fr a=sampleUniformScalar(),e=sampleUniformScalar();Proof p;
    p.nonce_commitment=commit(pp.vector_parameters,d,a);
    p.nonce_evaluation_commitment=scalarCommit(pp,innerProduct(s.coefficients,d),e);
    p.responses.assign(d.size(),Fr(0));auto c=challenge(pp,s,p);
    for(std::size_t i=0;i<d.size();i++)p.responses[i]=d[i]+c*witness.values[i];
    p.blind_response=a+c*witness.blind;p.evaluation_blind_response=e+c*eta;return p;
}
bool verify(const Parameters& pp,const Statement& s,const Proof& p,std::string* error) {
    try {
        auto c=challenge(pp,s,p);
        need(commit(pp.vector_parameters,p.responses,p.blind_response)==p.nonce_commitment+s.commitment*c,"committed C equation failed");
        need(scalarCommit(pp,innerProduct(s.coefficients,p.responses),p.evaluation_blind_response)==p.nonce_evaluation_commitment+s.evaluation_commitment*c,"committed D equation failed");
        if(error)error->clear();
        return true;
    }catch(const std::exception& e){if(error)*error=e.what();return false;}
}
}
