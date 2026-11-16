#include "oneblock_plan.hpp"
#include "classified_layout.hpp"
#include "execution_target.hpp"
#include "oneblock_model.hpp"
#include <sstream>
#include <type_traits>
#include <iostream>
#include <cstdio>
#include <unistd.h>
int main(int argc,char**argv){try{
 using namespace zkgpt;using namespace r7;using crypto::need;r1::init();unsigned blocks=argc>1?std::stoul(argv[1]):1;
 static_assert(!std::is_copy_constructible_v<R6PublicArtifacts>);
 static_assert(!std::is_move_constructible_v<R6PublicArtifacts>);
 static_assert(std::is_same_v<decltype(std::declval<const R6PublicArtifacts&>().graph()),const layeredCircuit&>);
 // Public planning is called before interactive protocol I/O: neither cout
 // nor the native builder's printf diagnostics may escape onto stdout.
 std::cout.flush();std::fflush(stdout);auto capture=std::unique_ptr<FILE,decltype(&std::fclose)>(std::tmpfile(),std::fclose);need(bool(capture),"stdout capture file");
 int saved=dup(1);need(saved>=0 && dup2(fileno(capture.get()),1)>=0,"stdout capture descriptor");
 std::unique_ptr<R6PublicArtifacts>a;try{a=buildR6PublicArtifacts(blocks);}catch(...){dup2(saved,1);close(saved);throw;}
 std::cout.flush();std::fflush(stdout);dup2(saved,1);close(saved);need(std::ftell(capture.get())==0,"public build leaked standard-output diagnostics");
 const auto*storage=a->graph().circuit.data();
 auto p=oneblock::discover(*a);need(storage==a->graph().circuit.data(),"public graph storage was copied/replaced");
 std::ostringstream native;r6::writePlan(native,*a);need(native.str()==p.native_json,"audited native writer differs");
 need(p.context.plan.circuit_id==executionTarget(blocks).circuit && p.context.plan.master==a->plan().master,"same native query plan");
 auto c=buildR6Context(executionTarget(blocks).manifest_pin,*a);
 need(c.layout_sha256==p.context.layout_sha256 && c.objects.size()==p.objects.size(),"context and coverage layout/object identity");
 auto layout=oneblock::classified::makeCategoryLayout(c,*a);auto xs=nativeXSpans(c,*a);
 need(xs.size()==1 && xs[0].count==32768 && layout.x_spans[0].offset==xs[0].offset,"registry X identity");
 auto reject=[&](const auto&f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}need(rejected,"unsafe public artifact reuse accepted");};
 reject([&]{buildR6Context("short",*a);});auto wrong=c;wrong.plan.circuit_id="wrong";reject([&]{nativeXSpans(wrong,*a);});
 reject([&]{buildR6PublicArtifacts(2);});
 if(blocks==1){auto l=master3::trustedLayout(c,*a);master3::validate(l);auto trust=oneblock::model::loadTrust(*a);need(trust.plan.circuit_id==a->plan().circuit_id,"shared native operator trust");}
 // The independent legacy writer must audit graph bytes, including a changed residual coefficient.
 auto changed=a->graph();const auto last=a->network().blockConnections().back().last_layer;changed.circuit.at(last).uni_gates.at(0).sc=-1;
 reject([&]{std::ostringstream out;r6::writePlan(out,changed,a->network(),a->plan());});
 std::cout<<"PUBLIC_BUILD_SHARED_TESTED blocks="<<blocks<<" objects="<<c.objects.size()<<" relations="<<p.required.size()<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
