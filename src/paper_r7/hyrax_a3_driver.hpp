#pragma once
#include "interactive.hpp"
namespace zkgpt::r7::hyrax_a3 {
// Bounded experiment driver. Attack behavior belongs only to these test targets.
void experiment(crypto::PipeIO&,const std::string&mode,std::size_t n,const std::string&attack);
}
