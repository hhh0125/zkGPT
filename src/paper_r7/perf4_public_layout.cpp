#include "classified_commitments.hpp"
#include "oneblock_public.hpp"
#include "paper_r2/r6_model.hpp"
#include "wire.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
using namespace zkgpt::r7;using namespace oneblock;
namespace cl=zkgpt::r7::oneblock::classified;
struct Run {std::size_t begin,count;cl::Coordinate at;Fr bias;bool constant=false;};
struct BiasCount {Fr previous;bool have=false;std::uint64_t serialized=0,reused=0;
 void add(const Fr&b,std::size_t count){if(!count)return;bool same=have && previous==b;serialized+=!same;reused+=count-!same;previous=b;have=true;}
};
int main(int argc,char**argv){try{
 need(argc==2,"PERF4 public census STATEMENT");r1::init();auto discovery=discover(12);auto p=cl::makeLayout(discovery.context);
 std::ifstream in(argv[1],std::ios::binary);Bytes bytes{std::istreambuf_iterator<char>(in),{}};auto statement=R7Wire::decodeStatement(bytes);need(validateStatement(statement,discovery.context).code==Code::Ok,"PERF4 trusted statement");auto plan=cl::singlePlan(p,statement.x);
 std::vector<std::vector<single::PublicCell>>cells(p.roots.size());for(auto c:plan.public_cells)cells.at(c.source-1).push_back(c);
 std::cout<<"{\"kind\":\"layout\",\"identity\":"<<std::quoted(cl::identity(p))<<",\"roots\":"<<p.roots.size()<<",\"views\":"<<p.views.size()<<",\"base_runs\":"<<p.base_runs.size()<<",\"logical_domain\":"<<plan.domain<<",\"physical_domain\":"<<plan.physical_domain<<",\"private_runs\":"<<plan.runs.size()<<"}\n";
 auto countBias=[&](BiasCount&count,const Run&r){
  if(r.constant){count.add(r.bias,r.count);return;}
  auto&list=cells.at(r.at.root);auto it=std::lower_bound(list.begin(),list.end(),r.at.index,[](const auto&c,std::size_t j){return c.index<j;});std::size_t next=r.at.index,end=next+r.count;
  for(;it!=list.end() && it->index<end;++it){count.add(r.bias,it->index-next);count.add(r.bias+statement.x.at(it->x_index),1);next=it->index+1;}count.add(r.bias,end-next);
 };
 for(const auto&[id,v]:p.views){std::vector<Run>runs;
  if(!v.mapped){if(v.pieces.empty())runs.push_back({0,v.padded,v.start,Fr(0)});else for(auto piece:v.pieces)runs.push_back({piece.begin,piece.count,piece.start,Fr(0)});}
  else{std::size_t begin=0;for(auto span:v.spans){if(begin>=v.actual)break;auto count=std::min(span.count,v.actual-begin);auto at=span.offset,end=at+count;
    while(at<end){auto c=cl::baseCoordinate(p,at);auto it=std::upper_bound(p.base_runs.begin(),p.base_runs.end(),at,[](std::size_t j,const auto&r){return j<r.begin;});need(it!=p.base_runs.begin(),"PERF4 source gap");--it;auto n=std::min(end-at,it->count-(at-it->begin));runs.push_back({begin+(at-span.offset),n,c,span.bias});at+=n;}
    begin+=count;
   }need(begin==v.actual,"PERF4 source coverage");if(v.actual<v.padded)runs.push_back({v.actual,v.padded-v.actual,{},Fr(0),true});
  }
  BiasCount count;for(const auto&r:runs)countBias(count,r);
  std::cout<<"{\"kind\":\"view\",\"id\":"<<std::quoted(id)<<",\"actual\":"<<v.actual<<",\"padded\":"<<v.padded<<",\"mapped\":"<<(v.mapped?"true":"false")<<",\"spans\":"<<v.spans.size()<<",\"pieces\":"<<v.pieces.size()<<",\"A_serializations\":"<<count.serialized<<",\"A_reuses\":"<<count.reused<<",\"runs\":[";bool first=true;
  for(const auto&r:runs){if(!first)std::cout<<',';first=false;std::cout<<"["<<r.begin<<','<<r.count<<','<<r.at.root<<','<<r.at.index<<','<<(r.constant?1:0)<<','<<std::quoted(r.bias.getStr())<<"]";}std::cout<<"]}\n";
 }
 layeredCircuit graph;{master3::QuietStdout quiet;LLM nn(12,12,64,768,3072);zkgpt::r6::configure(nn);graph=nn.buildPublicCircuit();}
 for(std::size_t layer=1;layer<graph.circuit.size();++layer)for(unsigned port=0;port<2;++port){const auto&l=graph.circuit[layer];const auto&ids=port?l.ori_id_v:l.ori_id_u;auto width=port?l.bit_length_v[0]:l.bit_length_u[0];if(width<0)continue;std::size_t padded=std::size_t(1)<<width;BiasCount count;std::uint64_t positive=0,negative=0,zero=0,changes=0;cl::Coordinate previous{};bool have=false;
  for(auto index:ids){auto c=cl::baseCoordinate(p,index);Fr bias=0;auto&list=cells.at(c.root);auto found=std::lower_bound(list.begin(),list.end(),c.index,[](const auto&x,std::size_t j){return x.index<j;});if(found!=list.end() && found->index==c.index)bias=statement.x.at(found->x_index);count.add(bias,1);
   if(have && previous.root==c.root){auto stride=std::int64_t(c.index)-previous.index;positive+=stride>0;negative+=stride<0;zero+=stride==0;}else ++changes;previous=c;have=true;
  }need(ids.size()<=padded,"PERF4 native padding");count.add(Fr(0),padded-ids.size());
  std::cout<<"{\"kind\":\"native\",\"layer\":"<<layer<<",\"port\":"<<port<<",\"actual\":"<<ids.size()<<",\"padded\":"<<padded<<",\"positive_steps\":"<<positive<<",\"negative_steps\":"<<negative<<",\"zero_steps\":"<<zero<<",\"root_changes\":"<<changes<<",\"A_serializations\":"<<count.serialized<<",\"A_reuses\":"<<count.reused<<"}\n";
 }
 std::cout<<"{\"kind\":\"complete\",\"public_only\":true,\"witness_generated\":false}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
