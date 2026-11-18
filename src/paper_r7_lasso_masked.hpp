#pragma once
#include "paper_r7_oneblock_sumcheck.hpp"
namespace zkgpt::r7::oneblock::masked {
enum class Mode { Sigma, Product01Research, AllProductsResearch };
inline thread_local Mode active=Mode::Sigma;
class ScopedMode {
 Mode previous_;
 public:explicit ScopedMode(Mode m):previous_(active){active=m;}
 ~ScopedMode(){active=previous_;}
 ScopedMode(const ScopedMode&)=delete;ScopedMode&operator=(const ScopedMode&)=delete;
};
std::array<Reduction,2> batch(IO&,std::size_t,const std::array<Hidden,2>&,const std::vector<Value>&,Random*,const std::string&,const std::array<ProductInputs,2>*,const std::vector<Fr>*);
void originalSingleTerminal(IO&,const Hidden&,const std::vector<Hidden>&,Random*,const std::string&);
Reduction single(IO&,std::size_t,const Hidden&,const std::vector<Value>&,Random*,const std::string&,const ProductInputs*,bool);
inline bool batchEnabled(){return active!=Mode::Sigma;}
inline bool singleEnabled(){return active==Mode::AllProductsResearch;}
}
