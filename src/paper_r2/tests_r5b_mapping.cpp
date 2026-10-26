#include "r5b_trust.hpp"
#include "models.hpp"
#include "paper_r1/wire.hpp"
#include <filesystem>
#include <chrono>
#include <iostream>
#include <limits>
#include <cstring>
namespace r1=zkgpt::r1;using namespace zkgpt;
void integer(r1::Bytes&b,std::int32_t n){std::uint32_t v;std::memcpy(&v,&n,4);for(unsigned i=0;i<4;++i)b.push_back(std::uint8_t(v>>(8*i)));}
int main(int argc,char**argv){try{r1::init();r1::require(argc>=2,"mapping test mode");if(std::string(argv[1])=="small"){
 const auto root=std::filesystem::path("r5b-mapping-unit-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directory(root);
 r1::Bytes matrix,bias,ln;integer(matrix,2);integer(matrix,3);for(std::int32_t x:{-1,std::numeric_limits<std::int32_t>::min(),0,std::numeric_limits<std::int32_t>::max(),7,-9})integer(matrix,x);integer(bias,2);integer(bias,-5);integer(bias,8);for(auto x:{768,1,-10,1,-10})integer(ln,x);for(int j=0;j<768;++j)integer(ln,j%11-5);for(int j=0;j<768;++j)integer(ln,j%7-3);
 r2::ModelQueryPlan p;p.sources.push_back({r1::PCSpec::make("test/W",2068,4096,r1::Layout::ColumnMajor),{}});
 std::vector<neuralNetwork::ParameterRegion>regions={{"fc_0.weight",0,16,2,3},{"fc_0.bias",16,4,2,1},{"ln_0.weight",20,1024,768,1},{"ln_0.bias",1044,1024,768,1}};
 auto run=[&](int variant){auto dir=root/("case-"+std::to_string(variant));std::filesystem::create_directory(dir);auto a=matrix;if(variant==2)a[0]=1;if(variant==3)a.pop_back();if(variant==4)a.push_back(0);r5b::ModelManifest m;m.exporter=r5b::digestHex(r1::readFile("src/export_gpt2_complete_params.py"));
  for(const auto&entry:std::vector<std::pair<std::string,r1::Bytes>>{{"fc_0.bin",a},{"fc_0_bias.bin",bias},{"ln_0.bin",ln}}){r1::writeFile((dir/entry.first).string(),entry.second);m.parameters.push_back({entry.first,r5b::digestHex(entry.second)});}if(variant==1)m.parameters[0].second=std::string(64,'0');return r5b::preprocessWeights(p,regions,m,dir.string());};
 auto w=run(0);std::vector<Fr>expected(4096,Fr(0));expected[0]=-1;expected[1]=std::numeric_limits<std::int32_t>::min();expected[4]=std::numeric_limits<std::int32_t>::max();expected[5]=7;expected[6]=-9;expected[16]=-5;expected[17]=8;for(int j=0;j<768;++j){expected[20+j]=j%11-5;expected[1044+j]=j%7-3;}r1::require(w==expected,"W signed int32, transpose, region order or padding mismatch");r1::require(r1::commit(p.sources.back().spec,w).rows==r1::commit(p.sources.back().spec,expected).rows,"deterministic W PC mismatch");
 for(int variant=1;variant<=4;++variant){bool rejected=false;try{run(variant);}catch(const std::exception&){rejected=true;}r1::require(rejected,"malformed or unpinned parameters accepted");}
 std::cout<<"PASS preprocessing signed-int32 extremes, output-major transpose, zero padding, LN halves, region order, deterministic PC; changed SHA, header, truncation and trailing bytes rejected\n";
 }else if(std::string(argv[1])=="real"){
 r1::require(argc==6,"real MANIFEST PARAMETER_DIR ANCHOR INDEPENDENT_SHA");setenv("ZKGPT_PARAMETER_DIR",argv[3],1);LLM nn(1,12,64,768,3072);nn.setR5BEngineeringCandidate();prover pr;nn.create(pr,false);nn.validateCurrentWitness(pr);auto plan=r2::planModelQueries(pr.C,nn.getWitnessRegistry(),nn.parameterRegions(),r1::ExpProfile::LegacyClampOne);auto manifest=r5b::parseManifest(r1::readFile(argv[2]));auto expected=r5b::preprocessWeights(plan,nn.parameterRegions(),manifest,argv[3]);std::size_t at=0,negative=0,zero=0;
 for(const auto &span:plan.sources.back().spans)for(std::size_t j=0;j<span.count;++j){const auto &v=pr.val[0][span.offset+j];r1::require(expected[at]==v,"real preprocessing/gather byte mismatch at "+std::to_string(at));negative+=v.isNegative();zero+=v.isZero();++at;}r1::require(at==plan.sources.back().spec.actual,"real complete W scalar coverage");for(std::size_t j=at;j<expected.size();++j)r1::require(expected[j].isZero(),"real W tail not zero");auto cw=r1::commit(plan.sources.back().spec,expected);auto anchor=r5b::loadAnchor(argv[4],argv[5],argv[2],plan);r5b::requireAnchorW(anchor,cw);r1::require(negative>0 && zero>0,"real negative/padding coverage missing");std::cout<<"PASS real W preprocessing==production witness for "<<at<<" scalar encodings; negative="<<negative<<" zero="<<zero<<" tail="<<expected.size()-at<<"; deterministic PC matches independent pinned anchor\n";
 }else throw std::invalid_argument("mapping test mode");return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
