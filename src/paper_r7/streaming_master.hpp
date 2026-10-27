#pragma once
#include "master_layout.hpp"
#include "hyrax_a3.hpp"
#include "stage2_opening.hpp"
namespace zkgpt::r7::master3 {
using Value=std::function<Fr(std::size_t)>;
struct Spec {std::size_t actual=0,N=0,rows=0,columns=1024;std::string layout,profile,scope,generators;};
struct Master {Spec spec;std::vector<G1>rows;};
struct Secret {Master public_pc;std::vector<Fr>blinds;Value value;};
Spec makeSpec(const Layout&,std::size_t,bool projection=false);
void validate(const Spec&);
Bytes encodeMaster(const Master&);
Master decodeMaster(const Bytes&,const Spec&expected);
std::string identity(const Master&);
G1 masterMSM(const std::vector<G1>&,const std::vector<Fr>&);
Secret commitMaster(const Spec&,const Value&);
bool registrationSupported(const Spec&);
void registerMaster(IO&,const Master&,const Secret*);
void openMaster(IO&,const Master&,const Query&,const Secret*,const Hidden*,Random*,const std::string&);
Fr evaluate(const Secret&,const std::vector<Fr>&);
class MappedValues {const std::uint8_t*data_=nullptr;std::size_t bytes_=0;public:explicit MappedValues(int);~MappedValues();MappedValues(const MappedValues&)=delete;Fr at(std::size_t)const;};
void cacheValues(int,const std::vector<Fr>&);
}
