// RED-only missing-feature stand-ins. Never linked in final CMake targets.
#include "zk_committed_linear_opening.hpp"
#include <stdexcept>
namespace zkproto::committed {
Parameters makeParameters(std::size_t n) { Parameters p; p.vector_parameters=zkproto::makeParameters(n); return p; }
void validateParameters(const Parameters&) { throw std::runtime_error("not implemented"); }
G1 scalarCommit(const Parameters&,const Fr&,const Fr&) { G1 g;g.clear();return g; }
ScalarCommitment randomScalarCommit(const Parameters&,const Fr&) { return {}; }
Fr challenge(const Parameters&,const Statement&,const Proof&) { return Fr(1); }
Proof prove(const Parameters&,const Statement& s,const SecretWitness&,const Fr&) { Proof p;p.responses.resize(s.coefficients.size(),Fr(0));return p; }
bool verify(const Parameters&,const Statement&,const Proof&,std::string*) { return false; }
Bytes encodeStatement(const Statement&) { return {}; }
Statement decodeStatement(const Bytes&) { throw std::runtime_error("not implemented"); }
Bytes encodeProof(const Proof&) { return {}; }
Proof decodeProof(const Bytes&) { throw std::runtime_error("not implemented"); }
}
