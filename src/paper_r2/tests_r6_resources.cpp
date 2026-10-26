#include "r6_system.hpp"
#include <iostream>
int main(){try{using zkgpt::r6::boundedRoom;if(boundedRoom(1000,100,30)!=70)throw std::logic_error("cgroup memory room ignored");if(boundedRoom(1000,100,110)!=0)throw std::logic_error("cgroup underflow");if(boundedRoom(20,100,30)!=20)throw std::logic_error("host room ignored");if(boundedRoom(1000,64,64)!=0)throw std::logic_error("PID cap ignored");std::cout<<"PASS effective memory/PID headroom including saturation\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
