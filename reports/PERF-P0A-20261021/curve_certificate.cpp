#include "core.hpp"
#include <iostream>
using namespace zkgpt::r1;
int main(){init();G1 p,q;p.x=-1;p.y=1;p.z=1;require(!p.isZero()&&mcl::ec::isValidJacobi(p),"point");G1::mulGeneric(q,p,G1::order_);require(q.isZero(),"rP");std::cout<<"PASS P=(-1,1) nonzero/on-curve; unreduced generic [r]P=infinity; verifyOrder="<<G1::verifyOrder_<<"\n";}
