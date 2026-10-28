#include "crypto.hpp"
#include <sys/random.h>
#include <cerrno>
#include <algorithm>
#include <cstring>
namespace zkgpt::r7::crypto {
void need(bool ok,const char*m){if(!ok)throw std::runtime_error(m);}
namespace {
void curve(){need(mcl::bn::BN::param.cp.curveType==MCL_BN254 && Fr::getModulo()==r1::kModulus && mcl::bn::Fp::getModulo()=="16798108731015832284940804142231733909889187121439069848933715426072753864723" && G1::order_==Fr::getOp().mp && G1::a_.isZero() && G1::b_==G1::Fp(2) && !G1::Fp::isETHserialization() && G1::getSerializedByteSize()==32 && mcl::bn::getMapToMode()==MCL_MAP_TO_MODE_ORIGINAL,"R7 exact MCL BN254 curve/encoding/map mode required");}
void group(const G1&p){curve();need(p.isValid() && p.isValidOrder(),"R7 invalid curve/subgroup point");}
G1 derive(const std::string&role){G1 p;mcl::bn::hashAndMapToG1(p,std::string(suite)+"/mcl-hashAndMapToG1/MCL_MAP_TO_MODE_ORIGINAL/"+role);group(p);need(!p.isZero(),"R7 identity generator");return p;}
const Parameters& trusted(std::size_t n){curve();need(n && n<=64 && !(n&(n-1)),"R7 generator dimension bound");static std::map<std::size_t,Parameters>cache;auto it=cache.find(n);if(it!=cache.end())return it->second;Parameters p;p.scalar=derive("output/G");p.hiding=derive("blind/H");for(std::size_t i=0;i<n;++i)p.columns.push_back(derive("column/"+std::to_string(i)));auto all=p.columns;all.push_back(p.scalar);all.push_back(p.hiding);for(std::size_t i=0;i<all.size();++i)for(std::size_t j=0;j<i;++j)need(all[i]!=all[j] && all[i]!=-all[j],"R7 generator collision");return cache.emplace(n,std::move(p)).first->second;}
void spec(Writer&w,const PCSpec&s){w.text(s.suite);w.text(s.role);w.text(s.layout);for(auto n:{s.actual,s.padded,s.rows,s.columns})w.number(n,8);}
}
void Writer::number(std::uint64_t x,unsigned n){for(unsigned i=0;i<n;++i)b.push_back(std::uint8_t(x>>(8*i)));}
void Writer::raw(const Bytes&x){need(x.size()<=4*1024*1024 && b.size()<=4*1024*1024-x.size(),"R7 crypto wire bound");b.insert(b.end(),x.begin(),x.end());}
void Writer::text(const std::string&s){need(s.size()<=256,"R7 crypto string bound");number(s.size());raw(Bytes(s.begin(),s.end()));}
void Writer::scalar(const Fr&f){need(f.isValid(),"R7 invalid scalar");raw(r1::encode(f));}
void Writer::point(const G1&p){group(p);raw(r1::encode(p));}
Bytes Reader::raw(std::size_t n){need(at<=b.size() && n<=b.size()-at,"R7 crypto truncated wire");Bytes out(b.begin()+at,b.begin()+at+n);at+=n;return out;}
std::uint64_t Reader::number(unsigned n){auto x=raw(n);std::uint64_t v=0;for(unsigned i=0;i<n;++i)v|=std::uint64_t(x[i])<<(8*i);return v;}
std::string Reader::text(){auto n=number();need(n<=256,"R7 crypto string bound");auto x=raw(n);return {x.begin(),x.end()};}
Fr Reader::scalar(){return r1::decodeFr(raw(32));}
G1 Reader::point(){return decodePoint(raw(32));}
void Reader::end(){need(at==b.size(),"R7 crypto trailing bytes");}
G1 decodePoint(const Bytes&b){curve();auto p=r1::decodeG1(b);group(p);return p;}
void OSRandom::fill(std::uint8_t*out,std::size_t n){need(n==32 && ++draws_<=200000,"R7 entropy budget");std::size_t pos=0;while(pos<n){auto k=getrandom(out+pos,n-pos,0);if(k<0 && errno==EINTR)continue;need(k>0,"R7 system entropy failure");pos+=std::size_t(k);}Bytes b(out,out+n);need(std::any_of(b.begin(),b.end(),[](auto x){return x!=0;}) && seen_.emplace(b,true).second,"R7 entropy health failure");}
Bytes Random::bytes(std::size_t n){need(n==32,"R7 entropy block size");Bytes b(n);fill(b.data(),b.size());return b;}
Fr Random::scalar(bool nonzero){curve();for(unsigned i=0;i<4096;++i){auto b=bytes(32);Fr f;if(f.deserialize(b.data(),b.size())==b.size() && f.isValid() && r1::encode(f)==b && (!nonzero || !f.isZero()))return f;}throw std::runtime_error("R7 scalar rejection budget exhausted");}
Parameters parameters(std::size_t n){return trusted(n);}
void validate(const Parameters&p){need(p.suite==suite,"R7 wrong generator suite");const auto&e=trusted(p.columns.size());need(p.scalar==e.scalar && p.hiding==e.hiding && p.columns==e.columns,"R7 generators differ from independently derived suite");}
Metrics& metrics(){static Metrics m;return m;}
G1 msm(const std::vector<G1>&g,const std::vector<Fr>&x){need(g.size()==x.size() && !g.empty() && g.size()<=65,"R7 bounded MSM shape");auto&m=metrics();++m.msm_calls;m.msm_terms+=g.size();m.max_msm=std::max<std::uint64_t>(m.max_msm,g.size());G1 out;G1::mulVec(out,const_cast<G1*>(g.data()),const_cast<Fr*>(x.data()),x.size());return out;}
G1 scalarCommit(const Fr&v,const Fr&r){const auto&p=trusted(1);return msm({p.scalar,p.hiding},{v,r});}
PCSpec PCSpec::make(const std::string&role,std::size_t actual,std::size_t padded){PCSpec s;s.role=role;s.actual=actual;s.padded=padded;unsigned d=r1::log2exact(padded);s.rows=std::size_t(1)<<(d/2);s.columns=padded/s.rows;validate(s);return s;}
void validate(const PCSpec&s){need(s.suite==suite,"R7 wrong PC suite");need(!s.role.empty() && s.role.size()<=160 && s.layout=="column-major-i+R*j","R7 PC role/layout mismatch");need(s.actual && s.actual<=s.padded && s.padded<=maxN && !(s.padded&(s.padded-1)),"R7 PC dimensions/padding bound");auto d=r1::log2exact(s.padded);need(s.rows==(std::size_t(1)<<(d/2)) && s.columns==s.padded/s.rows,"R7 PC row/column mismatch");}
void validate(const PC&p){validate(p.spec);need(p.rows.size()==p.spec.rows,"R7 PC row commitment count");for(auto&g:p.rows)group(g);}
PCSecret commit(const PCSpec&s,const std::vector<Fr>&w,Random&rng){validate(s);need(w.size()==s.actual,"R7 witness length mismatch");PCSecret out;out.public_pc.spec=s;out.values=w;out.values.resize(s.padded,Fr(0));const auto&p=trusted(s.columns);for(std::size_t i=0;i<s.rows;++i){std::vector<Fr>row;for(std::size_t j=0;j<s.columns;++j){need(out.values[i+s.rows*j].isValid(),"R7 invalid witness scalar");row.push_back(out.values[i+s.rows*j]);}out.blinds.push_back(rng.scalar());auto g=p.columns;g.push_back(p.hiding);row.push_back(out.blinds.back());out.public_pc.rows.push_back(msm(g,row));}return out;}
Bytes encode(const PC&p){validate(p);Writer w;spec(w,p.spec);w.number(p.rows.size());for(auto&g:p.rows)w.point(g);return w.b;}
PC decodePC(const Bytes&b){Reader r{b};PC p;p.spec.suite=r.text();p.spec.role=r.text();p.spec.layout=r.text();p.spec.actual=r.number(8);p.spec.padded=r.number(8);p.spec.rows=r.number(8);p.spec.columns=r.number(8);validate(p.spec);need(r.number()==p.spec.rows,"R7 PC row count mismatch");for(std::size_t i=0;i<p.spec.rows;++i)p.rows.push_back(r.point());r.end();validate(p);return p;}
Digest pcIdentity(const PC&p){return r1::hash(encode(p));}
Bytes encode(const Handle&h){need(h.suite==suite && !h.role.empty() && h.role.size()<=160 && h.session.size()==64,"R7 HiddenScalar identity");Writer w;w.text(h.suite);w.text(h.session);w.text(h.role);w.point(h.point);return w.b;}
Handle decodeHandle(const Bytes&b){Reader r{b};Handle h;h.suite=r.text();h.session=r.text();h.role=r.text();h.point=r.point();r.end();encode(h);return h;}
void validate(const Handle&h,const std::string&sid,const std::string&role){encode(h);need(h.session==sid && h.role==role,"R7 HiddenScalar session/role identity mismatch");}
Hidden hide(const std::string&sid,const std::string&role,const Fr&v,Random&rng){Hidden x;x.value=v;x.blind=rng.scalar();x.handle.session=sid;x.handle.role=role;x.handle.point=scalarCommit(v,x.blind);return x;}
Hidden combine(const std::string&sid,const std::string&role,const std::vector<Hidden>&h,const std::vector<Fr>&a){need(h.size()==a.size(),"R7 handle combination shape");Hidden out;out.handle.session=sid;out.handle.role=role;out.handle.point.clear();for(std::size_t i=0;i<h.size();++i){need(h[i].handle.session==sid,"R7 cross-session handle");out.value+=a[i]*h[i].value;out.blind+=a[i]*h[i].blind;out.handle.point+=h[i].handle.point*a[i];}return out;}
std::string hex(const Digest&d){static const char*digits="0123456789abcdef";std::string out;for(auto c:d){out+=digits[c>>4];out+=digits[c&15];}return out;}
void bindPublic(IO&io,const std::string&label,const Bytes&b){need(io.message(label,b)==b,"R7 public statement/parent binding mismatch");}
}
