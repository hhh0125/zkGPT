#include "mapped_input.hpp"
#include <iostream>
using namespace zkgpt::r1;
int main(){try{
 init();auto n=std::size_t(1)<<28;
 auto root=PCSpec::make("r2/sparse/master",n,n,Layout::RowMajor);
 auto child=PCSpec::make("r2/sparse/child",n/2,n/2,Layout::ColumnMajor);
 G1 zero;zero.clear();
 MappedInputSpec s{"zero-oracle-proof-with-odd-shift",{root,std::vector<G1>(root.rows,zero)},
  {{{child,std::vector<G1>(child.rows,zero)},{{1,n/2,Fr(0)}}}}, {}};
 // Zero polynomial has identity commitments and zero IPA messages, at every
 // challenge. This valid proof needs no 8GiB witness allocation to construct.
 auto opening=[&](const PCSpec &pc){OpenProof p;p.rounds.assign(log2exact(pc.columns),{zero,zero});return p;};
 MappedInputProof p;p.children={opening(child)};p.master=opening(root);
 p.sumcheck.rounds.assign(28,std::vector<Fr>(3,Fr(0)));
 Fs t("sparse-zero");verifyMappedInput(s,p,t);
 CheckedInputClaim checked{std::vector<Fr>(27,Fr(3)),Fr(0),{{1,n/2,Fr(0)}}};
 Fs tc("sparse-zero-checked");verifyMappedInput(s,p,tc,{checked});
 std::cout<<"PASS 2^28 master / 2^27 odd-offset mapped child verified without dense tables\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
