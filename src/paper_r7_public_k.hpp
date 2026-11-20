#pragma once
#include "paper_r7_unified_input.hpp"
namespace zkgpt::r7::oneblock::unified {
// Deprecated compatibility argument. Anonymous mmap state is never admitted,
// evicted or rebuilt according to these values or legacy budget environment.
struct KBudget {
 std::uint64_t ram=512ull<<20,file=4ull<<30;
 static KBudget environment();
};
struct KMetrics {
 std::uint64_t unique_root_builds=0,rebuild_calls=0;
 // Live/peak logical Fr payload; rounded virtual mappings are separate.
 std::uint64_t mmap_live_bytes=0,mmap_peak_bytes=0,mapped_bytes=0,mapped_peak_bytes=0;
 std::uint64_t unmapped_tail_bytes=0,unmap_calls=0;
 // Physical pages sampled on metrics(), pre/post fold and report(); not a continuous high-water mark.
 std::uint64_t resident_bytes=0,resident_peak_bytes=0;
 bool resident_measurement_available=true;
 std::uint64_t initial_builds=0,fallback_rebuild_calls=0,fold_coordinates=0,state_read_coordinates=0;
 std::uint64_t builder_peak_bytes=0;
 std::uint64_t direct_build_calls=0,direct_build_elements=0,vector_builder_calls=0,vector_to_mmap_copy_bytes=0;
 std::uint64_t build_marker_write_bytes=0,build_validation_elements=0;
 double direct_build_seconds=0,build_marker_seconds=0,build_validation_seconds=0,vector_to_mmap_copy_seconds=0;
 double initial_build_seconds=0,fold_cpu_seconds=0,fold_wall_seconds=0;
 double mmap_allocation_seconds=0,unmap_wall_seconds=0;
 // Deprecated cache-only counters. Always zero for anonymous mmap storage.
 std::uint64_t file_read_bytes=0,file_write_bytes=0,ram_peak_bytes=0,file_peak_bytes=0;
 std::uint64_t ram_compactions=0,ram_released_bytes=0,compaction_copy_bytes=0;
 double compaction_seconds=0,io_wall_seconds=0;
 std::uint64_t ram_roots=0,file_roots=0,rebuild_roots=0;
};
// Public data only. The builder returns an authenticated, rho-fixed kernel;
// every root is retained once until authentication ends. No state is shared.
class PublicK {
 struct Impl;std::unique_ptr<Impl>p_;
 public:
 // Explicit compatibility path for non-category callers; never a failure fallback.
 using Build=std::function<std::vector<Fr>(std::size_t)>;
 // Synchronous borrowed output: Fr lifetimes already started. Assign every
 // element before arithmetic, obey count, and never retain the pointer.
 using BuildInto=std::function<void(std::size_t,Fr*,std::size_t)>;
 PublicK(const std::vector<Segment>&,BuildInto,KBudget,const std::string&session,std::size_t domain=0);
 PublicK(const std::vector<Segment>&,Build,KBudget,const std::string&session,std::size_t domain=0);
 ~PublicK();PublicK(const PublicK&)=delete;PublicK&operator=(const PublicK&)=delete;
 void withRoot(std::size_t,const std::string&session,const std::function<void(const Value&)>&);
 void advance(const std::string&session,const Fr&challenge);
 Fr endpoint(const std::string&session,const std::vector<Fr>&point);
 const KMetrics&metrics()const;
 void report()const;
};
}
