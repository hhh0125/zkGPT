// Post-validation public-data microbenchmark. Does not replace FS verification.
#include "classified_commitments.hpp"
#include "oneblock_plan.hpp"
#include "wire.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <algorithm>
using namespace zkgpt::r7;using namespace oneblock;
namespace {
Bytes read(std::istream&in,std::size_t n){need(n<=4u*1024*1024,"measurement frame bound");Bytes b(n);in.read(reinterpret_cast<char*>(b.data()),n);need(bool(in),"measurement truncated proof");return b;}
std::uint64_t number(std::istream&in,unsigned n){auto b=read(in,n);Reader r{b};return r.number(n);}
double seconds(std::chrono::steady_clock::time_point start){return std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();}
}
int main(int argc,char**argv){try{
 need(argc==5,"usage: compact_measure oneblock|full12 PROOF STATEMENT VERIFIED_ROOT_ID");r1::init();
 auto discovery=discover(std::string(argv[1])=="full12"?12:1);auto layout=classified::makeLayout(discovery.context);
 std::ifstream statement(argv[3],std::ios::binary);Bytes statement_bytes{std::istreambuf_iterator<char>(statement),{}};auto s=R7Wire::decodeStatement(statement_bytes);
 need(validateStatement(s,discovery.context).code==Code::Ok,"measurement statement context");auto plan=classified::singlePlan(layout,s.x);single::Public pc;pc.plan=plan;
 std::ifstream proof(argv[2],std::ios::binary);need(read(proof,8)==Bytes({'Z','K','R','7','F','S','1',0}),"measurement proof magic");read(proof,number(proof,4));bool header=false,query=false;std::vector<Fr>point;
 while(!query){auto tag=number(proof,1);need(tag=='P',"measurement unexpected event");number(proof,8);auto n=number(proof,8);need(n && n<=256,"measurement label bound");auto label_bytes=read(proof,n);std::string label(label_bytes.begin(),label_bytes.end());auto payload=read(proof,number(proof,8));
  if(label=="SingleHyrax/header"){need(payload==single::descriptor(plan),"measurement reconstructed descriptor");header=true;}
  if(label.rfind("SingleHyrax/rows/",0)==0){need(header,"measurement rows before descriptor");Reader r{payload};auto count=r.number();need(count<=256 && pc.rows.size()+count<=plan.rows,"measurement row chunk bounds");for(std::size_t i=0;i<count;++i)pc.rows.push_back(r.point());r.end();}
  if(label=="UnifiedInput/opening/C_U/binding"){Reader r{payload};need(r.text()==single::suite && r.text()==argv[4],"measurement root binding");r.text();r.text();r.text();r.text();r.text();r.point();for(unsigned j=0;j<single::bits(plan.domain);++j)point.push_back(r.scalar());need(r.number(8)==s.x.size(),"measurement public X length");r.end();query=true;}
 }
 need(pc.rows.size()==plan.rows && single::identity(pc)==argv[4],"measurement actual verified row identity");std::vector<double>identity_times,mapping_times;std::size_t terms=0;
 for(unsigned repeat=0;repeat<3;++repeat){auto start=std::chrono::steady_clock::now();need(single::identity(pc)==argv[4],"measurement identity instability");identity_times.push_back(seconds(start));terms=0;start=std::chrono::steady_clock::now();single::mappedQueryTerms(plan,point,[&](const single::QueryTerm&){++terms;});mapping_times.push_back(seconds(start));}
 std::sort(identity_times.begin(),identity_times.end());std::sort(mapping_times.begin(),mapping_times.end());
 std::cout<<"{\"measurement_kind\":\"post-validation-public-data-microbenchmark\",\"verified_root_id\":\""<<argv[4]<<"\",\"repeats\":3,\"identity_median_seconds\":"<<identity_times[1]<<",\"identity_min_seconds\":"<<identity_times[0]<<",\"mapping_generation_median_seconds\":"<<mapping_times[1]<<",\"mapping_generation_min_seconds\":"<<mapping_times[0]<<",\"rank_terms\":"<<terms<<",\"rows\":"<<plan.rows<<",\"columns\":"<<plan.columns<<",\"logical_domain\":"<<plan.domain<<",\"physical_domain\":"<<plan.physical_domain<<"}\n";return 0;
 }catch(const std::exception&e){std::cerr<<"MEASUREMENT_FAILED "<<e.what()<<'\n';return 1;}}
