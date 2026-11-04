#pragma once
#include "unified_input.hpp"
#include "hyrax_a3.hpp"
#include <ostream>
namespace zkgpt::r7::oneblock::single {
inline constexpr const char*suite="R7-SINGLE-HYRAX-HIDDEN-ROWMAJOR-v1";
inline constexpr const char*layout="global-row-major-low-column-bits-v1";
inline constexpr const char*modelScope="PROVER_SELECTED_WEIGHTS_BOUND_IN_C_U-v1";
inline constexpr std::size_t maxDomain=std::size_t(1)<<32;
inline constexpr std::size_t maxRows=std::size_t(1)<<20;
struct Segment {std::size_t offset=0,actual=0,padded=0;};
struct Plan {
 std::string source_identity;
 std::size_t domain=0,rows=0,columns=0,logical_values=0,column_limit=0;
 // Segment0 is verifier-owned public X. Remaining segments are storage only;
 // none has a commitment, basis, blind, identity or independent opening.
 std::vector<Segment>segments;
};
Plan makePlan(const unified::Packing&,const std::vector<std::size_t>&actual,
 const std::string&source_identity,std::size_t column_limit=hyrax_a3::maxDot);
unsigned bits(std::size_t);
Bytes descriptor(const Plan&);
struct Public {Plan plan;std::vector<G1>rows;};
struct Secret {Public pc;std::vector<Fr>blinds;std::vector<Value>sources;};
// Callbacks borrow immutable sealed source storage, and may be read in parallel.
// Only one columns-sized scalar buffer per worker is required. No GPU residency.
Secret commit(const Plan&,std::vector<Value>,Random&,unsigned workers=1);
Bytes encode(const Public&);Public decode(const Bytes&,const Plan&expected);
std::string identity(const Public&);
void writeLayout(std::ostream&,const Public&);
// One header and bounded row chunks belong to ONE commitment identity.
Public exchange(IO&,const Plan&,const Secret*onlyP);
struct FixedMetrics {std::uint64_t rows=0,response_bytes=0;};
// Generalized masked Sigma enforces public X and every gap/tail. Full private
// rows are skipped; no full-row knowledge registration is introduced.
FixedMetrics constrainFixed(IO&,const Public&,const std::vector<Fr>&public_x,
 const Secret*onlyP,Random*onlyP_rng);
Fr evaluate(const Secret&,const std::vector<Fr>&);
void open(IO&,const Public&,const Query&,const Secret*onlyP_secret,const Hidden*onlyP_evaluation,
 Random*onlyP_rng,const std::string&role);
Hidden evaluateAndOpen(IO&,const Public&,const std::vector<Fr>&,
 const Secret*onlyP,Random*onlyP_rng,const std::string&role);
}
