#pragma once
#include <cstddef>
#include <cstdint>
namespace zkgpt::r7::crypto {
// Public length/time/allocation metadata only. Never inspect scalar bytes here.
double processCPU();
bool opt2MetricsEnabled();
class MSMTrace {
 std::size_t bin_;double started_;std::uint64_t allocations_;
 public:explicit MSMTrace(std::size_t);~MSMTrace();
};
}
