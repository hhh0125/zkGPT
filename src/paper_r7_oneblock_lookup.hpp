#pragma once
#include "paper_r7_oneblock_sumcheck.hpp"
#include "paper_r1_exp.hpp"
namespace zkgpt::r7::oneblock {
using LeafAuthenticator=std::function<void(IO&,const Query&,const Hidden*,Random*,std::string)>;
// Root is fixed before this reduction. Intermediate product layers are proved
// recursively and never represented by independent unauthenticated PCs.
// Prover-only optional storage, borrowed without copying. The caller retains
// one shared owner and must keep values unchanged until the call returns.
void hiddenProduct(IO&,std::size_t,const Hidden&,Value,const LeafAuthenticator&,Random*,const std::string&,const std::shared_ptr<std::vector<Fr>>& prepared_leaves={},bool allow_specialized=true);
using BatchedLeafAuthenticator=std::function<void(IO&,const std::vector<Fr>&,const std::array<Hidden,2>&,Random*,std::string)>;
// Prover base is F (without +1); roots are respectively product(F+1), product(F).
// role is the common product prefix; legacy coverage uses role+"/0" and "/1".
void hiddenBatchedProduct(IO&,std::size_t,const std::array<Hidden,2>&,
 const std::shared_ptr<std::vector<Fr>>&,const BatchedLeafAuthenticator&,Random*,const std::string&);
void hiddenMemory(IO&,const VectorOracle&address,const VectorOracle&value,const std::vector<Fr>&table,const VectorFactory&,Random*,const std::string&,bool prepared=false);
// Caller supplies role="r7/" + native RangeSpec/ExpSpec.id so factory IDs
// exactly match the native auxiliary registry. The encoded Range query must
// also be tied to its source by an affine gather or hiddenRangeBinding.
void hiddenRange(IO&,const VectorOracle&,unsigned bits,const VectorFactory&,Random*,const std::string&,bool prepared=false);
void hiddenRangeBinding(IO&,const VectorOracle&source,const VectorOracle&encoded,unsigned bits,bool signed_encoding,Random*,const std::string&);
void hiddenExp(IO&,const VectorOracle&index,const VectorOracle&value,r1::ExpProfile,const VectorFactory&,Random*,const std::string&,bool prepared=false);
}
