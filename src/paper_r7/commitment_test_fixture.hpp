#pragma once
#include "classified_commitments.hpp"
#include <iostream>
using namespace zkgpt::r7;using namespace oneblock;using namespace classified;
namespace {
struct Tape:IO {bool p,zero_coin=false;int corrupt_padding=-1;std::vector<std::pair<std::string,Bytes>>data;std::size_t at=0,coins=0;explicit Tape(bool b):p(b){}bool prover()const override{return p;}std::string session()const override{return std::string(64,'a');}Digest prefix()const override{return {};}Bytes message(const std::string&n,const Bytes&b={})override{if(p){auto out=b;if(corrupt_padding>=0 && n=="Classified/root/0/PC"){auto pc=decodeVector(out);auto j=std::size_t(corrupt_padding);pc.points.at(j/pc.columns)+=hyrax_a3::generators(pc.columns).at(j%pc.columns);out=encodeVector(pc);}data.push_back({n,out});return out;}need(at<data.size()&&data[at].first==n,"classified unit order");return data[at++].second;}Fr coin(const std::string&)override{return zero_coin?Fr(0):Fr(3+(coins++%71));}};
template<class F>void rejects(F f,const std::string&why){bool bad=false;try{f();}catch(const std::exception&){bad=true;}need(bad,why.c_str());}
Layout small(){Layout p;p.native=std::string(64,'a');p.roots={{"C_aux",5,8,true,false},{"C_lookup/helpers/0",8,8,false,false}};for(unsigned j=0;j<5;++j)p.base.push_back({0,j});View v;v.id="unit/native-E";v.actual=5;v.padded=8;v.start={1,0};v.kind=ObjectKind::ExpHelper;p.views.emplace(v.id,v);return p;}
}
