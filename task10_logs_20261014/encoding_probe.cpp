#include <mcl/bn.hpp>
#include <iostream>
int main(){mcl::bn::initPairing(mcl::BN254);std::cout<<"Fr_order="<<mcl::bn::Fr::getModulo()<<"\nFp_modulus="<<mcl::bn::Fp::getModulo()<<"\nFr_bytes="<<mcl::bn::Fr::getByteSize()<<"\nG1_bytes="<<mcl::bn::G1::getSerializedByteSize()<<"\n";}
