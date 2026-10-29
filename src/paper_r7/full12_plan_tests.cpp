#include "oneblock_plan.hpp"
#include "framework.hpp"
#include <iostream>
using namespace zkgpt::r7;using namespace oneblock;
int main(){try{
 zkgpt::r1::init();auto p=discover(12);requireCoverage(p);
 crypto::need(p.context.plan.shape.layer_count==12 && p.fc_layers==48,"real Full12 target/FC coverage");
 crypto::need(p.context.plan.master.actual==199348640 && p.context.plan.master.padded==(1u<<28),"Full12 native master regenerated");
 std::size_t exp=0;for(auto&e:p.context.plan.exps)exp+=p.context.plan.sources.at(e.index_child).spec.actual;
 crypto::need(exp==76032,"Full12 actual Exp queries");
 auto s=makeStatement(p.context,std::vector<Fr>(32768,Fr(0)),std::vector<Fr>(32768,Fr(0)));
 crypto::need(s.blocks==12 && validateStatement(s,p.context).code==Code::Ok,"Full12 external statement binding");
 s.blocks=1;crypto::need(validateStatement(s,p.context).code==Code::Reject,"suite/block downgrade rejected");
 auto b=p;b.required.pop_back();crypto::need(identity(b)!=identity(p),"missing native relation changes identity");
 b=p;b.required.push_back(b.required.front());bool rejected=false;try{requireCoverage(b);}catch(...){rejected=true;}crypto::need(rejected,"duplicate relation rejected");
 bool unsupported=false;try{discover(2);}catch(...){unsupported=true;}crypto::need(unsupported,"invented two-block target rejected");
 std::cout<<"FULL12_NATIVE_PLAN_TESTED FC="<<p.fc_layers<<" Exp="<<exp<<" relations="<<p.required.size()<<" objects="<<p.objects.size()<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
