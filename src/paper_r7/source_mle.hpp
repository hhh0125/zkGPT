#pragma once
#include "oneblock_sumcheck.hpp"
namespace zkgpt::r7::oneblock {
struct SourceMLEMetrics {std::uint64_t weight_peak_bytes=0,values_read=0,multiplications=0,additions=0;double weights_wall=0,dot_wall=0;};
Fr sourceMLE(std::size_t,const Value&,const std::vector<Fr>&,bool,SourceMLEMetrics* =nullptr);
}
