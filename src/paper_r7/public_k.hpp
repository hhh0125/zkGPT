#pragma once
#include "unified_input.hpp"
namespace zkgpt::r7::oneblock::unified {
struct KBudget {
 std::uint64_t ram=512ull<<20,file=4ull<<30;
 static KBudget environment();
};
struct KMetrics {
 std::uint64_t initial_builds=0,fallback_rebuild_calls=0,fold_coordinates=0,state_read_coordinates=0;
 std::uint64_t file_read_bytes=0,file_write_bytes=0,ram_peak_bytes=0,file_peak_bytes=0;
 std::uint64_t builder_peak_bytes=0;
 std::uint64_t ram_roots=0,file_roots=0,rebuild_roots=0;
 double initial_build_seconds=0,fold_cpu_seconds=0,fold_wall_seconds=0,io_wall_seconds=0;
};
// Public data only. The builder must return an authenticated, rho-fixed kernel;
// it must never return witness values. No object is shared across authentication.
class PublicK {
 struct Impl;std::unique_ptr<Impl>p_;
 public:
 using Build=std::function<std::vector<Fr>(std::size_t)>;
 PublicK(const std::vector<Segment>&,Build,KBudget,const std::string&session,std::size_t domain=0);
 ~PublicK();PublicK(const PublicK&)=delete;PublicK&operator=(const PublicK&)=delete;
 void withRoot(std::size_t,const std::string&session,const std::function<void(const Value&)>&);
 void advance(const std::string&session,const Fr&challenge);
 Fr endpoint(const std::string&session,const std::vector<Fr>&point);
 const KMetrics&metrics()const;
 void report()const;
};
}
