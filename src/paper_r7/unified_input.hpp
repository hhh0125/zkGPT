#pragma once
#include "oneblock_sumcheck.hpp"
#include <array>
namespace zkgpt::r7::oneblock::unified {
inline constexpr const char*suite="R7-PROTOCOL1-HIDDEN-UNIFIED-INPUT-v1";
struct Block {unsigned category;std::size_t padded;};
struct Segment {std::size_t offset,padded;};
struct Packing {std::size_t partition_size=0,domain=0;std::vector<Segment>segments;};
// Segment0 is public X. Later segments preserve original physical root order.
Packing pack(const std::vector<Block>&,std::size_t public_padded);
void validate(std::size_t,const std::vector<Segment>&);
Fr selector(const Segment&,const std::vector<Fr>&global_point);
using Consume=std::function<void(const Value&,const Value&)>;
// A provider invokes consume exactly once with a borrowed sealed input oracle
// and its public kernel. At most one physical-root kernel is live at a time.
using Factors=std::function<void(std::size_t,const Consume&)>;
struct Round {std::array<Fr,3>coefficients{Fr(0),Fr(0),Fr(0)};std::array<Fr,2>input_pair{Fr(0),Fr(0)},kernel_pair{Fr(0),Fr(0)};};
Round quadraticRound(std::size_t,const std::vector<Segment>&,const std::vector<Fr>&prefix,const Factors&);
// One hidden degree-two Sumcheck, with a verifier-computed public K endpoint.
Reduction reduce(IO&,std::size_t,const std::vector<Segment>&,const Hidden&,
 const Factors&onlyP,const std::function<Fr(const std::vector<Fr>&)>&kernel_at,
 Random*,const std::string&role);
}
