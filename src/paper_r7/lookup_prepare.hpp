#pragma once
#include "classified_layout.hpp"
namespace zkgpt::r7::oneblock::classified {
using Prepared=std::map<std::string,std::shared_ptr<std::vector<Fr>>>;
Prepared prepareLookup(const R7Context&,const Value&base);
}
