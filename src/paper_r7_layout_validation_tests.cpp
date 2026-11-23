#include "paper_r7_classified_commitments.hpp"
#include "paper_r7_fs_io.hpp"
#include "paper_r7_public_k.hpp"
#include <iostream>
#include <sstream>

namespace {
using namespace zkgpt::r7;
using namespace zkgpt::r7::crypto;
using namespace zkgpt::r7::oneblock;
namespace ui=unified;
namespace cl=classified;

template<class F> void rejects(F run,const std::string&reason){
 try{run();}catch(const std::exception&e){need(std::string(e.what()).find(reason)!=std::string::npos,e.what());return;}
 throw std::runtime_error("expected rejection: "+reason);
}
void guards(){
 auto packing=ui::pack({{2,8},{1,4},{2,2},{3,1}},2);
 std::ostringstream json;ui::writeLayout(json,packing);
 need(!ui::descriptor(packing).empty() && !json.str().empty(),"packing serialization");
 const ui::Factors unused=[](std::size_t,const ui::Consume&){throw std::runtime_error("invalid layout reached provider");};
 for(const auto&bad:std::vector<std::vector<ui::Segment>>{{{0,3}},{{1,2}},{{8,1}},{{0,4},{2,2}}}){
  constexpr auto invalid="unified unaligned/overlapping/out-of-domain segment";
  rejects([&]{ui::quadraticRound(8,bad,{},unused);},invalid);
  rejects([&]{ui::quadraticRoundFolded(8,bad,{},unused);},invalid);
  rejects([&]{ui::quadraticRoundStreaming(8,bad,{},unused,{},{});},invalid);
 }
 rejects([]{ui::descriptor(ui::Packing{});},"unified packing");
 auto plan=single::makePlan(packing,{2,7,4,2,1},std::string(64,'a'),{},{{1,0,0},{1,3,1}});
 auto rejectPlan=[&](auto mutate,const char*reason){auto bad=plan;mutate(bad);rejects([&]{single::decode({},bad);},reason);};
 rejectPlan([](auto&p){p.segments[0].offset=1;},"single source logical mapping mismatch");
 rejectPlan([](auto&p){p.segments[0].padded=4;},"single source logical mapping mismatch");
 rejectPlan([](auto&p){p.segments[0].actual=3;},"single native public X completeness");
 rejectPlan([](auto&p){p.runs[0].physical_offset++;},"single noncanonical direct source run");
 rejectPlan([](auto&p){p.public_cells[1].x_index=0;},"single native public coordinate");
 rejectPlan([](auto&p){p.segments[0].physical_length=1;},"single noncanonical logical/physical mapping");
 rejectPlan([](auto&p){p.packing=ui::Packing{};},"packing");
}

constexpr std::size_t domain=std::size_t(1)<<32;
void sparse(IO&io,Random*rng,const std::string&mode){
 // Sparse scalar Segments exercise all 32 rounds without materializing the virtual domain.
 std::vector<ui::Segment>segments;for(std::size_t k=0;k<8193;++k)segments.push_back({domain-1-k*524287,1});
 const std::array<Fr,2>values{Fr(3),Fr(0)},kernels{Fr(7),Fr(0)};
 auto initial=exchangeHidden(io,"initial",Fr(21),rng);
 std::unique_ptr<ui::PublicK>state;
 ui::Factors factors;ui::Inputs inputs;
 if(io.prover()){
  if(mode=="folded" || mode=="streaming")state=std::make_unique<ui::PublicK>(segments,ui::PublicK::Build([&](std::size_t k){return std::vector<Fr>{kernels[k!=0]};}),ui::KBudget{},io.session(),domain);
  if(mode=="streaming")inputs=[&](std::size_t k){return ui::InputSpan{&values[k!=0],1,1};};
  factors=[&](std::size_t k,const ui::Consume&consume){
   if(mode=="snapshot" && k==0)segments.clear();
   Value a=[&,k](std::size_t j){need(j==0,"U scalar bounds");return values[k!=0];};
   if(state)state->withRoot(k,io.session(),[&](const Value&b){consume(a,b);});
   else consume(a,[&,k](std::size_t j){need(j==0,"K scalar bounds");return kernels[k!=0];});
  };
 }
 const auto fixed=segments.front();
 auto endpoint=[&](const std::vector<Fr>&q){return ui::selector(fixed,q)*kernels[0];};
 auto result=ui::reduce(io,domain,segments,initial,factors,endpoint,rng,"sparse",state.get(),inputs);
 need(result.point.size()==32,"expected 32 Sumcheck rounds");
}

cl::Layout classifiedLayout(bool category){
 cl::Layout layout;layout.native="packing-regression";layout.category_mode=category;
 if(category){
  layout.categories[0]={1,1,0,0};layout.categories[1]={1,1,1,1};
  layout.roots={{"X/test",1,1,true,false,0,0},{"W/test",1,1,true,false,1,0}};
  layout.base_runs={{0,1,{0,0}},{1,1,{1,0}}};
  layout.master_actual=2;layout.x_spans={{0,1,Fr(0)}};
 }else{
  layout.roots={{"C_aux/test",2,2,true,false}};
  layout.base={{0,0},{0,1}};
 }
 return layout;
}
void classifiedCase(IO&io,Random*rng,const cl::Layout&layout,const std::vector<Fr>&x,bool single_pc){
 const std::vector<Fr>values{Fr(3),Fr(7)};
 cl::Prepared prepared;Value base;
 if(io.prover())base=[&](std::size_t i){return values.at(i);};
 cl::Commitments commitments(io,layout,base,io.prover()?&prepared:nullptr,rng,nullptr,nullptr,nullptr,nullptr,single_pc?&x:nullptr);
 commitments.constrainBaseTails();
 auto constant=exchangeHidden(io,"claim",Fr(7),rng);
 commitments.deferBase("claim",1,1,[](std::size_t j){need(j==0,"test claim bounds");return 1;},constant,{});
 commitments.authenticateUnifiedInput(x);
 need(commitments.authenticated(),"classified authentication");
}
}

