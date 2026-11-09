// Test-only deterministic entropy; never linked into production entry points.
#define main retained_perf5_lookup_main
#include "oneblock_lookup_tests.cpp"
#undef main
#include <fstream>
#include <sys/resource.h>
#include <chrono>
struct NativeFixedRandom:Random {
 std::uint64_t counter=0;
 void fill(std::uint8_t*p,std::size_t n)override{while(n){Writer w;w.text("perf4-native-test-only");w.number(counter++,8);auto d=r1::hash(w.b);auto count=std::min(n,d.size());std::copy_n(d.data(),count,p);p+=count;n-=count;}}
};
int main(int argc,char**argv){try{
 r1::init();need(argc==3,"usage: product_probe n output-message-artifact");auto n=std::stoull(argv[1]);need(n && n<=(1u<<23) && !(n&(n-1)),"bounded Product shape");
 NativeFixedRandom random;Tape p(true);auto begin=std::chrono::steady_clock::now();
 // Same native Memory leaf formula with table address/value and counter terms.
 // Public structured replay, not an assertion about private Full12 values.
 auto leaves=[](std::size_t j){auto address=j%512;return Fr(49)*Fr(address)+Fr(7)*Fr(3*address+1)+Fr(j/512+1)-Fr(11);};
 Fr product=1;for(std::size_t j=0;j<n;++j)product*=leaves(j);
 auto root=exchangeHidden(p,"perf5/product/root",product,&random);
 LeafAuthenticator auth=[n,leaves](IO&io,const Query&q,const Hidden*leaf,Random*r,std::string role){
  auto w=r1::weights(q.point);need(w.size()==n,"probe leaf dimension");Fr expected=0;for(std::size_t j=0;j<n;++j)expected+=w[j]*leaves(j);
  Hidden h;if(io.prover()){need(leaf,"probe leaf witness");h=*leaf;}else h.handle=q.evaluation;
  equalHidden(io,h,expected,r,role+"/public-fingerprint");
 };
 auto proveStart=std::chrono::steady_clock::now();hiddenProduct(p,n,root,leaves,auth,&random,"perf5/product");auto prove=std::chrono::duration<double>(std::chrono::steady_clock::now()-proveStart).count();
 Writer bytes;for(auto&m:p.messages){bytes.text(m.first);bytes.number(m.second.size(),8);bytes.raw(m.second);}
 std::ofstream out(argv[2],std::ios::binary);need(bool(out),"probe artifact open");out.write(reinterpret_cast<const char*>(bytes.b.data()),bytes.b.size());out.close();need(bool(out),"probe artifact write");
 Tape v(false);v.messages=p.messages;auto vr=exchangeHidden(v,"perf5/product/root",Fr(0),nullptr);auto verifyStart=std::chrono::steady_clock::now();hiddenProduct(v,n,vr,{},auth,nullptr,"perf5/product");need(v.pos==v.messages.size(),"probe strict message EOF");auto verify=std::chrono::duration<double>(std::chrono::steady_clock::now()-verifyStart).count();
 rusage usage{};getrusage(RUSAGE_SELF,&usage);std::cout.precision(12);std::cout<<"PERF5_PROBE {\"n\":"<<n<<",\"prover_seconds\":"<<prove<<",\"verifier_seconds\":"<<verify<<",\"wall_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()<<",\"proof_bytes\":"<<bytes.b.size()<<",\"proof_sha256\":\""<<hex(r1::hash(bytes.b))<<"\",\"peak_rss_kib\":"<<usage.ru_maxrss<<",\"independent_replay_accept\":true,\"public_structured_fingerprint_surrogate\":true}\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
