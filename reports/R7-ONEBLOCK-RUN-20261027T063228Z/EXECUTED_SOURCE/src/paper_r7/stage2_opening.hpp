#pragma once
#include "fixed_cw.hpp"
namespace zkgpt::r7::stage2 {
struct Phase {std::string name;double seconds=0;std::uint64_t p_wire_bytes=0,msm_calls=0,pc_total_N=0,folded_n=0;};
std::vector<Phase>&openingMeasurements();
class MaskLedger {
 std::set<std::string>seen_;
 public:void observe(const std::string&label,const Bytes&);
};
class ScopedIO final:public IO {
 IO&io_;std::string scope_;MaskLedger*ledger_;std::uint64_t sent_=0;
 public:ScopedIO(IO&i,std::string scope,MaskLedger*ledger=nullptr):io_(i),scope_(std::move(scope)),ledger_(ledger){}
 bool prover()const override{return io_.prover();}std::string session()const override{return io_.session();}Digest prefix()const override{return io_.prefix();}
 Bytes message(const std::string&,const Bytes& = {})override;Fr coin(const std::string&label)override{return io_.coin(scope_+"/"+label);}
 std::uint64_t sent()const{return sent_;}
};
class RegisteredCW {
 IO&io_;const Registry registry_;std::string session_;MaskLedger masks_;std::set<std::string>batches_;std::size_t openings_=0;
 RegisteredCW(IO&i,Registry r):io_(i),registry_(std::move(r)),session_(i.session()){}
 void open(const PC&,const std::string&,const std::vector<Query>&,const PCSecret*,const std::vector<Hidden>*,Random*,bool fixed);
 public:
 RegisteredCW(const RegisteredCW&)=delete;RegisteredCW&operator=(const RegisteredCW&)=delete;
 static RegisteredCW verifier(IO&,const TrustedRegistry&);
 static RegisteredCW prover(IO&,const Registry&,const PCSecret&,Random&);
 const Registry& approved()const{return registry_;}
 void openFixed(const std::string&batch,const std::vector<Query>&q,const PCSecret*s,const std::vector<Hidden>*h,Random*r){open(registry_.pc,batch,q,s,h,r,true);}
 void openInference(const PC&p,const std::string&batch,const std::vector<Query>&q,const PCSecret*s,const std::vector<Hidden>*h,Random*r){open(p,batch,q,s,h,r,false);}
};
}
