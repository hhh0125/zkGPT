#pragma once
#include "oneblock_plan.hpp"
#include "oneblock_sumcheck.hpp"
#include <map>
namespace zkgpt::r7::oneblock::classified {
inline constexpr const char*suite="R7-CLASSIFIED-HYRAX-LOOKUP-INTERACTIVE-ROWMAJOR-v4";
struct Coordinate {std::uint32_t root=0,index=0;bool operator==(const Coordinate&o)const{return root==o.root&&index==o.index;}};
// registered classifies native base roots for packing and layout identity only.
// It does not enable full row knowledge registration.
struct RootSpec {std::string id;std::size_t actual=0,padded=0;bool registered=false,model=false;};
struct Piece {std::size_t begin=0,count=0;Coordinate start;};
struct View {std::string id;std::size_t actual=0,padded=0;bool mapped=false;std::vector<r1::InputSpan>spans;Coordinate start;ObjectKind kind;std::vector<Piece>pieces;};
struct Layout {std::string native;std::vector<RootSpec>roots;std::vector<Coordinate>base;std::map<std::string,View>views;std::vector<Piece>base_runs;std::size_t master_actual=0;};
Layout makePartitionedLayout(const R7Context&,std::size_t);
std::size_t baseSize(const Layout&);Coordinate baseCoordinate(const Layout&,std::size_t);
std::string partitionedIdentity(const Layout&);
void writePartitionedLayout(std::ostream&,const Layout&);
struct Located {Coordinate coordinate;Fr bias=0;bool constant=false;};
Layout makeLayout(const R7Context&,std::size_t slab_limit=1u<<24);
void validate(const Layout&,const R7Context&,std::size_t slab_limit=1u<<24);
std::string identity(const Layout&);
Located locate(const Layout&,const View&,std::size_t);
void writeLayout(std::ostream&,const Layout&);
std::size_t power2(std::size_t);
}
