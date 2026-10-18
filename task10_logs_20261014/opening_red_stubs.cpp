#include "zk_linear_opening.hpp"
#include <stdexcept>
namespace zkproto {
[[noreturn]] void noOpening(){throw std::logic_error("linear opening not implemented");}
Fr openingChallenge(const Parameters&,const Statement&,const LinearOpeningProof&){noOpening();}
LinearOpeningProof proveLinearOpening(const Parameters&,const Statement&,const SecretWitness&){noOpening();}
bool verifyLinearOpening(const Parameters&,const Statement&,const LinearOpeningProof&,std::string*){noOpening();}
std::vector<std::uint8_t> encodeStatement(const Statement&){noOpening();}
Statement decodeStatement(const std::vector<std::uint8_t>&){noOpening();}
std::vector<std::uint8_t> encodeProof(const LinearOpeningProof&){noOpening();}
LinearOpeningProof decodeProof(const std::vector<std::uint8_t>&){noOpening();}
}
