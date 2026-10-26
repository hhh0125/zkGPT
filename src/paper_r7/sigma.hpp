#pragma once
#include "crypto.hpp"
#include <array>
namespace zkgpt::r7::crypto {
struct Relations{std::string role;std::vector<Handle>handles;std::vector<std::vector<Fr>>linear;std::vector<std::array<std::size_t,3>>products;};
// Every relation, full handle and mask is fixed before rho; B fixed before c.
void sigma(IO&,const Relations&,const std::vector<Hidden>*,Random*);
struct SumcheckInput{PC pc;std::size_t n=0,degree=0,target=0;std::string role;};
// Complete reduction + independent endpoint/product batch + same-handle openings.
void committedSumcheck(IO&,const SumcheckInput&,const PCSecret*,Random*,const std::string&attack="");
void openingExample(IO&,std::size_t n,const PCSecret*,Random*,bool wrong_evaluation=false);
}
