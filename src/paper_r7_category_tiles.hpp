#pragma once
#include "paper_r7_classified_layout.hpp"
#include "paper_r7_unified_input.hpp"
namespace zkgpt::r7::oneblock::classified {
// Computational dyadic views only; their union is exactly category actuals.
struct CategoryTile {unified::Segment segment;std::size_t root,begin;};
std::vector<CategoryTile> categoryTiles(const Layout&);
}
