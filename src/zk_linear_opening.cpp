#include "zk_linear_opening.hpp"
#include <cybozu/sha2.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>

namespace zkproto {
namespace {
using Bytes=std::vector<std::uint8_t>;
void need(bool ok,const char* error) {
    if(!ok)throw std::invalid_argument(error);
}
void smallLength(std::size_t n) {
    need(n==2 || n==4,"prototype length must be 2 or 4");
}
struct Encoder {
    Bytes bytes;
    void raw(const void* p,std::size_t n) {
        auto* b=static_cast<const std::uint8_t*>(p);
        bytes.insert(bytes.end(),b,b+n);
    }
    void u32(std::uint32_t x) {
        for(int j=0;j<4;j++)bytes.push_back(static_cast<std::uint8_t>(x>>(8*j)));
    }
    void text(const std::string& s) {
        need(s.size()<=128,"domain length exceeds bound");
        u32(s.size());raw(s.data(),s.size());
    }
    template<class T> void scalarOrPoint(const T& x) {
        need(x.isValid(),"invalid element");
        std::array<std::uint8_t,32> b{};
        need(x.serialize(b.data(),b.size())==b.size(),"unsupported element encoding");
        raw(b.data(),b.size());
    }
    void point(const G1& p) {
        need(p.isValid() && p.isValidOrder(),"invalid G1 subgroup element");
        scalarOrPoint(p);
    }
};
struct Decoder {
    const Bytes& bytes;std::size_t cursor=0;
    explicit Decoder(const Bytes& b):bytes(b) {
        need(b.size()<=4096,"prototype encoding exceeds bound");
    }
    const std::uint8_t* take(std::size_t n) {
        need(n<=bytes.size()-cursor,"truncated prototype encoding");
        auto* p=bytes.data()+cursor;cursor+=n;return p;
    }
    std::uint32_t u32() {
        auto* b=take(4);std::uint32_t x=0;
        for(int j=0;j<4;j++)x|=std::uint32_t(b[j])<<(8*j);
        return x;
    }
    std::string text() {
        auto n=u32();need(n<=128,"domain length exceeds bound");
        return std::string(reinterpret_cast<const char*>(take(n)),n);
    }
    void magic(const char* tag) {need(std::memcmp(take(4),tag,4)==0,"prototype magic mismatch");}
    template<class T> T scalarOrPoint() {
        auto* b=take(32);T x;
        need(x.deserialize(b,32)==32 && x.isValid(),"invalid element encoding");
        std::array<std::uint8_t,32> canonical{};
        need(x.serialize(canonical.data(),32)==32 &&
             std::equal(canonical.begin(),canonical.end(),b),
             "noncanonical element encoding");
        return x;
    }
    G1 point() {
        auto p=scalarOrPoint<G1>();
        need(p.isValidOrder(),"G1 outside prime-order subgroup");return p;
    }
    void finish() {need(cursor==bytes.size(),"illegal trailing bytes");}
};
void validateStatement(const Parameters& pp,const Statement& s) {
    validateParameters(pp);
    need(s.version==kPrototypeVersion,"unsupported statement version");
    need(s.generator_domain==kGeneratorDomain,"unsupported generator domain");
    need(s.coefficients.size()==pp.generators.size(),"statement dimension mismatch");
    need(s.commitment.isValid() && s.commitment.isValidOrder(),"invalid statement commitment");
    need(s.value.isValid(),"invalid statement scalar");
    for(const auto& b:s.coefficients)need(b.isValid(),"invalid public coefficient");
}
void validateProofShape(const Parameters& pp,const LinearOpeningProof& p) {
    need(p.version==kPrototypeVersion,"unsupported proof version");
    need(p.domain==kOpeningDomain,"unsupported transcript domain");
    need(p.responses.size()==pp.generators.size(),"proof response dimension");
    need(p.nonce_commitment.isValid() && p.nonce_commitment.isValidOrder(),"invalid nonce commitment");
    need(p.nonce_inner_product.isValid() && p.blind_response.isValid(),"invalid proof scalar");
    for(const auto& z:p.responses)need(z.isValid(),"invalid response scalar");
}
}
Bytes encodeStatement(const Statement& s) {
    smallLength(s.coefficients.size());Encoder e;
    e.raw("ZKLS",4);e.u32(s.version);e.u32(s.coefficients.size());
    e.text(s.generator_domain);e.point(s.commitment);
    for(const auto& b:s.coefficients)e.scalarOrPoint(b);
    e.scalarOrPoint(s.value);return e.bytes;
}
Statement decodeStatement(const Bytes& bytes) {
    Decoder d(bytes);d.magic("ZKLS");Statement s;s.version=d.u32();
    need(s.version==kPrototypeVersion,"unsupported statement version");
    auto n=d.u32();smallLength(n);s.generator_domain=d.text();
    need(s.generator_domain==kGeneratorDomain,"unsupported generator domain");
    s.commitment=d.point();s.coefficients.resize(n);
    for(auto& b:s.coefficients)b=d.scalarOrPoint<Fr>();
    s.value=d.scalarOrPoint<Fr>();d.finish();return s;
}
Bytes encodeProof(const LinearOpeningProof& p) {
    smallLength(p.responses.size());Encoder e;
    e.raw("ZKLP",4);e.u32(p.version);e.u32(p.responses.size());e.text(p.domain);
    e.point(p.nonce_commitment);e.scalarOrPoint(p.nonce_inner_product);
    for(const auto& z:p.responses)e.scalarOrPoint(z);
    e.scalarOrPoint(p.blind_response);return e.bytes;
}
LinearOpeningProof decodeProof(const Bytes& bytes) {
    Decoder d(bytes);d.magic("ZKLP");LinearOpeningProof p;p.version=d.u32();
    need(p.version==kPrototypeVersion,"unsupported proof version");
    auto n=d.u32();smallLength(n);p.domain=d.text();
    need(p.domain==kOpeningDomain,"unsupported transcript domain");
    p.nonce_commitment=d.point();p.nonce_inner_product=d.scalarOrPoint<Fr>();
    p.responses.resize(n);for(auto& z:p.responses)z=d.scalarOrPoint<Fr>();
    p.blind_response=d.scalarOrPoint<Fr>();d.finish();return p;
}
Fr openingChallenge(const Parameters& pp,const Statement& s,
                    const LinearOpeningProof& p) {
    validateStatement(pp,s);validateProofShape(pp,p);
    Encoder t;t.text("zkGPT.zkproto.linear-map.fs.sha256.v1");
    auto statement=encodeStatement(s);t.u32(statement.size());
    t.raw(statement.data(),statement.size());t.u32(pp.generators.size());
    for(const auto& g:pp.generators)t.point(g);
    t.point(pp.hiding_generator);t.u32(p.version);t.text(p.domain);
    t.point(p.nonce_commitment);t.scalarOrPoint(p.nonce_inner_product);
    // Uniform nonzero Fr in the ideal-SHA256 model, not biased reduction.
    for(std::uint32_t counter=0;counter<4096;counter++) {
        auto trial=t;trial.u32(counter);
        cybozu::Sha256 sha;sha.update(trial.bytes.data(),trial.bytes.size());
        std::array<std::uint8_t,32> h{};
        sha.digest(h.data(),h.size(),nullptr,0);
        Fr c=0;
        if(c.deserialize(h.data(),h.size())!=32 || !c.isValid() || c.isZero())continue;
        std::array<std::uint8_t,32> canonical{};
        if(c.serialize(canonical.data(),canonical.size())==32 && canonical==h)return c;
    }
    throw std::runtime_error("challenge rejection sampling exhausted");
}
LinearOpeningProof proveLinearOpening(const Parameters& pp,const Statement& s,
                                     const SecretWitness& witness) {
    validateStatement(pp,s);
    need(witness.values.size()==pp.generators.size(),"prover witness dimension");
    need(commit(pp,witness.values,witness.blind)==s.commitment,"prover commitment mismatch");
    need(innerProduct(witness.values,s.coefficients)==s.value,"prover public value mismatch");
    std::vector<Fr> d(witness.values.size(),Fr(0));
    for(auto& mask:d)mask=sampleUniformScalar();
    Fr r=sampleUniformScalar();
    LinearOpeningProof proof;
    proof.nonce_commitment=commit(pp,d,r);
    proof.nonce_inner_product=innerProduct(d,s.coefficients);
    proof.responses.assign(d.size(),Fr(0));
    // Response fields are validated for shape only; not absorbed before c.
    auto c=openingChallenge(pp,s,proof);
    for(std::size_t i=0;i<d.size();i++)proof.responses[i]=d[i]+c*witness.values[i];
    proof.blind_response=r+c*witness.blind;
    return proof;
}
bool verifyLinearOpening(const Parameters& pp,const Statement& s,
                         const LinearOpeningProof& proof,std::string* error) {
    try {
        auto c=openingChallenge(pp,s,proof);
        need(commit(pp,proof.responses,proof.blind_response)==
             proof.nonce_commitment+s.commitment*c,"opening commitment equation failed");
        need(innerProduct(proof.responses,s.coefficients)==
             proof.nonce_inner_product+c*s.value,"opening linear equation failed");
        if(error)error->clear();
        return true;
    }catch(const std::exception& e) {if(error)*error=e.what();return false;}
}
}
