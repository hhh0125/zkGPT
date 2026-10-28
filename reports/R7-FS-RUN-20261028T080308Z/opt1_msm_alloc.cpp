// Test-only allocation accounting, separate from the wall-time benchmark.
#include "paper_r7/oneblock_vectors.hpp"
#include "paper_r7/streaming_master.hpp"
#include <cstdlib>
#include <new>
#include <iostream>
static bool counting=false;static std::size_t allocations=0;
void* operator new(std::size_t n){if(counting)++allocations;if(void*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
using namespace zkgpt::r7;using namespace oneblock;
G1 legacy(const std::vector<G1>&g,const std::vector<Fr>&x){G1 out;out.clear();for(std::size_t at=0;at<g.size();at+=1024){auto n=std::min<std::size_t>(1024,g.size()-at);std::vector<G1>b(g.begin()+at,g.begin()+at+n);std::vector<Fr>v(x.begin()+at,x.begin()+at+n);G1 p;G1::mulVec(p,b.data(),v.data(),n);out+=p;}return out;}
int main(){try{
 r1::init();auto g=hyrax_a3::generators(1024);SessionRandom rng;std::vector<Fr>x(1024);for(auto&f:x)f=rng.scalar();
 auto expected=master3::masterMSM(g,x);need(legacy(g,x)==expected,"allocation reference differs");
 std::size_t observed[2]{};for(unsigned old=0;old<2;++old){allocations=0;counting=true;G1 last;for(unsigned i=0;i<32;++i)last=old?legacy(g,x):master3::masterMSM(g,x);counting=false;observed[old]=allocations;need(last==expected,"allocation test changed MSM");}
 need(observed[1]>=observed[0]+64,"MSM allocation reduction missing");
 std::cout<<"{\"calls\":32,\"terms_per_call\":1024,\"old_cpp_allocations\":"<<observed[1]<<",\"new_cpp_allocations\":"<<observed[0]<<",\"reusable_base_workspace_bytes\":"<<1024*sizeof(G1)<<",\"eliminated_scalar_copy_bytes_per_call\":"<<1024*sizeof(Fr)<<"}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
