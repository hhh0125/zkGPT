#include "core.hpp"
#include <iostream>
using namespace zkgpt::r1;
int main(){init();Fr direct(UINT64_MAX);Fr h(std::uint64_t(1)<<32);Fr exact=h*h-Fr(1);std::cout<<"direct="<<direct.getStr()<<"\nexact="<<exact.getStr()<<"\n";return direct==Fr(-1)&&exact.getStr()=="18446744073709551615"?0:1;}
