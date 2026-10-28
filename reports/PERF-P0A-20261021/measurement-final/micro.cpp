#include "pc.hpp"
#include "p0a_audit.hpp"
#include <ctime>
#include <iostream>
using namespace zkgpt::r1;
int main(){try{init();std::vector<G1> points;std::vector<Bytes> encoded;for(unsigned i=0;i<64;++i){G1 p;mcl::bn::hashAndMapToG1(p,"P0A/micro/"+std::to_string(i));points.push_back(p);encoded.push_back(encode(p));}auto spec=PCSpec::make("p0a/micro",4096,4096,Layout::RowMajor);std::vector<Fr>w(4096);for(size_t i=0;i<w.size();++i)w[i]=Fr(i%127);auto c=commit(spec,w);p0a_count::install();
auto measure=[&](const char *name,auto f){auto t=std::chrono::steady_clock::now();auto cpu=std::clock();auto before=p0a_count::counts().actual.load();f();std::cout<<"MICRO "<<name<<" wall="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-t).count()<<" cpu="<<double(std::clock()-cpu)/CLOCKS_PER_SEC<<" order_checks="<<p0a_count::counts().actual.load()-before<<'\n';};
volatile size_t used=0;measure("encode_2000",[&]{for(unsigned i=0;i<2000;++i)used+=encode(points[i%64]).size();});measure("decode_2000",[&]{for(unsigned i=0;i<2000;++i)used+=decodeG1(encoded[i%64]).isZero();});measure("shape_and_append_200x64rows",[&]{for(unsigned i=0;i<200;++i){Fs t("P0A/micro");append(t,c);used+=t.digest()[0];}});std::cout<<"PASS micro checksum="<<used<<'\n';return 0;}catch(const std::exception&e){std::cerr<<"FAIL micro "<<e.what()<<'\n';return 1;}}
