#pragma once
#include "classified_layout.hpp"
#include "unified_input.hpp"
namespace zkgpt::r7::oneblock::classified {
// Computational dyadic views only; their union is exactly category actuals.
struct CategoryTile {unified::Segment segment;std::size_t root,begin;};
std::vector<CategoryTile> categoryTiles(const Layout&);
}
