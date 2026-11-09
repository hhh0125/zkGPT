#include "classified_commitments.hpp"
#include <iostream>
#include <map>
using namespace zkgpt::r7;using namespace oneblock;
namespace cl=classified;
std::size_t mapBytes(const cl::Layout&l){std::size_t n=l.base.capacity()*sizeof(cl::Coordinate)+l.base_runs.capacity()*sizeof(cl::Piece);for(const auto&[id,v]:l.views)n+=v.spans.capacity()*sizeof(r1::InputSpan)+v.pieces.capacity()*sizeof(cl::Piece);return n;}
int main(int argc,char**argv){try{
 r1::init();need(argc==2,"category census blocks");unsigned blocks=std::stoul(argv[1]);R7Context c;cl::Layout old,now;{master3::QuietStdout quiet;c=discover(blocks).context;old=cl::makeLayout(c);now=cl::makeCategoryLayout(c);}std::vector<Fr>x(32768,Fr(0));auto a=cl::singlePlan(old,x),b=cl::singlePlan(now,x);
 std::size_t rootpad=0;for(auto&r:old.roots)rootpad+=r.padded;
 // Verify each storage chunk has an exact interval partition, and native
 // val[0] coordinates have one location. Helper object padding stays stored.
 std::vector<std::vector<std::pair<std::size_t,std::size_t>>>coverage(now.roots.size());std::size_t master=0;
 for(const auto&r:now.base_runs){need(r.begin==master,"census native gap/overlap");master+=r.count;coverage.at(r.start.root).push_back({r.start.index,r.count});}
 need(master==c.plan.master.actual,"census native coverage");
 for(const auto&[id,v]:now.views)if(!v.mapped){std::size_t local=0;for(const auto&r:v.pieces){need(r.begin==local,"census helper local gap");local+=r.count;coverage.at(r.start.root).push_back({r.start.index,r.count});}need(local==v.padded,"census helper materialized padding lost");}
 for(std::size_t k=0;k<coverage.size();++k){auto&v=coverage[k];std::sort(v.begin(),v.end());std::size_t n=0;for(auto interval:v){need(interval.first==n,"census stored duplicate/gap");n+=interval.second;}need(n==now.roots[k].actual && now.roots[k].padded==n,"census storage tail padding");}
 std::size_t xi=0;for(auto&s:now.x_spans)for(std::size_t j=0;j<s.count;++j,++xi){auto at=cl::baseCoordinate(now,s.offset+j);const auto&r=now.roots.at(at.root);need(r.category==0 && r.category_offset+at.index==xi,"census X misplaced/duplicated");}
 std::cout<<"{\"blocks\":"<<blocks<<",\"native_master_actual\":"<<master<<",\"native_X_spans\":[";
 for(std::size_t j=0;j<now.x_spans.size();++j){if(j)std::cout<<',';std::cout<<"{\"offset\":"<<now.x_spans[j].offset<<",\"count\":"<<now.x_spans[j].count<<'}';}
 auto shape=[](const single::Plan&p){std::cout<<"\"logical_domain\":"<<p.domain<<",\"logical_values\":"<<p.logical_values<<",\"physical_values\":"<<p.physical_values<<",\"physical_domain\":"<<p.physical_domain<<",\"rows\":"<<p.rows<<",\"columns\":"<<p.columns<<",\"commitment_count\":1";};
 std::cout<<"],\"old\":{\"root_count\":"<<old.roots.size()<<",\"sum_root_padded\":"<<rootpad<<",\"source_map_owned_payload_bytes\":"<<mapBytes(old)<<',';shape(a);std::cout<<",\"category_capacities\":[";for(unsigned k=0;k<4;++k){if(k)std::cout<<',';std::cout<<a.packing.regions[k].capacity;}std::cout<<"]},\"new\":{\"category_count\":"<<b.segments.size()<<",\"storage_chunks\":"<<now.roots.size()<<",\"source_map_owned_payload_bytes\":"<<mapBytes(now)<<',';shape(b);
 std::cout<<",\"categories\":[";const char*names[]{"X","W","Aux","Query","Helper"};for(unsigned k=0;k<5;++k){if(k)std::cout<<',';auto z=now.categories[k];std::cout<<"{\"category\":\""<<names[k]<<"\",\"actual\":"<<z.actual<<",\"padded\":"<<z.padded<<",\"logical_offset\":"<<z.logical_offset<<",\"physical_offset\":"<<z.physical_offset<<",\"physical_length\":"<<z.actual<<'}';}
 std::cout<<"]},\"mapping_coverage\":\"PASS\",\"status\":\"PUBLIC_LAYOUT_CENSUS_NO_PROOF\"}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
