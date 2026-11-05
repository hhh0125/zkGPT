#include "crypto.hpp"
#include <chrono>
#include <iostream>
using namespace zkgpt::r7;
static std::size_t calls=0;
static decltype(G1::isValidOrderFast) prior=nullptr;
static bool countOrder(const G1&p){++calls;if(prior)return prior(p);G1 q;G1::mulGeneric(q,p,G1::order_);return q.isZero();}
template<class F>void timeIt(const char*name,F fn){auto t=std::chrono::steady_clock::now();auto c=crypto::processCPU();fn();std::cout<<"{\"operation\":\""<<name<<"\",\"points\":1024,\"wall_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-t).count()<<",\"cpu_seconds\":"<<crypto::processCPU()-c<<"}\n";}
int main(){try{r1::init();auto H=crypto::parameters(1).hiding;std::vector<G1>points;for(unsigned i=0;i<1024;++i)points.push_back(H*Fr(i+1));
 std::vector<Bytes>encoded;
 timeIt("R7-Writer-point",[&]{for(auto&p:points){crypto::Writer w;w.point(p);encoded.push_back(w.b);}});
 timeIt("R1-encode-point",[&]{for(std::size_t i=0;i<points.size();++i)crypto::need(r1::encode(points[i])==encoded[i],"encoder bytes differ");});
 timeIt("R7-Reader-point",[&]{for(std::size_t i=0;i<points.size();++i){crypto::Reader r{encoded[i]};crypto::need(r.point()==points[i],"R7 decoder differs");r.end();}});
 timeIt("R1-decode-point",[&]{for(std::size_t i=0;i<points.size();++i)crypto::need(r1::decodeG1(encoded[i])==points[i],"R1 decoder differs");});
 prior=G1::isValidOrderFast;G1::isValidOrderFast=countOrder;
 calls=0;crypto::Writer w;w.point(points[0]);auto writer=calls;
 calls=0;crypto::Reader r{w.b};r.point();auto reader=calls;
 calls=0;r1::encode(points[0]);auto enc=calls;
 calls=0;r1::decodeG1(w.b);auto dec=calls;
 G1::isValidOrderFast=prior;
 std::cout<<"{\"operation\":\"order-callback-count\",\"R7_Writer\":"<<writer<<",\"R7_Reader\":"<<reader<<",\"R1_encode\":"<<enc<<",\"R1_decode\":"<<dec<<",\"bytes_and_points_matched\":true}\n";
 return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
