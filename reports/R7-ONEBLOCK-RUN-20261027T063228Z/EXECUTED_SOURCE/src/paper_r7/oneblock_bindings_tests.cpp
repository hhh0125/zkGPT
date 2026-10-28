#include "oneblock_bindings.hpp"
#include <algorithm>
#include <iostream>

using namespace zkgpt::r7;
using namespace oneblock;
using namespace crypto;
namespace {
struct CountRandom final:Random {
 unsigned draws=0;
 void fill(std::uint8_t*p,std::size_t n)override{auto b=r1::encode(Fr(++draws));need(b.size()==n,"bindings test entropy width");std::copy(b.begin(),b.end(),p);}
};
struct Tape final:IO {
 bool p;std::vector<std::pair<std::string,Bytes>>records;std::vector<std::string>coin_labels;std::size_t at=0,coins=0;bool wrong_lambda=false,bound_claims=false,zero_lambda=false;
 explicit Tape(bool role):p(role){}
 bool prover()const override{return p;}std::string session()const override{return std::string(64,'d');}Digest prefix()const override{return {};}
 Bytes message(const std::string&label,const Bytes&b={})override{if(label=="unit/mapped/claims")bound_claims=true;if(p){records.emplace_back(label,b);return b;}need(at<records.size() && records[at].first==label,"bindings independent verifier message order");return records[at++].second;}
 Fr coin(const std::string&label)override{if(label.rfind("unit/mapped/lambda/",0)==0)need(bound_claims,"all mapped claim identities must precede every fresh aggregation coin");coin_labels.push_back(label);Fr c(3+coins++);if(wrong_lambda && label=="unit/mapped/lambda/0")c+=Fr(1);if(zero_lambda && label.find("/lambda/")!=std::string::npos)c=0;return c;}
};
void reject(const std::function<void()>&f,const char*why,bool algebra=false){bool failed=false;try{f();}catch(const std::exception&e){auto text=std::string(e.what());failed=!algebra || text.find("relation failed")!=std::string::npos || text.find("equation failed")!=std::string::npos;}need(failed,why);}
std::vector<MappedMasterClaim> publicClaims(const std::vector<MappedMasterClaim>&claims){auto out=claims;for(auto&c:out){auto handle=c.value.handle;c.value=Hidden{};c.value.handle=handle;}return out;}
}
int main(){try{
 r1::init();auto context=master3::trustedContext();auto layout=master3::trustedLayout(context);auto spec=master3::makeSpec(layout,4096,true);
 Value source=[](std::size_t j){return Fr(int(j%17)-8);};auto secret=master3::commitMaster(spec,source);CountRandom random;
 std::vector<MappedMasterClaim>claims;
 for(unsigned scenario=0;scenario<3;++scenario){
  MappedMasterClaim c;c.id="query/"+std::to_string(scenario);c.actual=scenario==2?1:13;c.padded=scenario==2?1:16;c.bias=Fr(int(scenario)-1);
  if(scenario==1)c.coordinate_bias=[](std::size_t j){return Fr(int(j%3)-1);};
  c.index=[scenario](std::size_t j){return scenario==0?7+3*j:scenario==1?3+(j%3):std::size_t(2050-1);};
  c.point.assign(r1::log2exact(c.padded),Fr(2+scenario));std::vector<Fr>values(c.padded,Fr(0));
  for(std::size_t j=0;j<c.actual;++j)values[j]=source(c.index(j))+(c.coordinate_bias?c.coordinate_bias(j):c.bias);
  c.value=hide(std::string(64,'d'),"unit/claim/"+std::to_string(scenario),r1::mle(values,c.point),random);claims.push_back(std::move(c));
 }
 Tape p(true);masterB::RegistrationMetrics pStats;auto registeredP=masterB::Registered::establish(p,secret.public_pc,&secret,"bindings-unit",pStats);
 authenticateMappedClaims(p,registeredP,&secret,source,claims,&random,"unit/mapped");
 need(pStats.verified_rows==spec.rows && pStats.padded_coordinates==spec.N-spec.actual,"bindings fixture actually registers every row and padding");
 Tape v(false);v.records=p.records;masterB::RegistrationMetrics vStats;auto registeredV=masterB::Registered::establish(v,secret.public_pc,nullptr,"bindings-unit",vStats);
 authenticateMappedClaims(v,registeredV,nullptr,{},publicClaims(claims),nullptr,"unit/mapped");
 need(v.at==v.records.size() && v.coin_labels==p.coin_labels,"bindings independent verifier checks registration, reduction and A3");
 auto firstLambda=std::find(p.coin_labels.begin(),p.coin_labels.end(),"unit/mapped/lambda/0");need(firstLambda!=p.coin_labels.end(),"bindings uses fresh verifier aggregation coins");
 // A fixed wrong value is otherwise well-formed and privately known. It must
 // reach an arithmetic equation, rather than a canonical-encoding rejection.
 auto wrongClaim=claims;wrongClaim[0].value=hide(p.session(),"unit/claim/wrong",claims[0].value.value+Fr(1),random);
 reject([&]{authenticateMappedClaims(p,registeredP,&secret,source,wrongClaim,&random,"unit/wrong-claim");},"bindings rejects wrong claimed value algebraically",true);
 auto wrongIndex=claims;wrongIndex[0].index=[](std::size_t j){return 8+3*j;};
 reject([&]{authenticateMappedClaims(p,registeredP,&secret,source,wrongIndex,&random,"unit/wrong-index");},"bindings rejects altered mapping with unchanged claim algebraically",true);
 // P's altered mapping cannot replace the independently trusted V mapping.
 Tape mappedV(false);mappedV.records=v.records;masterB::RegistrationMetrics mappedStats;auto mappedRegistry=masterB::Registered::establish(mappedV,secret.public_pc,nullptr,"bindings-unit",mappedStats);
 auto different=publicClaims(claims);different[0].index=[](std::size_t j){return 8+3*j;};
 reject([&]{authenticateMappedClaims(mappedV,mappedRegistry,nullptr,{},different,nullptr,"unit/mapped");},"bindings binds every trusted mapping before aggregation");
 Tape challengedV(false);challengedV.records=v.records;challengedV.wrong_lambda=true;masterB::RegistrationMetrics challengedStats;
 auto challengedRegistry=masterB::Registered::establish(challengedV,secret.public_pc,nullptr,"bindings-unit",challengedStats);
 reject([&]{authenticateMappedClaims(challengedV,challengedRegistry,nullptr,{},publicClaims(claims),nullptr,"unit/mapped");},"bindings proof cannot import old aggregation challenges");
 auto outside=claims;outside[0].index=[spec](std::size_t){return spec.actual;};
 reject([&]{authenticateMappedClaims(p,registeredP,&secret,source,outside,&random,"unit/outside");},"bindings rejects a map into the padded Master tail");
 p.zero_lambda=true;reject([&]{authenticateMappedClaims(p,registeredP,&secret,source,claims,&random,"unit/zero-lambda");},"bindings rejects a zero aggregation challenge");p.zero_lambda=false;
 std::cout<<"ONEBLOCK_BINDINGS_UNIT_PASSED claims="<<claims.size()<<" registered_rows="<<vStats.verified_rows<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"ONEBLOCK_BINDINGS_UNIT_FAILED "<<e.what()<<'\n';return 1;}}
