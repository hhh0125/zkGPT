#pragma once
#include "streaming_master.hpp"
#include "sigma.hpp"
namespace zkgpt::r7::master3 {
class RegisteredMaster {
 IO&io_;const Master&master_;std::string session_;
 RegisteredMaster(IO&i,const Master&m):io_(i),master_(m),session_(i.session()){}
 public:RegisteredMaster(const RegisteredMaster&)=delete;
 RegisteredMaster(RegisteredMaster&&)=default;
 static RegisteredMaster establish(IO&i,const Master&m,const Secret*s){registerMaster(i,m,s);return RegisteredMaster(i,m);}
 const Master&get(IO&i)const{need(&i==&io_ && i.session()==session_,"M3 registered context session mismatch");return master_;}
};
void bindChild(IO&,RegisteredMaster&,const PC&,const Gather&,const Secret*,const PCSecret*,Random*,const std::string&attack="");
}
