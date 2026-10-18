#include "zk_committed_linear_opening.hpp"
#include <iostream>
int main(){mcl::bn::initPairing(mcl::BN254);auto p=zkproto::committed::makeParameters(2);auto g=p.value_generator;g.normalize();mcl::bn::Fp b=g.y*g.y-g.x*g.x*g.x;
std::cout<<"Fr_order="<<mcl::bn::Fr::getModulo()<<"\nFp_modulus="<<mcl::bn::Fp::getModulo()<<"\ncurve_b="<<b<<"\nFr_bytes="<<mcl::bn::Fr::getByteSize()<<"\nG1_bytes="<<mcl::bn::G1::getSerializedByteSize()<<"\n";}
