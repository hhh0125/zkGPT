#pragma once
#include "paper_r7_unified_input.hpp"

namespace zkgpt::r7::oneblock::unified::detail {
// A reduction owns one fixed Segment order. Retained U shares its snapshot.
class ValidatedLayout {
 std::size_t domain_;
 std::shared_ptr<const std::vector<Segment>>segments_;
 public:
 ValidatedLayout(std::size_t n,const std::vector<Segment>&segments);
 std::size_t domain()const{return domain_;}
 ValidatedLayout(const ValidatedLayout&)=default;
 const std::vector<Segment>&segments()const{return *segments_;}
 Round streamingRound(const std::vector<Fr>&,const Factors&,const Inputs&,UOptions,UMetrics*,FoldedU*,const std::string&,PublicK*,Workers*)const;
};
}
