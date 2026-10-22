#include "exp.hpp"
#include "wire.hpp"
#include <iostream>
using namespace zkgpt::r1;
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}require(bad,"Exp profile attack accepted");}
int main(){try{
 init();ExpSpec s{"profile/Exp",PCSpec::make("profile/t",6,8,Layout::ColumnMajor),PCSpec::make("profile/E",6,8,Layout::RowMajor),ExpProfile::LegacyClampOne};
 std::vector<Fr> t{Fr(0),Fr(6891),Fr(7453),Fr(8191),Fr(8192),Fr(std::uint64_t(0xffffffff)),Fr(0),Fr(0)},e(8,Fr(0));
 for(unsigned i=0;i<6;++i)e[i]=Fr(expValue(std::uint32_t(t[i].getInt64()),s.profile));
 auto ct=commit(s.index,t),ce=commit(s.value,e);Fs a("profile"),b("profile");auto p=proveExp(s,ct,ce,t,e,a);verifyExp(s,ct,ce,p,b);
 require(a.digest()==b.digest() && expValue(0xffffffff,s.profile)==1,"clamp tail and transcript");
 require(expTableDigest(s.profile)!=expTableDigest(),"profiles share table identity");
 auto strict=s;strict.profile=ExpProfile::Unclamped;
 rejects([&]{serializeExp(ExpStatement{s,ct,ce});});
 rejects([&]{serializeExp(ExpStatement{s,ct,ce},p);});
 rejects([&]{Fs v("profile");verifyExp(strict,ct,ce,p,v);});
 auto wrong=e;wrong[4]=Fr(0);rejects([&]{Fs v("profile");proveExp(s,ct,commit(s.value,wrong),t,wrong,v);});
 auto wrong_t=t;wrong_t[0]=Fr(1);rejects([&]{Fs v("profile");proveExp(s,commit(s.index,wrong_t),ce,wrong_t,e,v);});
 auto padding=e;padding[6]=Fr(1);rejects([&]{Fs v("profile");proveExp(s,ct,commit(s.value,padding),t,padding,v);});
 auto invalid=s;invalid.profile=static_cast<ExpProfile>(7);rejects([&]{invalid.validate();});
 std::cout<<"PASS explicit clamped indexed relation, padding and profile substitution rejection\n";return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
