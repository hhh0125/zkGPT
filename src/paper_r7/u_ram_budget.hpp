#pragma once
#include <cstdint>
#include <limits>
#include <string>
namespace zkgpt::r7::oneblock::unified {
// Public resource observations only; never contains input or challenge data.
struct UMemoryRoom {
 std::uint64_t system_available=0,cgroup_available=std::numeric_limits<std::uint64_t>::max();
 bool inspected=false;
 bool fits(std::uint64_t additional)const;
};
UMemoryRoom inspectUMemory(const std::string&proc="/proc",const std::string&cgroup="/sys/fs/cgroup");
enum class UFoldMode {Deferred,Immediate,Auto};
struct UImmediateOptions {
 UFoldMode mode=UFoldMode::Deferred;
 std::uint64_t budget=16ull<<30;
 static UImmediateOptions environment();
};
}
