#pragma once
#include "unified_input.hpp"
#include "hyrax_a3.hpp"
#include <ostream>
namespace zkgpt::r7::oneblock::single {
inline constexpr const char*suite="R7-SINGLE-HYRAX-DIRECT-LOGICAL-FIVE-CATEGORIES-HIDDEN-v5";
inline constexpr const char*layout="direct-logical-five-categories-committed-X-v5";
inline constexpr const char*modelScope="PROVER_SELECTED_WEIGHTS_BOUND_IN_C_U-v1";
inline constexpr std::size_t maxDomain=std::size_t(1)<<32;
inline constexpr std::size_t maxRows=std::size_t(1)<<16;
struct Segment {
 std::size_t offset=0,actual=0,padded=0,physical_offset=0,physical_length=0;
 bool is_public=false;std::string source_id;
};
struct LogicalLayout {std::size_t domain=0,logical_values=0;unified::Packing packing;};
// physical_values counts populated source cells; zero cells still occupy domain positions.
struct PhysicalCommitmentLayout {std::size_t physical_values=0,physical_domain=0,rows=0,columns=0;};
struct PublicCell {std::size_t source=0,index=0,x_index=0;};
struct PhysicalRun {std::size_t source=0,source_offset=0,count=0,physical_offset=0;};
struct Plan:LogicalLayout,PhysicalCommitmentLayout {
 std::string source_identity;bool category_mode=false;
 // In category mode every segment (including native X) is committed.
 // Legacy test/probe plans keep public segment 0. No segment has a separate
 // commitment, basis, blind or opening identity.
 std::vector<Segment>segments;
 std::vector<PublicCell>public_cells;
 std::vector<PhysicalRun>runs;
};
Plan makePlan(const unified::Packing&,const std::vector<std::size_t>&actual,
 const std::string&source_identity,const std::vector<std::string>&source_ids={},const std::vector<PublicCell>&native_public={});
// Category segments are consecutive and may be unaligned. No storage chunk
// contributes padding or a protocol identity. Empty categories have no segment.
Plan makeCategoryPlan(const std::array<std::size_t,5>&actual,const std::string&source_identity);
void validateCategoryPlan(const Plan&);
PhysicalCommitmentLayout matrixShape(std::size_t actual);
unsigned bits(std::size_t);
Bytes descriptor(const Plan&);
struct Public {Plan plan;std::vector<G1>rows;};
void writeCategoryLayout(std::ostream&,const Public&);
struct Secret {Public pc;std::vector<Fr>blinds;std::vector<Value>sources;};
// Callbacks borrow immutable sealed source storage, and may be read in parallel.
// Only one columns-sized scalar buffer per worker is required. No GPU residency.
Secret commit(const Plan&,std::vector<Value>,Random&,unsigned workers=1);
Bytes encode(const Public&);Public decode(const Bytes&,const Plan&expected);
std::string identity(const Public&);
void writeLayout(std::ostream&,const Public&);
// One header and bounded row chunks belong to ONE commitment identity.
Public exchange(IO&,const Plan&,const Secret*onlyP);
// One rank-one logical MLE query over the identical commitment coordinates.
// Row/column weights only; no domain-sized coefficient array.
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
