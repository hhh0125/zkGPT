#include "zk_linear_opening.hpp"
#include <stdexcept>
namespace zkproto {
[[noreturn]] void missing(){throw std::logic_error("prototype feature not implemented");}
Parameters makeParameters(std::size_t){missing();}
void validateParameters(const Parameters&){missing();}
Fr innerProduct(const std::vector<Fr>&,const std::vector<Fr>&){missing();}
G1 commit(const Parameters&,const std::vector<Fr>&,const Fr&){missing();}
RandomizedCommitment randomCommit(const Parameters&,const std::vector<Fr>&){missing();}
Fr openingChallenge(const Parameters&,const Statement&,const LinearOpeningProof&){missing();}
LinearOpeningProof proveLinearOpening(const Parameters&,const Statement&,const SecretWitness&){missing();}
bool verifyLinearOpening(const Parameters&,const Statement&,const LinearOpeningProof&,std::string*){missing();}
std::vector<std::uint8_t> encodeStatement(const Statement&){missing();}
Statement decodeStatement(const std::vector<std::uint8_t>&){missing();}
std::vector<std::uint8_t> encodeProof(const LinearOpeningProof&){missing();}
LinearOpeningProof decodeProof(const std::vector<std::uint8_t>&){missing();}
}
