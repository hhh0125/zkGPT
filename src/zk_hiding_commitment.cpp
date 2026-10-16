#include "zk_hiding_commitment.hpp"
#include <stdexcept>
#include <algorithm>
#include <cerrno>
#include <sys/random.h>

namespace zkproto {
namespace {
void checkField() {
    if (Fr::getModulo() !=
        "16798108731015832284940804142231733909759579603404752749028378864165570215949" ||
        mcl::bn::Fp::getModulo() !=
        "16798108731015832284940804142231733909889187121439069848933715426072753864723" ||
        Fr::getByteSize()!=32 || G1::getSerializedByteSize()!=32)
        throw std::invalid_argument("prototype requires MCL BN254 default fields");
}
void generatorValid(const G1& g) {
    if(g.isZero() || !g.isValid() || !g.isValidOrder())
        throw std::invalid_argument("invalid prototype generator");
}
}
Parameters makeParameters(std::size_t n) {
    checkField();
    if(n!=2 && n!=4)throw std::invalid_argument("prototype length must be 2 or 4");
    Parameters pp;pp.generators.resize(n);
    for(std::size_t i=0;i<n;i++) {
        mcl::bn::hashAndMapToG1(pp.generators[i],
                              pp.domain+"/g/"+std::to_string(i));
        generatorValid(pp.generators[i]);
    }
    mcl::bn::hashAndMapToG1(pp.hiding_generator,pp.domain+"/h");
    generatorValid(pp.hiding_generator);
    for(std::size_t i=0;i<n;i++) {
        if(pp.generators[i]==pp.hiding_generator ||
           pp.generators[i]==-pp.hiding_generator)
            throw std::runtime_error("prototype generator collision");
        for(std::size_t j=0;j<i;j++)
            if(pp.generators[i]==pp.generators[j] ||
               pp.generators[i]==-pp.generators[j])
                throw std::runtime_error("prototype generator collision");
    }
    return pp;
}
void validateParameters(const Parameters& pp) {
    if(pp.domain!=kGeneratorDomain)
        throw std::invalid_argument("unsupported generator domain");
    auto expected=makeParameters(pp.generators.size());
    if(pp.generators!=expected.generators ||
       pp.hiding_generator!=expected.hiding_generator)
        throw std::invalid_argument("parameters differ from verifier-derived generators");
}
Fr innerProduct(const std::vector<Fr>& a,const std::vector<Fr>& b) {
    if(a.size()!=b.size() || (a.size()!=2 && a.size()!=4))
        throw std::invalid_argument("prototype inner-product dimensions");
    Fr y=0;
    for(std::size_t i=0;i<a.size();i++) {
        if(!a[i].isValid() || !b[i].isValid())
            throw std::invalid_argument("invalid scalar");
        y+=a[i]*b[i];
    }
    return y;
}
G1 commit(const Parameters& pp,const std::vector<Fr>& w,const Fr& rho) {
    validateParameters(pp);
    if(w.size()!=pp.generators.size())
        throw std::invalid_argument("commitment witness length");
    if(!rho.isValid())throw std::invalid_argument("invalid blinding scalar");
    for(const auto& a:w)if(!a.isValid())throw std::invalid_argument("invalid witness scalar");
    G1 c;
    G1::mulVec(c,const_cast<G1*>(pp.generators.data()),
               const_cast<Fr*>(w.data()),w.size());
    c+=pp.hiding_generator*rho;
    return c;
}
RandomizedCommitment randomCommit(const Parameters& pp,const std::vector<Fr>& w) {
    RandomizedCommitment out;out.witness.values=w;
    out.witness.blind=sampleUniformScalar();
    out.point=commit(pp,w,out.witness.blind);
    return out;
}
bool detail::scalarCandidate(const std::array<std::uint8_t,32>& raw,Fr& out) {
    out=0;Fr candidate=0;
    if(candidate.deserialize(raw.data(),raw.size())!=raw.size() ||
       !candidate.isValid())return false;
    std::array<std::uint8_t,32> canonical{};
    if(candidate.serialize(canonical.data(),canonical.size())!=canonical.size() ||
       canonical!=raw)return false;
    out=candidate;return true;
}
Fr sampleUniformScalar() {
    checkField();
    for(std::size_t attempt=0;attempt<4096;attempt++) {
        std::array<std::uint8_t,32> raw{};
        std::size_t filled=0;
        while(filled<raw.size()) {
            auto n=::getrandom(raw.data()+filled,raw.size()-filled,0);
            if(n<0 && errno==EINTR)continue;
            if(n<=0)throw std::runtime_error("system entropy read failed");
            filled+=static_cast<std::size_t>(n);
        }
        Fr scalar=0;
        if(detail::scalarCandidate(raw,scalar))return scalar;
    }
    throw std::runtime_error("uniform scalar rejection sampling exhausted");
}
}
