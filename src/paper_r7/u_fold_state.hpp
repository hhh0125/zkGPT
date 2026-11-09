#pragma once
#include "unified_input.hpp"
namespace zkgpt::r7::oneblock::unified {
struct UFoldMetrics {
 std::uint64_t capacity_peak_bytes=0,live_bytes=0,roots_materialized=0,advances=0;
 std::uint64_t original_read_coordinates=0,cache_fold_read_coordinates=0,fold_input_coordinates=0,fold_write_coordinates=0,multiplications=0,additions=0;
 double fold_wall_seconds=0,fold_cpu_seconds=0;
};
// One authentication's private derived state. Original sealed Sources stay
// unchanged. No callbacks, files or state survive this object; no V access.
class FoldedU {
 struct Impl;std::unique_ptr<Impl>p_;
 public:
 FoldedU(std::size_t,const std::vector<Segment>&,const Inputs&,const std::string&,std::uint64_t budget,bool deferred_callbacks=false);
 ~FoldedU();FoldedU(const FoldedU&)=delete;FoldedU&operator=(const FoldedU&)=delete;
 static bool requested();static std::uint64_t environmentBudget();
 bool enabled()const;bool cached()const;
 void capture(std::size_t,std::size_t,const Fr*,std::size_t);std::size_t domain()const;
 void beginRound(const std::string&,const std::vector<Fr>&,const std::vector<Segment>&);
 InputSpan span(std::size_t,const std::string&,const std::vector<Fr>&,InputSpan original);
 void completeRound(const std::string&,const std::vector<Fr>&);
 void advance(const std::string&,const Fr&,const Inputs&original);
 void poison()noexcept;const UFoldMetrics&metrics()const;void report()const;
};
}
