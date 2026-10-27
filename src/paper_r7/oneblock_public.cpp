#include "oneblock_public.hpp"
#include <memory>
#include <unordered_map>
namespace zkgpt::r7::oneblock {
namespace {
using Kernel=std::unordered_map<std::size_t,Fr>;
std::size_t paddedSize(std::size_t n){std::size_t out=1;while(out<n)out*=2;return out;}
Fr kernelMLE(const Kernel&k,const std::vector<Fr>&point){
 Fr sum=0;for(const auto&entry:k){Fr coefficient=1;for(std::size_t bit=0;bit<point.size();++bit)coefficient*=((entry.first>>bit)&1)?point[bit]:Fr(1)-point[bit];sum+=entry.second*coefficient;}return sum;
}
}
TrustedPublicEntries trustedPublicEntries(const R7Statement&s,const R7Context&context){
 const auto&plan=context.plan;plan.master.validate();need(plan.shape.sequence_length==32 && plan.shape.hidden_dimension==768 && s.x.size()==32768 && plan.master.actual>=s.x.size(),"Oneblock public frozen X shape");need(s.master_actual==plan.master.actual && s.master_padded==plan.master.padded && s.y.size()==plan.output.size(),"Oneblock public statement/context shape");TrustedPublicEntries out;
 out.x.reserve(s.x.size());for(std::size_t i=0;i<s.x.size();++i){need(s.x[i].isValid(),"Oneblock invalid public X scalar");out.x.emplace_back(i,s.x[i]);}
 out.y.reserve(s.y.size());for(std::size_t i=0;i<s.y.size();++i){need(plan.output[i]<plan.master.actual && s.y[i].isValid(),"Oneblock public Y trusted source bounds/value");out.y.emplace_back(plan.output[i],s.y[i]);}
 out.constants.reserve(plan.constants.size()+plan.reserved_zero.size());for(auto i:plan.constants){need(i<plan.master.actual,"Oneblock public one source bounds");out.constants.emplace_back(i,Fr(1));}for(auto i:plan.reserved_zero){need(i<plan.master.actual,"Oneblock public zero source bounds");out.constants.emplace_back(i,Fr(0));}return out;
}
void publicBinding(IO&io,const masterB::Registered&registered,const master3::Secret*secret,Value master,const PublicEntries&entries,Random*rng,const std::string&role){
 const auto&m=registered.get(io);need(m.spec.scope=="Base-Master-prefix" && !role.empty() && role.size()<=80,"Oneblock public Master scope/role");need(!entries.empty() && entries.size()<=100000,"Oneblock public entries bound");if(io.prover())need(secret && bool(master) && rng,"Oneblock public private Master input");
 Writer statement;statement.text("R7-ONEBLOCK-REGISTERED-MASTER-PUBLIC-BINDING-v1");statement.text(role);statement.text(io.session());statement.text(master3::identity(m));statement.number(m.spec.actual,8);statement.number(m.spec.N,8);statement.number(entries.size(),8);
 for(const auto&entry:entries){need(entry.first<m.spec.actual,"Oneblock public entry outside actual Master");need(entry.second.isValid(),"Oneblock invalid public entry scalar");statement.number(entry.first,8);statement.scalar(entry.second);}bindPublic(io,role+"/public-entries",statement.b);
 // All entries are fixed in the transcript before V samples even the first
 // position coordinate. The list's padded tail has no source and zero value.
 auto list_domain=paddedSize(entries.size());std::vector<Fr>position;for(unsigned bit=0;bit<r1::log2exact(list_domain);++bit)position.push_back(io.coin(role+"/position-query/"+std::to_string(bit)));auto weights=r1::weights(position);
 auto kernel=std::make_shared<Kernel>();kernel->reserve(entries.size());Fr expected=0;for(std::size_t i=0;i<entries.size();++i){kernel->try_emplace(entries[i].first,Fr(0)).first->second+=weights[i];expected+=weights[i]*entries[i].second;}
 Value public_kernel=[kernel](std::size_t i){auto found=kernel->find(i);return found==kernel->end()?Fr(0):found->second;};auto initial=publicHidden(io,role+"/expected",expected);Expression product{2,{{Fr(1),{0,1}}}};
 // expressionSumcheck's degree-two terminal authenticates BOTH factors. The
 // private factor opens through the registered token, and V recomputes K(r).
 Value padded_master;if(io.prover())padded_master=[master=std::move(master),actual=m.spec.actual](std::size_t i){return i<actual?master(i):Fr(0);};auto reduction=expressionSumcheck(io,m.spec.N,initial,{padded_master,public_kernel},product,rng,role+"/sumcheck");need(reduction.endpoints.size()==2,"Oneblock public reduction endpoints");registered.opening(io,{reduction.point,reduction.endpoints[0].handle},secret,io.prover()?&reduction.endpoints[0]:nullptr,rng,role+"/master-endpoint");equalHidden(io,reduction.endpoints[1],kernelMLE(*kernel,reduction.point),rng,role+"/kernel-endpoint");
}
}
