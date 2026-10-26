#include "mapped_input.hpp"
#include <limits>
#include <iostream>
using namespace zkgpt::r1;
template<class Fn>void rejects(Fn f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}require(bad,"mapped invalid boundary accepted");}
int main(){try{init();auto m=PCSpec::make("boundary/master",7,8,Layout::RowMajor),q=PCSpec::make("boundary/child",4,4,Layout::ColumnMajor);std::vector<Fr>w{Fr(-5),Fr(1),Fr(2),Fr(3),Fr(4),Fr(5),Fr(6),Fr(0)};std::vector<Fr>v{Fr(134),Fr(123),Fr(134),Fr(134)};
 MappedInputSpec s{"boundary",commit(m,w),{{commit(q,v),{{6,1,Fr(128)},{0,1,Fr(128)},{6,2,Fr(128)}}}}, {}};
 // Last span intentionally invalid: actual ends7, source6+2 reaches padding.
 rejects([&]{s.validate();});s.children[0].spans={{6,1,Fr(128)},{0,1,Fr(128)},{6,1,Fr(128)},{6,1,Fr(128)}};
 Fs a("bound"),b("bound");auto proof=proveMappedInput(s,w,{v},a);verifyMappedInput(s,proof,b);require(a.digest()==b.digest(),"boundary transcript");
 const auto max=std::numeric_limits<std::size_t>::max();unsigned rejected=0;
 for(auto span:decltype(s.children[0].spans){{max,1,Fr(0)},{0,max,Fr(0)},{max,max,Fr(0)},{7,1,Fr(0)},{6,2,Fr(0)},{0,0,Fr(0)}}){auto bad=s;bad.children[0].spans={span};rejects([&]{Fs t("bound");proveMappedInput(bad,w,{v},t);});++rejected;}
 for(auto wrong:std::vector<std::vector<Fr>>{{},{Fr(134)},{Fr(134),Fr(123),Fr(134)}, {Fr(134),Fr(123),Fr(134),Fr(134),Fr(0)},{Fr(0),Fr(123),Fr(134),Fr(134)}}){rejects([&]{Fs t("bound");proveMappedInput(s,w,{wrong},t);});++rejected;}
 auto bad=s;bad.children[0].spans={{0,3,Fr(128)},{1,max,Fr(128)}};rejects([&]{Fs t("bound");proveMappedInput(bad,w,{v},t);});++rejected;
 auto padded=PCSpec::make("boundary/padded",3,4,Layout::RowMajor);std::vector<Fr>vp{w[6],w[0],w[6],Fr(0)};s.children={{{commit(padded,vp)},{{6,1,Fr(0)},{0,1,Fr(0)},{6,1,Fr(0)}}}};
 {Fs x("tail"),y("tail");auto p=proveMappedInput(s,w,{vp},x);verifyMappedInput(s,p,y);}vp[3]=Fr(1);rejects([&]{Fs t("tail");proveMappedInput(s,w,{vp},t);});++rejected;
 auto shortw=w;shortw.pop_back();rejects([&]{Fs t("tail");proveMappedInput(s,shortw,{vp},t);});++rejected;
 std::cout<<"PASS "<<rejected<<" span/size/overflow negative cases; lastvalidindex,repeatednegativebias,padding included\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
