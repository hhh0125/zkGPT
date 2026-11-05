#pragma once
#include "unified_input.hpp"
#include "hyrax_a3.hpp"
#include <ostream>
namespace zkgpt::r7::oneblock::single {
inline constexpr const char*suite="R7-SINGLE-HYRAX-COMPACT-INDEPENDENT-REGIONS-HIDDEN-v3";
inline constexpr const char*layout="compact-private-independent-logical-regions-mapped-query-v3";
inline constexpr const char*modelScope="PROVER_SELECTED_WEIGHTS_BOUND_IN_C_U-v1";
inline constexpr std::size_t maxDomain=std::size_t(1)<<32;
inline constexpr std::size_t maxRows=std::size_t(1)<<16;
struct Segment {
 std::size_t offset=0,actual=0,padded=0,physical_offset=0,physical_length=0;
 bool is_public=false;std::string source_id;
};
struct LogicalLayout {std::size_t domain=0,logical_values=0;unified::Packing packing;};
struct PhysicalCommitmentLayout {std::size_t physical_values=0,physical_domain=0,rows=0,columns=0;};
struct PublicCell {std::size_t source=0,index=0,x_index=0;};
struct PhysicalRun {std::size_t source=0,source_offset=0,count=0,physical_offset=0;};
struct Plan:LogicalLayout,PhysicalCommitmentLayout {
 std::string source_identity;
 // Segment0 is verifier-owned public X. Remaining segments are storage only;
 // none has a commitment, basis, blind, identity or independent opening.
 std::vector<Segment>segments;
 std::vector<PublicCell>public_cells;
 std::vector<PhysicalRun>runs;
};
Plan makePlan(const unified::Packing&,const std::vector<std::size_t>&actual,
 const std::string&source_identity,const std::vector<std::string>&source_ids={},const std::vector<PublicCell>&native_public={});
PhysicalCommitmentLayout matrixShape(std::size_t actual);
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
// Exact rank-one decomposition of the logical query after compact embedding.
// At most four terms/run; no coefficient array over the logical domain.
struct QueryTerm {std::size_t first_row=0;std::vector<Fr>left,right;};
using ConsumeTerm=std::function<void(const QueryTerm&)>;
void mappedQueryTerms(const Plan&,const std::vector<Fr>&logical_point,const ConsumeTerm&);
Fr publicContribution(const Plan&,const std::vector<Fr>&logical_point,const std::vector<Fr>&public_x);
Fr evaluate(const Secret&,const std::vector<Fr>&);
void open(IO&,const Public&,const Query&,const std::vector<Fr>&public_x,const Secret*onlyP_secret,const Hidden*onlyP_evaluation,
 Random*onlyP_rng,const std::string&role);
Hidden evaluateAndOpen(IO&,const Public&,const std::vector<Fr>&,const std::vector<Fr>&public_x,
 const Secret*onlyP,Random*onlyP_rng,const std::string&role);
}
