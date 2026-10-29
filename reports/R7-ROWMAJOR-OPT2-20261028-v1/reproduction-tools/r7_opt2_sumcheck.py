from pathlib import Path
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd().resolve()==R
P=R/'src/paper_r7'
p=P/'oneblock_sumcheck.hpp';s=p.read_text();needle='Reduction expressionSumcheck(';s=s.replace(needle,'''namespace detail {
// Pure local arithmetic, no transcript, RNG, handles, or challenge operations.
std::vector<Fr>expressionPolynomial(const Expression&,std::size_t,const std::vector<Value>&,const std::vector<std::vector<Fr>>&,bool first);
}
'''+needle);p.write_text(s)
p=P/'oneblock_sumcheck.cpp';s=p.read_text().replace('#include <algorithm>','#include <algorithm>\n#include <array>')
a=s.index('Fr expression(const Expression&');b=s.index('\nHidden alias',a);s=s[:a]+s[b:]
a=s.index('Reduction expressionSumcheck(')
helper=r'''namespace detail {
std::vector<Fr>expressionPolynomial(const Expression&e,std::size_t current,const std::vector<Value>&factors,const std::vector<std::vector<Fr>>&folded,bool first){
 unsigned degree=0;for(auto&t:e.terms)degree=std::max(degree,unsigned(t.factors.size()));
 std::vector<Fr>coeff(degree+1,Fr(0));std::array<Fr,8>a,d;
 for(std::size_t j=0;j<current/2;++j){
  for(std::size_t k=0;k<e.factor_count;++k){a[k]=first?factors[k](2*j):folded[k][2*j];d[k]=(first?factors[k](2*j+1):folded[k][2*j+1])-a[k];}
  for(auto&t:e.terms){auto n=t.factors.size();const auto&c=t.coefficient;
   if(!n){coeff[0]+=c;continue;}
   auto i=t.factors[0];if(n==1){coeff[0]+=c*a[i];coeff[1]+=c*d[i];continue;}
   auto k=t.factors[1];auto ab0=a[i]*a[k],ab1=a[i]*d[k]+d[i]*a[k],ab2=d[i]*d[k];
   if(n==2){coeff[0]+=c*ab0;coeff[1]+=c*ab1;coeff[2]+=c*ab2;continue;}
   auto l=t.factors[2];coeff[0]+=c*(ab0*a[l]);coeff[1]+=c*(ab0*d[l]+ab1*a[l]);coeff[2]+=c*(ab1*d[l]+ab2*a[l]);coeff[3]+=c*(ab2*d[l]);
  }
 }
 return coeff;
}
}
'''
s=s[:a]+helper+s[a:]
a=s.index('   std::vector<Fr>evaluations(');b=s.index('\n  }',a);s=s[:a]+'   coeff=detail::expressionPolynomial(e,current,factors,folded,!round);'+s[b:];p.write_text(s)
p=P/'opt2_sumcheck_tests.cpp';s=p.read_text().replace('std::vector<std::vector<Fr>>folded=e.factor_count?input:input;','std::vector<std::vector<Fr>>folded=input;');p.write_text(s)
print('DIRECT DEGREE 0..3 COEFFICIENT ARITHMETIC WRITTEN; IO/RNG/FOLDING UNCHANGED')