int main(int argc,char**argv){try{
 zkgpt::r1::init();
 if(argc==2 && std::string(argv[1])=="guards"){guards();return 0;}
 need(argc==4,"prove|verify plain|folded|streaming|snapshot|legacy|legacy-roots|category PATH");
 const std::string action=argv[1],mode=argv[2];
 const bool p=action=="prove",classified=mode=="legacy" || mode=="legacy-roots" || mode=="category";
 need(p || action=="verify","test action");
 need(classified || mode=="plain" || mode=="folded" || mode=="streaming" || mode=="snapshot","test mode");
 const auto layout=classifiedLayout(mode=="category");const std::vector<Fr>x{Fr(3)};
 crypto::Writer instance;
 if(classified){
  instance.text(cl::unifiedInputIdentity(layout,x));
  instance.raw(single::descriptor(cl::singlePlan(layout,x)));
  if(!layout.category_mode)instance.raw(ui::descriptor(ui::pack({{2,2}},1)));
 }else instance.text("sparse-32-rounds");
 fs::Binding binding{"Unit","r7-layout-validation",fs::generatorIdentity(),instance.b};
 OSRandom rng;std::unique_ptr<fs::FsIO>io;
 if(p)io=std::make_unique<fs::FsProverIO>(argv[3],binding);
 else io=std::make_unique<fs::FsVerifierIO>(argv[3],binding);
 std::cout<<"parameter_id="<<binding.parameter_id<<" public_instance=";
 for(auto byte:binding.public_instance){constexpr auto digits="0123456789abcdef";std::cout<<digits[byte>>4]<<digits[byte&15];}
 std::cout<<'\n';
 if(classified)classifiedCase(*io,p?&rng:nullptr,layout,x,mode!="legacy-roots");
 else sparse(*io,p?&rng:nullptr,mode);
 io->finish();
 std::cout<<"PASS "<<action<<" "<<mode<<" "<<hex(zkgpt::r1::hash(instance.b))<<" "<<io->metadataJson()<<'\n';
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
