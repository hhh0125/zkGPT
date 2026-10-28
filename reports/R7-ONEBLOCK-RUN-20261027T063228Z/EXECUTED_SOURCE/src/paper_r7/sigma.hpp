#pragma once
#include "crypto.hpp"
#include <array>
namespace zkgpt::r7::crypto {
struct Relations{std::string role;std::vector<Handle>handles;std::vector<std::vector<Fr>>linear;std::vector<std::array<std::size_t,3>>products;};
// Every relation, full handle and mask is fixed before rho; B fixed before c.
void sigma(IO&,const Relations&,const std::vector<Hidden>*,Random*);
struct SumcheckInput{PC pc;std::size_t n=0,degree=0,target=0;std::string role;};
enum class TerminalBackendKind { CSIPAResearchV1, HyraxA3Stage2V1, MasterBridgeA3V1 };
struct SumcheckRelation{std::size_t n=0,degree=0;std::string role;};
struct SumcheckWitness{std::vector<std::vector<Fr>>factors;Fr claim=0;};
class TerminalOpeningBackend {
 public:virtual ~TerminalOpeningBackend()=default;
 virtual TerminalBackendKind kind()const=0;
 virtual void authenticate(IO&,const Hidden&initial,const std::vector<Hidden>&factors,const std::vector<Fr>&point,Random*)=0;
};
// Shared arithmetic reduction; concrete verifier selects backend before interaction.
void committedSumcheckRelation(IO&,const SumcheckRelation&,const SumcheckWitness*,TerminalOpeningBackend&,Random*,const std::string&attack="");
using SparsePublicKernel=std::map<std::size_t,Fr>;
// Degree2 specialization for a public sparse factor, preserving hidden rounds.
// P's first materialization is HALF of the original source domain.
std::pair<Hidden,std::vector<Fr>> committedLinearSumcheck(IO&,std::size_t,const Hidden&,const std::function<Fr(std::size_t)>&,const SparsePublicKernel&,Random*,const std::string&role);
// Complete reduction + independent endpoint/product batch + same-handle openings.
void committedSumcheck(IO&,const SumcheckInput&,const PCSecret*,Random*,const std::string&attack="");
void openingExample(IO&,std::size_t n,const PCSecret*,Random*,bool wrong_evaluation=false,std::size_t queries=1);
}
