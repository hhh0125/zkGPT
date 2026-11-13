from pathlib import Path
import os
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(R)
S=R/'reports/R7-MLE-FOLD-20261108-v1';P=R/'src/paper_r7'
(P/'category_tiles.hpp').write_text('''#pragma once
#include "classified_layout.hpp"
#include "unified_input.hpp"
namespace zkgpt::r7::oneblock::classified {
// Computational dyadic views only; their union is exactly category actuals.
struct CategoryTile {unified::Segment segment;std::size_t root,begin;};
std::vector<CategoryTile> categoryTiles(const Layout&);
}
''')
p=P/'category_auth.cpp';s=p.read_text();s=s.replace('#include "public_k.hpp"','#include "public_k.hpp"\n#include "category_tiles.hpp"')
s=s.replace('struct Tile {unified::Segment segment;std::size_t root,begin;};\nstd::vector<Tile>tiles(const Layout&l){\n std::vector<Tile>out;', '}\nstd::vector<CategoryTile>categoryTiles(const Layout&l){\n std::vector<CategoryTile>out;')
s=s.replace('[](const Tile&a,const Tile&b)', '[](const CategoryTile&a,const CategoryTile&b)')
s=s.replace('return out;\n}\n}\nstd::vector<Fr>Commitments::categoryKernel','return out;\n}\nstd::vector<Fr>Commitments::categoryKernel')
s=s.replace('auto plan=tiles(layout_);','auto plan=categoryTiles(layout_);');p.write_text(s)
(P/'category_mle_fold_tests.cpp').write_text((S/'category_mle_fold_tests.cpp').read_text())
p=P/'CMakeLists.txt';s=p.read_text();s+='''
add_executable(r7_category_mle_fold_tests category_mle_fold_tests.cpp)
target_link_libraries(r7_category_mle_fold_tests PRIVATE paper_r7)
add_test(NAME r7_category_mle_fold COMMAND r7_category_mle_fold_tests)
set_tests_properties(r7_category_mle_fold PROPERTIES TIMEOUT 240 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} ENVIRONMENT "ZKGPT_THREADS=4;MALLOC_ARENA_MAX=2")
''';p.write_text(s)
print('phase A independent reference installed')
