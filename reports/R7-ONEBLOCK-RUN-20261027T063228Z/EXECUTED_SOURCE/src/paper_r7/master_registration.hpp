#pragma once
#include "master_bridge.hpp"
namespace zkgpt::r7::masterB {
using namespace master3;
inline constexpr const char*sessionSuite="R7-STAGE3B1-CHUNKED-MASTER-SESSION-v1";
inline constexpr const char*registrationSuite="R7-STAGE3B1-CONSTRAINED-ROW-SCHNORR-v1";
inline constexpr std::size_t chunkRows=64;
struct RegistrationMetrics {std::size_t verified_rows=0,chunks=0,padded_coordinates=0;double mask_seconds=0,response_seconds=0,verify_seconds=0;std::uint64_t response_payload_bytes=0;};
std::size_t activeColumns(const Spec&,std::size_t);
Bytes chunkHeader(IO&,const Master&,std::size_t,std::size_t,const std::string&,const std::string&);
void checkChunk(IO&,const Master&,const Secret*,std::size_t,std::size_t,const std::string&,RegistrationMetrics&,std::set<std::string>&,const std::string&,const std::string&attack="");
class Registered {
 IO&io_;const Master&master_;std::string session_;
 Registered(IO&i,const Master&m):io_(i),master_(m),session_(i.session()){}
 public:Registered(const Registered&)=delete;Registered(Registered&&)=default;
 static Registered establish(IO&,const Master&,const Secret*,const std::string&,RegistrationMetrics&,const std::string&attack="");
 const Master&get(IO&i)const{need(&i==&io_ && i.session()==session_,"B1 registered context/session mismatch");return master_;}
 void opening(IO&i,const Query&q,const Secret*s,const Hidden*h,Random*r,const std::string&id)const{openMaster(i,get(i),q,s,h,r,id);}
};
}
