#pragma once
#include "oneblock_plan.hpp"
#include "oneblock_sumcheck.hpp"
#include <map>
namespace zkgpt::r7::oneblock::classified {
inline constexpr const char*suite="R7-CLASSIFIED-HYRAX-LOOKUP-INTERACTIVE-v2";
struct Coordinate {std::uint32_t root=0,index=0;bool operator==(const Coordinate&o)const{return root==o.root&&index==o.index;}};
struct RootSpec {std::string id;std::size_t actual=0,padded=0;bool registered=false,model=false;};
struct View {std::string id;std::size_t actual=0,padded=0;bool mapped=false;std::vector<r1::InputSpan>spans;Coordinate start;ObjectKind kind;};
struct Layout {std::string native;std::vector<RootSpec>roots;std::vector<Coordinate>base;std::map<std::string,View>views;};
struct Located {Coordinate coordinate;Fr bias=0;bool constant=false;};
Layout makeLayout(const R7Context&,std::size_t slab_limit=1u<<24);
void validate(const Layout&,const R7Context&);
std::string identity(const Layout&);
Located locate(const Layout&,const View&,std::size_t);
void writeLayout(std::ostream&,const Layout&);
std::size_t power2(std::size_t);
}
