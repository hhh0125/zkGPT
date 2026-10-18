#pragma once
#include "range_logup.hpp"
namespace zkgpt::zk::testing {
inline const char* fixtureContext(){return "task12/logup/public-fixture/v1";}
inline std::vector<Fr> publicFixtureTable(const std::string& id){
    if(id=="arbitrary-fr-table-v1")return {Fr(2)/Fr(3),Fr(5)/Fr(7),Fr(11),Fr(19),Fr(2)/Fr(3),Fr(29),Fr(41),Fr(67)};
    if(id=="repeated-table-v1")return {Fr(7)/Fr(13),Fr(7)/Fr(13),Fr(7)/Fr(13),Fr(7)/Fr(13)};
    if(id=="singleton-table-v1")return {Fr(7)/Fr(13)};
    throw std::invalid_argument("unexpected application table identifier");
}
inline void checkExpectedFixture(const FieldLogUpZkStatement& s,const std::string& expectedContext){
    if(s.context!=expectedContext || s.query_index!=2 || s.layer_id!=3 || s.circuit_id[0]!=12 ||
       s.generator_domain.domain!="zkGPT/range" || s.table!=publicFixtureTable(s.table_id))
        throw std::invalid_argument("unexpected application context/table/domain/metadata");
    for(size_t i=1;i<s.circuit_id.size();++i)if(s.circuit_id[i]!=0)
        throw std::invalid_argument("unexpected circuit identifier");
}
}
