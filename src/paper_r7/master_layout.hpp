#pragma once
#include "r6_adapter.hpp"
#include "crypto.hpp"
namespace zkgpt::r7::master3 {
using namespace crypto;
inline constexpr const char*suite="R7-STAGE3A-STREAMING-MASTER-A3-v1";
struct Region {std::string id,kind,order="original-R6-val0";std::uint64_t offset=0,count=0,rows=1,columns=0,stride=1,logical_rows=0,logical_columns=0;std::string alias="";};
struct Layout {std::uint64_t actual=0,padded=0,object_count=0;std::string profile,context;std::vector<Region>regions;std::vector<std::size_t>projection;std::vector<WitnessObject>views;};
Layout trustedLayout(const R7Context&);
R7Context trustedContext(unsigned blocks=1);
class QuietStdout {int saved_=-1;public:QuietStdout();~QuietStdout();};
void validate(const Layout&);
Bytes encodeLayout(const Layout&);
std::string layoutDigest(const Layout&);
std::string scopeDigest(const Layout&,std::size_t N,bool projection);
struct MapTerm {std::size_t child=0,source=0;Fr coefficient=1;};
struct Gather {std::string id;std::size_t child_size=0;std::vector<MapTerm>terms;std::vector<Fr>bias;};
Gather fcMap(unsigned);
Gather testMap(const std::string&);
void validate(const Gather&,std::size_t);
Bytes encodeGather(const Gather&);
std::vector<Fr> gatherKernel(const Gather&,const std::vector<Fr>&,std::size_t,Fr&);
}
