#pragma once
#include "classified_layout.hpp"
namespace zkgpt::r7::oneblock::classified {
// Owned public metadata only. Neither class retains Layout/View references.
class StaticBaseMap {
 std::size_t size_;std::vector<Piece>runs_;std::vector<std::uint32_t>directory_;
 public:explicit StaticBaseMap(const Layout&);
 Coordinate at(std::size_t)const;
 const std::vector<Piece>&runs()const{return runs_;}
 std::size_t bytes()const{return sizeof(*this)+runs_.capacity()*sizeof(Piece)+directory_.capacity()*sizeof(std::uint32_t);}
};
class StaticSourceMap {
 struct Run {std::size_t begin,count;Located first;};
 std::size_t padded_;std::vector<Run>runs_;
 public:StaticSourceMap(const Layout&,const View&,const StaticBaseMap&);
 Located at(std::size_t)const;
 std::size_t bytes()const{return sizeof(*this)+runs_.capacity()*sizeof(Run);}
};
}
