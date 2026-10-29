#pragma once
#include "paper_r1/core.hpp"
#include "opt2_metrics.hpp"
#include <map>
#include <functional>
namespace zkgpt::r7::crypto {
namespace r1=zkgpt::r1;
using r1::Fr;using r1::G1;using r1::Bytes;using r1::Digest;
inline constexpr const char* suite="R7-MCL-BN254-H-PC-CSIPA-AUDIT-RESEARCH-v1";
inline constexpr std::size_t maxN=4096;
void need(bool,const char*);
struct Writer{Bytes b;void number(std::uint64_t,unsigned=4);void raw(const Bytes&);void text(const std::string&);void scalar(const Fr&);void point(const G1&);};
struct Reader{const Bytes&b;std::size_t at=0;Bytes raw(std::size_t);std::uint64_t number(unsigned=4);std::string text();Fr scalar();G1 point();void end();};
G1 decodePoint(const Bytes&);
class Random{public:virtual ~Random()=default;virtual void fill(std::uint8_t*,std::size_t)=0;Fr scalar(bool nonzero=false);Bytes bytes(std::size_t);};
class OSRandom:public Random{std::size_t draws_=0;std::map<Bytes,bool>seen_;public:void fill(std::uint8_t*,std::size_t)override;};
struct Parameters{std::string suite=crypto::suite;G1 scalar,hiding;std::vector<G1>columns;};
Parameters parameters(std::size_t columns);
void validate(const Parameters&);
struct Metrics{std::uint64_t msm_calls=0,msm_terms=0,max_msm=0;};
Metrics& metrics();
G1 msm(const std::vector<G1>&,const std::vector<Fr>&);
struct PCSpec{std::string suite=crypto::suite,role,layout="column-major-i+R*j";std::size_t actual=0,padded=0,rows=0,columns=0;static PCSpec make(const std::string&,std::size_t,std::size_t);};
struct PC{PCSpec spec;std::vector<G1>rows;};
struct PCSecret{PC public_pc;std::vector<Fr>values,blinds;};
void validate(const PCSpec&);void validate(const PC&);
PCSecret commit(const PCSpec&,const std::vector<Fr>&actual,Random&);
Bytes encode(const PC&);PC decodePC(const Bytes&);Digest pcIdentity(const PC&);
struct Handle{std::string suite=crypto::suite,session,role;G1 point;};
struct Hidden{Handle handle;Fr value=0,blind=0;};
Bytes encode(const Handle&);Handle decodeHandle(const Bytes&);
void validate(const Handle&,const std::string&session,const std::string&role);
Hidden hide(const std::string&session,const std::string&role,const Fr&,Random&);
G1 scalarCommit(const Fr&,const Fr&);
Hidden combine(const std::string&session,const std::string&role,const std::vector<Hidden>&,const std::vector<Fr>&);
std::string hex(const Digest&);
// Only a live verifier-side implementation may generate coins. No receipt import.
class IO{public:virtual ~IO()=default;virtual bool prover()const=0;virtual std::string session()const=0;virtual Bytes message(const std::string&,const Bytes& = {})=0;virtual Fr coin(const std::string&)=0;virtual Digest prefix()const=0;};
void bindPublic(IO&,const std::string&,const Bytes&);
struct Query{std::vector<Fr>point;Handle evaluation;};
// Each call binds all PC/query/handle bytes before one audit and K<=4 cores.
void opening(IO&,const PC&,const std::vector<Query>&,const PCSecret*,const std::vector<Hidden>*,Random*);
}
