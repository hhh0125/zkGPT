#pragma once
#include "oneblock_sumcheck.hpp"
#include <array>
namespace zkgpt::r7::oneblock::unified {
class PublicK;
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
// Kernel oracle is already low-bit folded; sealed input oracle is unchanged.
Round quadraticRoundFolded(std::size_t,const std::vector<Segment>&,const std::vector<Fr>&prefix,const Factors&);
// Borrowed sealed RowMajor storage; implicit padded suffix is zero. Never owned.
struct InputSpan {const Fr*data=nullptr;std::size_t stored=0,padded=0;};
using Inputs=std::function<InputSpan(std::size_t)>;
struct UOptions {unsigned threads=1;std::size_t tile_pairs=1024;bool factored=true;
 // Test-only immediate observer. Production leaves empty; never serialized.
 std::function<void(std::size_t,std::size_t,const Fr*,std::size_t)>observe;
 static UOptions environment();};
struct UMetrics {std::uint64_t coordinates=0,bytes=0,mapping_calls=0,multiplications=0,additions=0,tile_bytes=0,thread_bytes=0,extra_bytes=0,allocations=0,allocated_bytes=0,fused_load_bytes=0,product_multiplications=0,product_additions=0;double scan_cpu=0,scan_wall=0,product_wall=0;};
Round quadraticRoundStreaming(std::size_t,const std::vector<Segment>&,const std::vector<Fr>&,const Factors&,const Inputs&,UOptions,UMetrics* =nullptr);
// One hidden degree-two Sumcheck, with a verifier-computed public K endpoint.
Reduction reduce(IO&,std::size_t,const std::vector<Segment>&,const Hidden&,
 const Factors&onlyP,const std::function<Fr(const std::vector<Fr>&)>&kernel_at,
 Random*,const std::string&role,PublicK*onlyP_state=nullptr,const Inputs&onlyP_inputs={});
}
