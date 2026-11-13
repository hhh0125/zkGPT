#pragma once
#include "oneblock_sumcheck.hpp"
#include "paper_r1/exp.hpp"
namespace zkgpt::r7::oneblock {
using LeafAuthenticator=std::function<void(IO&,const Query&,const Hidden*,Random*,std::string)>;
// Root is fixed before this reduction. Intermediate product layers are proved
// recursively and never represented by independent unauthenticated PCs.
void hiddenProduct(IO&,std::size_t,const Hidden&,Value,const LeafAuthenticator&,Random*,const std::string&);
void hiddenMemory(IO&,const VectorOracle&address,const VectorOracle&value,const std::vector<Fr>&table,const VectorFactory&,Random*,const std::string&,bool prepared=false);
// Caller supplies role="r7/" + native RangeSpec/ExpSpec.id so factory IDs
// exactly match the native auxiliary registry. The encoded Range query must
// also be tied to its source by an affine gather or hiddenRangeBinding.
void hiddenRange(IO&,const VectorOracle&,unsigned bits,const VectorFactory&,Random*,const std::string&,bool prepared=false);
void hiddenRangeBinding(IO&,const VectorOracle&source,const VectorOracle&encoded,unsigned bits,bool signed_encoding,Random*,const std::string&);
void hiddenExp(IO&,const VectorOracle&index,const VectorOracle&value,r1::ExpProfile,const VectorFactory&,Random*,const std::string&,bool prepared=false);
}
