#include "paper_r7_classified_commitments.hpp"
#include "paper_r7_fs_io.hpp"
#include "paper_r7_fs_test_fixture.hpp"
#include "paper_r7_public_k.hpp"
#include "paper_r7_u_fold_state.hpp"
#include "paper_r7_unified_workers.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <random>
#include <iostream>
#include <sstream>
#include <cstdlib>

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

void productDifferential(){
 const std::vector<ui::Segment> segments{{0,8},{8,2},{11,1},{14,2}};
 std::vector<Fr> a(16), b(16);
 for(std::size_t j=0;j<a.size();++j){a[j]=Fr(j*j+1);b[j]=Fr(j*7+3);}
 const ui::Factors factors=[&](std::size_t k,const ui::Consume& consume){
  const auto offset=segments.at(k).offset;
  consume([&](std::size_t j){return a.at(offset+j);},
          [&](std::size_t j){return b.at(offset+j);});
 };
 const ui::Inputs inputs=[&](std::size_t k){
  const auto seg=segments.at(k);
  return ui::InputSpan{a.data()+seg.offset,seg.padded,seg.padded};
 };
 setenv("ZKGPT_UNIFIED_OPT","1",1);
 for(const auto challenge:{Fr(0),Fr(1),Fr(37)}){
  std::vector<Fr> prefix;
  for(unsigned t=0;t<4;++t){
   ui::UMetrics reference_metrics, metrics;
   auto reference=ui::quadraticRoundStreaming(16,segments,prefix,factors,inputs,{},&reference_metrics);
   auto candidate=ui::quadraticRoundStreaming(16,segments,prefix,factors,inputs,ui::UOptions::environment(),&metrics);
   need(candidate.coefficients==reference.coefficients && candidate.input_pair==reference.input_pair,"U1 product differential");
   need(metrics.product_multiplications && metrics.product_multiplications*4==reference_metrics.product_multiplications*3,"U1 multiplication count");
   prefix.push_back(challenge);
  }
 }
}

