#pragma once
#include "paper_r7_classified_layout.hpp"
#include "paper_r7_native_workers.hpp"
namespace zkgpt::r7::oneblock::classified {
using Prepared=std::map<std::string,std::shared_ptr<std::vector<Fr>>>;
Prepared prepareLookup(const R7Context&,const Value&base);
Prepared prepareLookupWithWorkers(const R7Context&,const Value&base,detail::NativeWorkers&);
}
