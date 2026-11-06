#define main inherited_perf2_fixture_main
#include "opt3a_kernel_tests.cpp"
#undef main
#include "k_terminal.hpp"
Fr literalFold(std::vector<Fr>v,const std::vector<Fr>&q){for(auto r:q){for(std::size_t j=0;j<v.size()/2;++j)v[j]=v[2*j]*(Fr(1)-r)+v[2*j+1]*r;v.resize(v.size()/2);}return v.at(0);}
void blocks(){
 for(unsigned da:{0u,1u,3u,6u})for(unsigned dr:{0u,2u,5u})for(unsigned variant=0;variant<3;++variant){
  std::vector<Fr>a(da),r(dr);for(unsigned t=0;t<da;++t)a[t]=variant<2?Fr(variant):Fr(-3-int(t));for(unsigned t=0;t<dr;++t)r[t]=variant<2?Fr(1-variant):Fr(5+t);
  TerminalBasis b(a,r);auto A=r1::weights(a),R=r1::weights(r);
  for(unsigned bits=0;bits<=da;++bits)for(std::size_t j=0;j<A.size();j+=std::size_t(1)<<bits)for(std::size_t z=0;z<R.size();++z){
   Fr repeated=0;for(std::size_t x=0;x<(std::size_t(1)<<bits);++x)repeated+=A[j+x]*R[z];need(b.block(j,z,bits,true)==repeated,"PERF2 repeated multiplicity block");
   if(bits<=dr && !(z&((std::size_t(1)<<bits)-1))){Fr want=0;for(std::size_t x=0;x<(std::size_t(1)<<bits);++x)want+=A[j+x]*R[z+x];need(b.block(j,z,bits,false)==want,"PERF2 bilinear dyadic block");}
  }
 }
 std::cout<<"PERF2_DYADIC_BLOCKS_ZERO_ONE_DIFFERENT_DIMENSIONS_PASS\n";
}
void kernels(){
 for(std::size_t n:{64u,128u,1024u,4096u})for(unsigned pattern=0;pattern<5;++pattern){Fixture f;f.setup(n,6,pattern);auto&c=*f.pcs;Access::Auth a(4);for(unsigned i=0;i<4;++i)Access::prepare(c,a,i);
  for(unsigned lane=0;lane<5;++lane){std::vector<Fr>rho(4,Fr(0));if(lane<4)rho[lane]=1;else rho={Fr(3),Fr(-5),Fr(0),Fr(11)};
   for(unsigned k=0;k<6;++k){auto K=Access::kernel(c,k,rho,a);
    for(unsigned variant=0;variant<3;++variant){std::vector<Fr>q(r1::log2exact(n),variant<2?Fr(variant):Fr(-3));need(Access::endpoint(c,k,rho,a,q)==literalFold(K,q),"PERF2 K_i/aggregate nonboolean endpoint");}
    // Boolean endpoint queries independently recover EVERY physical source K
    // coefficient, including multiplicity, affine bias omission and zero tail.
    if(n<=128)for(std::size_t z=0;z<n;++z){std::vector<Fr>q;for(unsigned t=0;t<r1::log2exact(n);++t)q.push_back(Fr((z>>t)&1));need(Access::endpoint(c,k,rho,a,q)==K[z],"PERF2 per-coordinate K_i/aggregate mismatch");}
   }
  }
 }
 // Check each intermediate folded coordinate, not only the final scalar.
 Fixture f;f.setup(64,1,0);auto&c=*f.pcs;Access::Auth a(4);for(unsigned i=0;i<4;++i)Access::prepare(c,a,i);std::vector<Fr>rho{Fr(1),Fr(0),Fr(-2),Fr(3)},prefix;auto v=Access::kernel(c,0,rho,a);
 for(unsigned t=0;t<=6;++t){for(std::size_t z=0;z<v.size();++z){auto q=prefix;for(unsigned b=t;b<6;++b)q.push_back(Fr((z>>(b-t))&1));need(Access::endpoint(c,0,rho,a,q)==v[z],"PERF2 intermediate K fold coordinate");}if(t<6){Fr r=t%2?Fr(0):Fr(2);for(std::size_t j=0;j<v.size()/2;++j)v[j]=v[2*j]*(Fr(1)-r)+v[2*j+1]*r;v.resize(v.size()/2);prefix.push_back(r);}}
 // Exhausted mapping budgets retain callback digest validation, not a new trust path.
 Fixture g;g.setup(1024,6,4);Access::Auth fallback(4);fallback.map_bytes=fallback.map_budget;for(unsigned i=0;i<4;++i)Access::prepare(*g.pcs,fallback,i);std::vector<Fr>q(10,Fr(3));auto K=Access::kernel(*g.pcs,0,rho,fallback);need(Access::endpoint(*g.pcs,0,rho,fallback,q)==literalFold(K,q),"PERF2 unfrozen fallback");
 Access::claims(*g.pcs)[0].map=[](std::size_t)->Located{return {{0,0},Fr(9),false};};bool bad=false;try{Access::endpoint(*g.pcs,0,rho,fallback,q);}catch(const std::exception&e){bad=std::string(e.what()).find("changed after defer")!=std::string::npos;}need(bad,"PERF2 mutable fallback map accepted");
 std::cout<<"PERF2_KI_AGGREGATE_EVERY_FOLD_AND_FALLBACK_PASS\n";
}
int main(){try{
 r1::init();Fixture f;f.setup(1024,1,0);auto&c=*f.pcs;Access::Auth a(4);for(unsigned i=0;i<4;++i)Access::prepare(c,a,i);
 std::vector<Fr>rho{Fr(3),Fr(-5),Fr(7),Fr(11)},q(10,Fr(2));auto K=Access::kernel(c,0,rho,a);
 unsetenv("ZKGPT_PERF2_K_REFERENCE");
 need(Access::endpoint(c,0,rho,a,q)==literalFold(K,q) && Access::metrics(c).endpoint_fast_coordinates==0,"PERF2 experimental candidate enabled by default");
 setenv("ZKGPT_PERF2_K_REFERENCE","0",1);
 need(Access::endpoint(c,0,rho,a,q)==literalFold(K,q),"PERF2 K endpoint differs");
 need(Access::metrics(c).endpoint_fast_coordinates>512,"PERF2 contiguous terminal still expands all K coefficients");
 std::cout<<"PERF2_K_CONTRACT_PASS\n";blocks();kernels();return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