void streamingDifferential(){
 constexpr std::size_t n=16384;
 const std::vector<ui::Segment> segments{{0,8192},{8192,16},{8225,1},{12288,1024}};
 std::vector<std::vector<Fr>> original_u,original_k;
 for(const auto&seg:segments){
  std::vector<Fr> u(seg.padded), k(seg.padded);
  for(std::size_t j=0;j<seg.padded;++j){u[j]=Fr((seg.offset+j)*13+j*j+1);k[j]=Fr((seg.offset+j)*7+3);}
  original_u.push_back(std::move(u));original_k.push_back(std::move(k));
 }
 for(unsigned mask:{0u,1u,3u,7u,135u}){
  const ui::Inputs inputs=[&](std::size_t k){
   const auto&v=original_u.at(k);
   if(k==1){
    ui::InputSpan span{nullptr,v.size(),v.size(),[&v](std::size_t j){return v.at(j);},&v};
    span.window=[&v](std::size_t first,std::size_t count){
     const auto boundary=first<7?7:v.size();
     return ui::InputWindow{v.data()+first,std::min(count,boundary-first)};
    };
    return span;
   }
   if(k==segments.size()-1)return ui::InputSpan{nullptr,v.size(),v.size(),[&v](std::size_t j){return v.at(j);},&v};
   return ui::InputSpan{v.data(),v.size(),v.size()};
  };
  ui::PublicK state(segments,[&](std::size_t k,Fr*out,std::size_t count){
   std::copy_n(original_k.at(k).begin(),count,out);
  },{},"stream",n);
  ui::FoldedU folded(n,segments,inputs,"stream");
  ui::detail::Workers workers(4);
  auto u=original_u,k=original_k;
  const ui::Factors provider=[&](std::size_t root,const ui::Consume& consume){
   state.withRoot(root,"stream",[&](const Value&b){consume([&](std::size_t j){return original_u.at(root).at(j);},b);});
  };
  const ui::Factors reference=[&](std::size_t root,const ui::Consume& consume){
   consume([&](std::size_t j){return original_u.at(root).at(j);},
           [&](std::size_t j){return original_k.at(root).at(j);});
  };
  std::vector<Fr> prefix;
  Fr previous=0;
  for(std::size_t root=0;root<u.size();++root)for(std::size_t j=0;j<u[root].size();++j)previous+=u[root][j]*k[root][j];
  ui::Round last;
  for(unsigned t=0;t<14;++t){
   ui::UOptions options;options.threads=4;options.tile_pairs=2048;options.optimization=mask;
   if(!(mask&ui::U3))options.observe=[&](std::size_t root,std::size_t first,const Fr* values,std::size_t count){
    need(std::equal(values,values+count,u.at(root).begin()+first),"round U state differential");
   };
   ui::UMetrics metrics;
   const auto expected=ui::quadraticRound(n,segments,prefix,reference);
   last=ui::quadraticRoundStreaming(n,segments,prefix,provider,inputs,options,&metrics,&folded,"stream",&state,&workers);
   need(last.coefficients==expected.coefficients && last.input_pair==expected.input_pair,"retained round differs from independent oracle");
   const auto&c=last.coefficients;
   need(c[0]+c[0]+c[1]+c[2]==previous,"streaming recurrence");
   if(t==0 && (mask&ui::U2))
    need(metrics.product_workers_peak==4 && metrics.parallel_product_tiles>0,"round zero did not execute a four-worker product");
   if(t==0 && (mask&ui::U3))
    need(metrics.parallel_product_tiles==1,"contiguous U/K borrow should dispatch as one batch");
   if(t<2 && (mask&ui::U3))
    need(metrics.u_span_elements && metrics.k_span_elements && !metrics.k_callback_calls && metrics.fused_wall>0,"fused span path not used");
   if(t==0 && (mask&ui::U3))need(metrics.mapping_calls==original_u.back().size()+2,"callback fallback lost coordinates");
   const Fr challenge(t%3==0?0:t%3==1?1:37);
   auto*fold_workers=(mask&ui::F1)?&workers:nullptr;
   state.advance("stream",challenge,fold_workers);folded.advance("stream",challenge,inputs,fold_workers);
   auto fold=[&](auto&roots){for(auto&v:roots)if(v.size()>1){for(std::size_t j=0;j<v.size()/2;++j)v[j]=v[2*j]+challenge*(v[2*j+1]-v[2*j]);v.resize(v.size()/2);}};
   fold(u);fold(k);
   prefix.push_back(challenge);
   previous=c[0]+challenge*(c[1]+challenge*c[2]);
   if(t<13)for(std::size_t root=0;root<k.size();++root)state.withRootSpan(root,"stream",[&](const Fr* values,std::size_t count){
    need(count==k[root].size() && std::equal(values,values+count,k[root].begin()),"round K state differential");
   });
  }
  Fr expected_u=0,expected_k=0;
  for(std::size_t root=0;root<u.size();++root){
   const auto w=ui::selector(segments[root],prefix);
   expected_u+=w*u[root][0];expected_k+=w*k[root][0];
  }
  need(last.input_pair[0]+prefix.back()*(last.input_pair[1]-last.input_pair[0])==expected_u,"final U endpoint differential");
  need(state.endpoint("stream",prefix)==expected_k && expected_u*expected_k==previous,"final K endpoint differential");
 }
 ui::PublicK protection({{0,2}},[](std::size_t){return std::vector<Fr>{Fr(1),Fr(2)};},{},"borrow",2);
 protection.withRootSpan(0,"borrow",[&](const Fr*data,std::size_t count){
  rejects([&]{protection.advance("borrow",Fr(3));},"active span borrow");
  need(count==2 && data[1]==Fr(2),"active K span invalidated");
 });
 protection.advance("borrow",Fr(3));
 rejects([&]{protection.withRootSpan(0,"foreign",[](const Fr*,std::size_t){});},"foreign authentication session");
 rejects([&]{protection.withRootSpan(0,"borrow",[](const Fr*,std::size_t){});},"closed/failed state");
}

