#include "oneblock_sumcheck.hpp"
#include <iostream>
using namespace zkgpt::r7::oneblock;
int main(){try{
 r1::init();std::vector<Fr>w{Fr(2),Fr(3),Fr(5),Fr(7)},child{Fr(11),Fr(13),Fr(17),Fr(19),Fr(23),Fr(29),Fr(31),Fr(37)};
 ProductInputs inputs{w,child};auto c=detail::productPolynomialContiguous(inputs);
 need(c==std::vector<Fr>({Fr(3621),Fr(3845),Fr(1368),Fr(164)}),"C2 hand-derived cubic coefficients");
 std::vector<std::vector<Fr>>folded(3);detail::foldProductInputs(inputs,Fr(3),folded);
 need(folded==std::vector<std::vector<Fr>>({{Fr(5),Fr(11)},{Fr(29),Fr(47)},{Fr(31),Fr(53)}}),"C2 hand-derived challenge fold");
 for(unsigned bits=1;bits<=10;++bits){auto n=std::size_t(1)<<bits;w.resize(n);child.resize(2*n);for(std::size_t j=0;j<n;++j)w[j]=Fr(std::int64_t(j%7)-3);for(std::size_t j=0;j<2*n;++j)child[j]=Fr(std::int64_t(j%13)-6);
  std::vector<Value>f{[&](std::size_t j){return w[j];},[&](std::size_t j){return child[2*j];},[&](std::size_t j){return child[2*j+1];}};
  auto generic=detail::expressionPolynomial(Expression{3,{{Fr(1),{0,1,2}}}},n,f,{},true);need(detail::productPolynomialContiguous(inputs)==generic,"C2 generic coefficient mismatch");
  for(auto r:{Fr(0),Fr(1),Fr(-1),Fr(7)}){detail::foldProductInputs(inputs,r,folded);for(unsigned k=0;k<3;++k)for(std::size_t j=0;j<n/2;++j){auto a=f[k](2*j),b=f[k](2*j+1);need(folded[k][j]==a+r*(b-a),"C2 first fold mismatch");}}
 }
 w.resize(3);child.resize(6);bool reject=false;try{detail::productPolynomialContiguous(inputs);}catch(const std::exception&){reject=true;}need(reject,"C2 nonpower domain accepted");
 w.resize(4);child.resize(7);reject=false;try{detail::foldProductInputs(inputs,Fr(3),folded);}catch(const std::exception&){reject=true;}need(reject,"C2 child shape accepted");
 std::cout<<"PASS C2 literal coefficients/fold, signed/zero factors, shapes\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
