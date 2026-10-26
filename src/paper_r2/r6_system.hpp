#pragma once
#include "model_resources.hpp"
#include <filesystem>
#include <fstream>
#include <limits>
#include <vector>
namespace zkgpt::r6 {
inline std::size_t boundedRoom(std::size_t available,std::size_t limit,std::size_t current){return std::min(available,current>=limit?std::size_t(0):limit-current);}
struct SystemRoom{std::size_t memory,pids;};
inline SystemRoom effectiveRoom(){SystemRoom room{r2::availableMemory(),std::numeric_limits<std::size_t>::max()};std::ifstream cg("/proc/self/cgroup");std::string line,path;while(std::getline(cg,line))if(line.rfind("0::",0)==0)path=line.substr(3);r1::require(!path.empty(),"R6 requires visible unified cgroup membership");
 std::filesystem::path root="/sys/fs/cgroup",at=root/std::filesystem::path(path).relative_path();r1::require(std::filesystem::is_directory(at),"R6 cannot inspect own cgroup");
 while(true){for(bool memory:{true,false}){auto max=at/(memory?"memory.max":"pids.max"),used=at/(memory?"memory.current":"pids.current");if(std::filesystem::is_regular_file(max)){std::ifstream m(max);std::string cap;r1::require(bool(m>>cap),"R6 cgroup limit unreadable");if(cap!="max"){std::ifstream u(used);std::size_t current;r1::require(bool(u>>current),"R6 cgroup usage unreadable");auto limit=std::stoull(cap);if(memory)room.memory=boundedRoom(room.memory,limit,current);else room.pids=boundedRoom(room.pids,limit,current);}}}if(at==root)break;at=at.parent_path();}
 return room;
}
}