void streamingBench(){
 using Clock=std::chrono::steady_clock;
 std::cout<<"component,coordinates,condition,mask,threads,repeat,round,wall_seconds,scan_seconds,product_seconds,fused_seconds,coefficient_multiplications,workers,u_callbacks,k_callbacks,u_span,k_span,tile_reads,tile_writes\n";
 for(const auto n:{std::size_t(1)<<16,std::size_t(1)<<20}){
  const std::vector<ui::Segment> segments{{0,n}};
  std::vector<Fr> u(n),k(n);
  std::mt19937_64 engine(424);
  for(std::size_t j=0;j<n;++j){u[j]=Fr(engine());k[j]=Fr(engine());}
  const ui::Inputs inputs=[&](std::size_t){return ui::InputSpan{u.data(),n,n};};
  ui::Round expected[2];
  for(const unsigned threads:{1u,2u,4u,8u,16u,32u}){
   ui::detail::Workers workers(threads);
   for(unsigned mask:{0u,1u,3u,7u}){
    if(threads!=1 && mask<3)continue;
    for(unsigned repeat=0;repeat<3;++repeat){
     ui::PublicK state(segments,[&](std::size_t,Fr*out,std::size_t count){std::copy_n(k.data(),count,out);},{},"bench",n);
     ui::FoldedU folded(n,segments,inputs,"bench");
     const ui::Factors provider=[&](std::size_t root,const ui::Consume& consume){
      state.withRoot(root,"bench",[&](const Value&b){consume([&](std::size_t j){return u[j];},b);});
     };
     ui::UOptions options;options.threads=threads;options.tile_pairs=(mask&ui::U2)?16384:1024;options.optimization=mask;
     std::vector<Fr> prefix;
     for(unsigned round=0;round<2;++round){
      ui::UMetrics m;
      const auto start=Clock::now();
      auto candidate=ui::quadraticRoundStreaming(n,segments,prefix,provider,inputs,options,&m,&folded,"bench",&state,&workers);
      const auto wall=std::chrono::duration<double>(Clock::now()-start).count();
      if(!mask && !repeat)expected[round]=candidate;
      need(candidate.coefficients==expected[round].coefficients,"microbenchmark result drift");
      std::cout<<"u_product,"<<(n>>round)<<','<<(round?"retained_after_fold":"initial_allocation")<<','<<mask<<','<<threads<<','<<repeat<<','<<round<<','<<std::setprecision(9)<<wall<<','<<m.scan_wall<<','<<m.product_wall<<','<<m.fused_wall<<','<<m.product_multiplications<<','<<m.product_workers_peak<<','<<m.mapping_calls<<','<<m.k_callback_calls<<','<<m.u_span_elements<<','<<m.k_span_elements<<','<<m.tile_reads<<','<<m.tile_writes<<'\n';
      if(round==0){
       state.advance("bench",Fr(29));folded.advance("bench",Fr(29),inputs);prefix.push_back(Fr(29));
      }
     }
    }
   }
  }
 }
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
 if(argc==2 && std::string(argv[1])=="product"){productDifferential();streamingDifferential();std::cout<<"PASS round differential and K span lifecycle\n";return 0;}
 if(argc==2 && std::string(argv[1])=="bench"){streamingBench();return 0;}
 if(argc==2 && std::string(argv[1])=="guards"){guards();return 0;}
 need(argc==4,"prove|verify plain|folded|streaming|snapshot|legacy|legacy-roots|category PATH");
 const std::string action=argv[1],mode=argv[2];
 const bool deterministic=action=="prove-fixed";
 const bool p=action=="prove" || deterministic,classified=mode=="legacy" || mode=="legacy-roots" || mode=="category";
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
 if(deterministic){
  zkgpt::r7::test::SeededRandom rng;
  zkgpt::r7::test::FixedFixtureProver io(argv[3],binding);
  if(classified)classifiedCase(io,&rng,layout,x,mode!="legacy-roots");
  else sparse(io,&rng,mode);
  io.finish();
  std::cout<<"PASS deterministic complete proof "<<mode<<' '<<io.proofBytes()<<'\n';
  return 0;
 }
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
