#pragma once
#include "sigma.hpp"
#include <functional>
#include <memory>
namespace zkgpt::r7::oneblock {
using namespace crypto;
using Value=std::function<Fr(std::size_t)>;
struct Monomial {Fr coefficient;std::vector<std::size_t>factors;};
struct Expression {std::size_t factor_count;std::vector<Monomial>terms;};
struct Reduction {Hidden initial;std::vector<Hidden>endpoints;std::vector<Fr>point;};
struct VectorOracle {
 std::size_t actual=0,padded=0;std::string id;Value value;
 std::function<void(IO&,const Query&,const Hidden*,Random*,std::string)>open;
};
using VectorFactory=std::function<VectorOracle(IO&,std::string,std::size_t,std::size_t,Value)>;
struct EvidenceSink {virtual ~EvidenceSink()=default;virtual void checkedRelation(const std::string&)=0;};
inline void checked(IO&io,std::string id){if(id.rfind("r7/",0)==0)id.erase(0,3);if(auto sink=dynamic_cast<EvidenceSink*>(&io))sink->checkedRelation(id);}
Reduction expressionSumcheck(IO&,std::size_t,const Hidden&,const std::vector<Value>&,const Expression&,Random*,const std::string&);
Hidden publicHidden(IO&,const std::string&,const Fr&);
Hidden exchangeHidden(IO&,const std::string&,const Fr&,Random*);
void equalHidden(IO&,const Hidden&,const Fr&,Random*,const std::string&);
// New live profile only: system entropy in bounded diagnostic windows. No seed
// expansion. Old per-component OSRandom and all old test profiles are unchanged.
class SessionRandom:public Random {
 std::unique_ptr<OSRandom>window_=std::make_unique<OSRandom>();std::size_t draws_=0;
 public:void fill(std::uint8_t*p,std::size_t n)override{if(++draws_%4096==0)window_=std::make_unique<OSRandom>();window_->fill(p,n);}
};
}
