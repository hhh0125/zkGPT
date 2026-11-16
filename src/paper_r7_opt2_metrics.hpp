#pragma once
#include <cstddef>
#include <cstdint>
namespace zkgpt::r7::crypto {
// Public length/time/allocation metadata only. Never inspect scalar bytes here.
double processCPU();
bool opt2MetricsEnabled();
// Scoped ordinary C++ allocation counters on this thread (not C malloc/aligned).
class AllocationTrace {
 std::uint64_t count_,bytes_;
 public:AllocationTrace();~AllocationTrace();
 AllocationTrace(const AllocationTrace&)=delete;AllocationTrace&operator=(const AllocationTrace&)=delete;
 std::uint64_t count()const;std::uint64_t bytes()const;
};
class MSMTrace {
 std::size_t bin_;double started_;std::uint64_t allocations_;
 public:explicit MSMTrace(std::size_t);~MSMTrace();
};
}
