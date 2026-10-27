#pragma once
#include "master_registration.hpp"
namespace zkgpt::r7::masterB {
std::vector<std::size_t> globalMap(const Layout&,std::size_t,bool real);
SparsePublicKernel kernelFor(const std::vector<std::size_t>&,const std::vector<Fr>&,std::size_t);
Fr sparseMLE(const SparsePublicKernel&,const std::vector<Fr>&);
void globalBridge(IO&,const Registered&,const Registered&,const Layout&,const Secret*,const Secret*,Random*,const std::string&attack="");
}
