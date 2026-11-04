#pragma once
#include "oneblock_sumcheck.hpp"
#include "hyrax_a3.hpp"
namespace zkgpt::r7::oneblock {
inline constexpr const char* vectorSuite="R7-ONEBLOCK-HIDING-VECTOR-PC-BALANCED-ROWMAJOR-v3";
inline constexpr const char* vectorLayout="hyrax-balanced-row-major-low-column-bits-v2";
// Ordinary vectors only. Trusted W keeps its independently approved PCSpec.
struct VectorShape {std::size_t rows,columns;};
VectorShape vectorShape(std::size_t padded);
struct VectorPC {std::string id;std::size_t actual=0,padded=0,rows=0,columns=0;std::vector<G1>points;r1::Layout layout=static_cast<r1::Layout>(255);};
struct VectorSecret {VectorPC pc;std::shared_ptr<std::vector<Fr>>values;std::vector<Fr>blinds;};
Bytes encodeVector(const VectorPC&);VectorPC decodeVector(const Bytes&);
VectorSecret commitVector(std::string,std::size_t,std::size_t,Value,Random&);
VectorSecret commitVectorOwned(std::string,std::size_t,std::size_t,std::shared_ptr<std::vector<Fr>>,Random&);
void openVector(IO&,const VectorPC&,const Query&,const VectorSecret*,const Hidden*,Random*,const std::string&);
VectorOracle exchangeVector(IO&,std::string,std::size_t,std::size_t,Value,Random*,std::function<void(const VectorPC&)> on_bound={});
}
