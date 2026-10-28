#include "classified_layout.hpp"
#include <iostream>
using namespace zkgpt::r7;using namespace oneblock;using namespace classified;
int main(){try{
 r1::init();auto native=discover();auto p=makeLayout(native.context);validate(p,native.context);
 need(p.base.size()==native.context.plan.master.actual,"classified every original coordinate routed");
 std::set<std::pair<std::size_t,std::size_t>>seen;
 for(auto&r:p.base)need(seen.insert({r.root,r.index}).second,"classified base partition injective");
 need(p.views.size()==native.context.objects.size()-1,"classified all nonmaster native views");
 auto bad=p;bad.base[1]=bad.base[0];bool rejected=false;try{validate(bad,native.context);}catch(...){rejected=true;}need(rejected,"classified wrong map rejected");
 need(identity(p)==identity(makeLayout(native.context)),"classified deterministic layout");
 std::size_t registered=0;for(auto&r:p.roots){need(r.padded<=(1u<<24),"classified bounded slab");if(r.registered)registered+=r.actual;}
 need(registered==native.context.plan.master.actual,"classified full base knowledge retained");
 std::cout<<"CLASSIFIED_LAYOUT_TESTED roots="<<p.roots.size()<<" registered="<<registered<<" views="<<p.views.size()<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"CLASSIFIED_LAYOUT_FAILED "<<e.what()<<'\n';return 1;}}
