#pragma once
#include "oneblock_sumcheck.hpp"
#include <array>
#include <ostream>
namespace zkgpt::r7::oneblock::unified {
class PublicK;
class FoldedU;
inline constexpr const char*suite="R7-PROTOCOL1-HIDDEN-INDEPENDENT-REGIONS-INPUT-v2";
struct Block {unsigned category;std::size_t padded;};
struct Segment {std::size_t offset,padded;};
struct Region {std::size_t used=0,capacity=0,alignment=1,base=0;};
struct Packing {std::size_t domain=0;std::vector<Segment>segments;std::array<Region,4>regions{};std::vector<unsigned>categories;};
// Legacy pack only: segment 0 is public X. Category callers instead supply
// aligned computational tiles, including the physically committed X.
Packing pack(const std::vector<Block>&,std::size_t public_padded);
std::array<std::size_t,4>independentCapacities(const std::array<std::size_t,4>&);
void validate(const Packing&);
Bytes descriptor(const Packing&);
void writeLayout(std::ostream&,const Packing&);
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
struct InputSpan {const Fr*data=nullptr;std::size_t stored=0,padded=0;Value read={};const void*storage_identity=nullptr;std::size_t storage_offset=0;};
using Inputs=std::function<InputSpan(std::size_t)>;
struct UOptions {unsigned threads=1;std::size_t tile_pairs=1024;bool factored=true;
 // Test-only immediate observer. Production leaves empty; never serialized.
 std::function<void(std::size_t,std::size_t,const Fr*,std::size_t)>observe;
 // Reference execution retains arithmetic on the public implicit zero suffix.
 // Both choices keep every tile, K query, product and endpoint obligation.
 // Experimental: exact arithmetic tests pass, but A/B showed an unstable
 // whole-round regression. Keep production on the reference scan by default.
 bool elide_zero_suffix=false;
 // PERF3 independent dot-accumulator candidate; no zero suffix elision.
 bool striped_dot=false;
 bool profile_reads=false;
 static UOptions environment();};
 struct UMetrics {std::uint64_t coordinates=0,bytes=0,mapping_calls=0,multiplications=0,additions=0,tile_bytes=0,thread_bytes=0,extra_bytes=0,allocations=0,allocated_bytes=0,fused_load_bytes=0,product_multiplications=0,product_additions=0,striped_dot_groups=0;
 std::uint64_t contracted_coordinates=0,implicit_zero_coordinates=0;
 std::uint64_t scan_calls=0,timed_read_elements=0,read_elements=0,profile_refills=0,profile_hits=0,profile_misses=0,original_read_elements=0,retained_read_elements=0,storage_chunks_touched=0,state_hits=0,state_misses=0;
 bool storage_identity_complete=true,read_profile_enabled=false;
 double state_capture_wall=0,state_capture_cpu=0,read_wall=0,read_cpu=0,weights_wall=0,scan_cpu=0,scan_wall=0,product_wall=0;};
std::string scanMetricsJSON(unsigned,const UMetrics&);
Round quadraticRoundStreaming(std::size_t,const std::vector<Segment>&,const std::vector<Fr>&,const Factors&,const Inputs&,UOptions,UMetrics* =nullptr,FoldedU* =nullptr,const std::string&session={});
// One hidden degree-two Sumcheck, with a verifier-computed public K endpoint.
Reduction reduce(IO&,std::size_t,const std::vector<Segment>&,const Hidden&,
 const Factors&onlyP,const std::function<Fr(const std::vector<Fr>&)>&kernel_at,
 Random*,const std::string&role,PublicK*onlyP_state=nullptr,const Inputs&onlyP_inputs={});
}
