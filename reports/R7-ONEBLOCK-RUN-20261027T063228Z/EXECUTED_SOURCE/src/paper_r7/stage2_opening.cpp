#include "stage2_opening.hpp"
#include "wire.hpp"
#include <chrono>
namespace zkgpt::r7::stage2 {
void MaskLedger::observe(const std::string&label,const Bytes&b){
 const bool mask=label=="A3/knowledge/masks" || label=="A3/final/delta-beta" || (label.rfind("A3/reduce/",0)==0 && label.size()>6 && label.substr(label.size()-6)=="/cross");
 if(!mask)return;
 need(seen_.size()<512,"S2 mask ledger budget");auto key=hex(r1::hash(b));need(seen_.insert(key).second,"S2 repeated public protocol masks rejected");
}
std::vector<Phase>&openingMeasurements(){static std::vector<Phase>v;return v;}
Bytes ScopedIO::message(const std::string&label,const Bytes&b){auto name=scope_+"/"+label;auto out=io_.message(name,b);if(prover())sent_+=R7Wire::encodeLocalRecord({session(),name,0,out}).size()+4;if(!prover() && ledger_)ledger_->observe(label,out);return out;}
RegisteredCW RegisteredCW::verifier(IO&io,const TrustedRegistry&r){need(!io.prover(),"S2 verifier context role");registration(io,r.value(),nullptr,nullptr);return RegisteredCW(io,r.value());}
RegisteredCW RegisteredCW::prover(IO&io,const Registry&r,const PCSecret&s,Random&rng){need(io.prover(),"S2 prover context role");registration(io,r,&s,&rng);return RegisteredCW(io,r);}
void RegisteredCW::open(const PC&pc,const std::string&batch,const std::vector<Query>&queries,const PCSecret*secret,const std::vector<Hidden>*hidden,Random*rng,bool fixed){
 need(io_.session()==session_,"S2 registry context cross-session");need(!batch.empty() && batch.size()<=100 && batches_.size()<16 && batches_.insert(batch).second,"S2 duplicate/invalid batch ID");
 need(queries.size()==1 || queries.size()==2 || queries.size()==4,"S2 batch K must be1/2/4");need(openings_+queries.size()<=32,"S2 opening budget");
 validate(pc);need(pc.spec.actual==pc.spec.padded,"S2 inference padded PC INCOMPLETE without zero-tail authentication");
 if(fixed)need(encode(pc)==encode(registry_.pc),"S2 fixed commitment substitution");else need(pc.spec.role.rfind("C_W_component",0)!=0,"S2 fixed weights require registered path");
 if(io_.prover())need(hidden && hidden->size()==queries.size(),"S2 batch witness shape");
 Writer binding;binding.text(suite);binding.text(hyrax_a3::messageVersion);binding.text(registryDigest(registry_));binding.text(hex(pcIdentity(pc)));binding.text(batch);binding.text(session_);auto parent=io_.prefix();binding.raw(Bytes(parent.begin(),parent.end()));binding.number(queries.size());
 for(std::size_t i=0;i<queries.size();++i){binding.number(i);binding.raw(encode(queries[i].evaluation));binding.number(queries[i].point.size());for(auto&a:queries[i].point)binding.scalar(a);}
 need(io_.message("S2/batch/"+batch+"/binding",binding.b)==binding.b,"S2 batch registry/handle/query binding mismatch");
 for(std::size_t i=0;i<queries.size();++i){auto name="S2/open/"+batch+"/"+std::to_string(i);ScopedIO scoped(io_,name,&masks_);auto start=std::chrono::steady_clock::now();auto calls=metrics().msm_calls;hyrax_a3::opening(scoped,pc,queries[i],secret,io_.prover()?&hidden->at(i):nullptr,rng);openingMeasurements().push_back({name,std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count(),scoped.sent(),metrics().msm_calls-calls,pc.spec.padded,pc.spec.columns});++openings_;}
}
}
