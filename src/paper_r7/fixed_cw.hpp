#pragma once
#include "hyrax_a3.hpp"
#include <set>
namespace zkgpt::r7::stage2 {
using namespace crypto;
inline constexpr const char* suite="R7-HYRAX-A3-STAGE2-INTERACTIVE-v1";
inline constexpr const char* version="R7-CW-COMPONENT-REGISTRY-v1";
inline constexpr const char* componentId="C_W_component/Layer4-FC0-row0-col0/v1";
inline constexpr const char* manifestPin="3ab43263570f08a2fe314bca99f2f95c11020abae17eca587f1717f52d298073";
struct Registry {
 std::string id=componentId,profile,curve="MCL-BN254-y2=x3+2-Fr-v1",generators,layout="R6-Layer4-FC0-prefix768-zero-tail256-column-major",proof_suite=hyrax_a3::suite;
 std::size_t logical=768,shape_rows=1,shape_columns=768;PC pc;
 std::string registry_id,source_manifest=manifestPin,circuit="1cf3bc83a9a9f289e540238255d467b7fa6e2801209aa00f07fe232c48635a9a";
};
Registry registry(const PC&);
Bytes encodeRegistry(const Registry&);Registry decodeRegistry(const Bytes&);
std::string registryDigest(const Registry&);
// Loader consumes an independently pinned, launcher-owned regular-file FD.
// Prover wire bytes cannot create a trusted approval object.
class TrustedRegistry {
 Registry value_;explicit TrustedRegistry(Registry r):value_(std::move(r)){}
 public:static TrustedRegistry load(int fd,const std::string&approved_digest);
 const Registry& value()const{return value_;}
};
void registration(IO&,const Registry&approved,const PCSecret*,Random*);
Bytes readOwnedFD(int fd,bool private_material=false);void writeOwnedFD(int fd,const Bytes&,bool private_material=false);
}
