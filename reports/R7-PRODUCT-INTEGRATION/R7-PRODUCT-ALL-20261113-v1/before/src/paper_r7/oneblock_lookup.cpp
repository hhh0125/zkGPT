#include "oneblock_lookup.hpp"
#include "perf4_profile.hpp"
#include "perf5_profile.hpp"
#include "perf6_options.hpp"
#include <algorithm>
#include <array>
#include <memory>
namespace zkgpt::r7::oneblock {
namespace {
void evidence(IO&io,const std::string&id){if(!io.prover())checked(io,id);}
Hidden exchange(IO&io,const std::string&role,const Fr&value,Random*rng){Hidden h;Bytes b;if(io.prover()){need(rng,"lookup entropy required");h=hide(io.session(),role,value,*rng);b=encode(h.handle);}b=io.message(role+"/handle",b);h.handle=decodeHandle(b);validate(h.handle,io.session(),role);return h;}
Hidden known(IO&io,const std::string&role,const Fr&value){Hidden h;h.value=value;h.handle.session=io.session();h.handle.role=role;h.handle.point=scalarCommit(value,Fr(0));return h;}
void linear(IO&io,const std::string&role,const std::vector<Hidden>&h,const std::vector<Fr>&a,Random*rng){need(h.size()==a.size(),"lookup linear shape");Relations r;r.role=role;for(auto&v:h)r.handles.push_back(v.handle);r.linear={a};sigma(io,r,io.prover()?&h:nullptr,rng);}
void constant(IO&io,const Hidden&h,const Fr&v,Random*rng,const std::string&role){linear(io,role,{h,known(io,role+"/public",v)},{Fr(1),Fr(-1)},rng);}
void shape(const VectorOracle&v){need(v.actual>0 && v.actual<=v.padded && v.padded<=(1u<<24) && !(v.padded&(v.padded-1)) && !v.id.empty() && bool(v.open),"lookup oracle shape/authenticator");}
void pairShape(const VectorOracle&a,const VectorOracle&b){shape(a);shape(b);need(a.actual==b.actual && a.padded==b.padded,"lookup source/value dimensions");}
std::vector<Fr> point(IO&io,std::size_t n,const std::string&role){std::vector<Fr>r;for(unsigned i=0;i<r1::log2exact(n);++i)r.push_back(io.coin(role+"/"+std::to_string(i)));return r;}
std::vector<Fr> evaluateWeights(const std::vector<Fr>&r){perf5::Scope t(perf5::LeafWeights);auto w=r1::weights(r);perf5::leafWeights(w.size());return w;}
Hidden evaluateWithWeights(IO&io,const VectorOracle&v,const std::vector<Fr>&r,const std::vector<Fr>&weights,Random*rng,const std::string&role){Fr value=0;if(io.prover()){need(bool(v.value),"lookup oracle missing private values");need(weights.size()==v.padded,"Evaluate shared weights dimension mismatch");for(std::size_t i=0;i<v.padded;++i)value+=weights[i]*v.value(i);}auto h=exchange(io,role,value,rng);v.open(io,{r,h.handle},io.prover()?&h:nullptr,rng,role+"/open");return h;}
Hidden evaluate(IO&io,const VectorOracle&v,const std::vector<Fr>&r,Random*rng,const std::string&role){Fr value=0;if(io.prover()){need(bool(v.value),"lookup oracle missing private values");auto w=evaluateWeights(r);for(std::size_t i=0;i<v.padded;++i)value+=w[i]*v.value(i);}auto h=exchange(io,role,value,rng);v.open(io,{r,h.handle},io.prover()?&h:nullptr,rng,role+"/open");return h;}
Fr publicMLE(const Value&v,std::size_t n,const std::vector<Fr>&r){auto w=r1::weights(r);need(w.size()==n,"lookup public MLE dimension");Fr out=0;for(std::size_t i=0;i<n;++i)out+=v(i)*w[i];return out;}
Fr active(std::size_t actual,const std::vector<Fr>&r){auto w=r1::weights(r);Fr out=0;for(std::size_t i=0;i<actual;++i)out+=w[i];return out;}
std::size_t integer(const Fr&x,std::size_t bound){auto b=r1::encode(x);std::uint64_t k=0;for(unsigned j=0;j<8;++j)k|=std::uint64_t(b[j])<<(8*j);for(unsigned j=8;j<32;++j)need(!b[j],"lookup private integer overflow");need(k<bound,"lookup private integer outside table");return std::size_t(k);}
Fr fingerprint(const Fr&a,const Fr&v,const Fr&c,const Fr&g,const Fr&g2,const Fr&t){return g2*a+g*v+c-t;}
// Fr addition/subtraction use fixed-size field arithmetic without allocation.
// Restore only after hiddenProduct's callbacks and tree owners are destroyed.
class AddOneGuard {
 std::vector<Fr>&values_;
 public:explicit AddOneGuard(std::vector<Fr>&values):values_(values){for(auto&x:values_)x+=Fr(1);}
 ~AddOneGuard() noexcept {for(auto&x:values_)x-=Fr(1);}
 AddOneGuard(const AddOneGuard&)=delete;
 AddOneGuard&operator=(const AddOneGuard&)=delete;
};
Value privateValues(IO&io,const std::shared_ptr<std::vector<Fr>>&v){if(!io.prover())return {};return [v](std::size_t i){return v->at(i);};}
void tail(IO&io,const VectorOracle&q,Random*rng,const std::string&role){if(q.actual==q.padded)return;auto r=point(io,q.padded,role+"/random");auto w=std::make_shared<std::vector<Fr>>(r1::weights(r));Value kernel=[w,actual=q.actual](std::size_t i){return i<actual?Fr(0):w->at(i);};auto zero=known(io,role+"/zero",Fr(0));std::vector<Value>f;if(io.prover())f={q.value,kernel};Expression expr{2,{{Fr(1),{0,1}}}};auto end=expressionSumcheck(io,q.padded,zero,f,expr,rng,role+"/sumcheck");need(end.endpoints.size()==2,"lookup tail endpoint count");q.open(io,{end.point,end.endpoints[0].handle},io.prover()?&end.endpoints[0]:nullptr,rng,role+"/source");constant(io,end.endpoints[1],publicMLE(kernel,q.padded,end.point),rng,role+"/kernel");}
}
void hiddenProduct(IO&io,std::size_t n,const Hidden&root,Value leaves,const LeafAuthenticator&auth,Random*rng,const std::string&role,const std::shared_ptr<std::vector<Fr>>&prepared_leaves){
 const auto*b2=std::getenv("ZKGPT_PERF6_B2");
 perf6::ScopedMessages message_options(b2 && std::string(b2)=="1");
 bool contiguous=false;if(io.prover()){const auto*c2=std::getenv("ZKGPT_PERF6_C2");contiguous=c2 && std::string(c2)=="1";}
 perf4::Scope native_scope("Native/Product",role,n);
 perf5::Product product_profile(role,n,io.prover());perf5::Scope product_total(perf5::Total);
 need(n && n<=(1u<<24) && !(n&(n-1)) && bool(auth),"lookup Product domain/authenticator");validate(root.handle,io.session(),root.handle.role);
 std::vector<std::shared_ptr<std::vector<Fr>>>tree;if(io.prover()){
  if(prepared_leaves){need(!prepared_leaves->empty() && prepared_leaves->size()==n,"lookup Product prepared leaves dimension mismatch");tree.push_back(prepared_leaves);}
  else{need(bool(leaves),"lookup Product leaves missing");std::shared_ptr<std::vector<Fr>>leaf;{perf5::Scope t(perf5::TreeAllocInit);leaf=std::make_shared<std::vector<Fr>>(n,Fr(0));}{perf5::Scope t(perf5::TreeLeaf);for(std::size_t i=0;i<n;++i)leaf->at(i)=leaves(i);}tree.push_back(leaf);}
  for(std::size_t width=n;width>1;width/=2){std::shared_ptr<std::vector<Fr>>next;{perf5::Scope t(perf5::TreeAllocInit);next=std::make_shared<std::vector<Fr>>(width/2,Fr(0));}{perf5::Scope t(perf5::TreeParent);for(std::size_t j=0;j<width/2;++j)next->at(j)=tree.back()->at(2*j)*tree.back()->at(2*j+1);}tree.push_back(next);}}
 // The supplied root replaces the stored top level; only children are read.
 if(io.prover() && n>1)tree.pop_back();
 Hidden carry=root;std::vector<Fr>row;const auto d=r1::log2exact(n);
 for(unsigned layer=0;layer<d;++layer){auto width=std::size_t(1)<<layer;auto name=role+"/level/"+std::to_string(layer);std::shared_ptr<std::vector<Fr>>w;Value eq;if(io.prover()){perf5::Scope t(perf5::Weights);perf5::weights(width);w=std::make_shared<std::vector<Fr>>(r1::weights(row));eq=[w](std::size_t j){return w->at(j);};}std::vector<Value>f;if(io.prover()){auto child=tree.at(d-layer-1);f={eq,[child](std::size_t j){return child->at(2*j);},[child](std::size_t j){return child->at(2*j+1);}};}
  Reduction end;
  if(contiguous){ProductInputs inputs{*w,*tree.at(d-layer-1)};end=productSumcheck(io,width,carry,f,rng,name+"/sumcheck",&inputs);}
  else end=productSumcheck(io,width,carry,f,rng,name+"/sumcheck");
  need(end.endpoints.size()==3,"lookup Product endpoint count");{perf5::Scope t(perf5::Equality);constant(io,end.endpoints[0],r1::equality(row,end.point),rng,name+"/equality");}{perf5::Scope t(perf5::Carry);auto r=io.coin(name+"/fold");carry=combine(io.session(),name+"/carry",{end.endpoints[1],end.endpoints[2]},{Fr(1)-r,r});bindPublic(io,name+"/carry",encode(carry.handle));row=end.point;row.insert(row.begin(),r);}
  if(io.prover()){perf5::Scope t(perf5::TreeFree);f.clear();if(perf5::context){const bool external_leaf=prepared_leaves && d-layer-1==0 && tree.at(0).get()==prepared_leaves.get();need(tree.at(d-layer-1).use_count()==(external_leaf?2:1),"PERF5 TreeFree requires final tree owner");}tree.at(d-layer-1).reset();}
 }
 {perf5::Scope t(perf5::LeafAuthentication);auth(io,{row,carry.handle},io.prover()?&carry:nullptr,rng,role+"/leaf");}
 // A layer is recorded only after the recursive chain reaches an actually
 // authenticated source leaf. A provisional recurrence is not tree coverage.
 {perf5::Scope t(perf5::Coverage);for(unsigned layer=0;layer<d;++layer)evidence(io,role+"/level/"+std::to_string(layer));
 evidence(io,role+"/root");}
}
void hiddenMemory(IO&io,const VectorOracle&a,const VectorOracle&v,const std::vector<Fr>&table,const VectorFactory&factory,Random*rng,const std::string&role,bool prepared){
 perf4::Scope native_scope("Native/Memory",role,a.padded);
 pairShape(a,v);need(!table.empty() && table.size()<=(1u<<20) && !(table.size()&(table.size()-1)),"lookup complete table size");Writer identity;identity.text("R7-HIDDEN-LASSO-v1");identity.text(a.id);identity.text(v.id);identity.number(a.actual,8);identity.number(a.padded,8);identity.number(table.size(),8);r1::Fs tableIdentity("R7-HIDDEN-LASSO-COMPLETE-TABLE-v1");tableIdentity.integer("count",table.size());for(auto&x:table)tableIdentity.scalar("value",x);auto tableDigest=tableIdentity.digest();identity.raw(Bytes(tableDigest.begin(),tableDigest.end()));bindPublic(io,role+"/identity",identity.b);
 auto readValues=std::make_shared<std::vector<Fr>>(),finalValues=std::make_shared<std::vector<Fr>>();if(io.prover() && !prepared){readValues->resize(a.padded,Fr(0));finalValues->resize(table.size(),Fr(0));std::vector<std::size_t>counts(table.size(),0);for(std::size_t i=0;i<a.padded;++i){auto j=integer(a.value(i),table.size());need(v.value(i)==table[j],"lookup private memory value mismatch");readValues->at(i)=Fr(counts[j]++);}for(std::size_t j=0;j<table.size();++j)finalValues->at(j)=Fr(counts[j]);}
 auto read=factory(io,role+"/read",a.padded,a.padded,privateValues(io,readValues));auto final=factory(io,role+"/final",table.size(),table.size(),privateValues(io,finalValues));readValues.reset();finalValues.reset();shape(read);shape(final);need(read.actual==a.padded && read.padded==a.padded && final.actual==table.size() && final.padded==table.size(),"lookup counter factory dimensions");
 auto g=io.coin(role+"/gamma"),tau=io.coin(role+"/tau");Fr g2;{perf4::Scope setup("Native/Fingerprint-setup",role,1);g2=g*g;}
 std::array<Value,3>leaves;std::array<Hidden,3>roots;
 std::shared_ptr<std::vector<Fr>>base_leaves;Fr root0=1,root1=1;
 if(io.prover()){
  perf4::Scope prepare("Native/Product01-base-leaves",role,a.padded);
  base_leaves=std::make_shared<std::vector<Fr>>(a.padded,Fr(0));
  for(std::size_t i=0;i<a.padded;++i){auto z=fingerprint(a.value(i),v.value(i),read.value(i),g,g2,tau);base_leaves->at(i)=z;root0*=z+Fr(1);root1*=z;}
 }
 // Preserve the root handle/RNG/message order despite computing 0/1 together.
 for(unsigned kind=0;kind<3;++kind){Fr value=kind==0?root0:kind==1?root1:Fr(1);auto size=kind==2?table.size():a.padded;if(io.prover() && kind==2){leaves[kind]=[&table,final,g,g2,tau](std::size_t i){return fingerprint(Fr(i),table[i],final.value(i),g,g2,tau);};for(std::size_t i=0;i<size;++i)value*=leaves[kind](i);}roots[kind]=exchange(io,role+"/root/"+std::to_string(kind),value,rng);}
 Fr initial=1;for(std::size_t i=0;i<table.size();++i)initial*=fingerprint(Fr(i),table[i],Fr(0),g,g2,tau);auto product=exchange(io,role+"/multiset-product",io.prover()?roots[1].value*roots[2].value:Fr(0),rng);std::vector<Hidden>h{roots[0],roots[1],roots[2],product};Relations multiset;multiset.role=role+"/multiset";for(auto&x:h)multiset.handles.push_back(x.handle);multiset.products={{1,2,3}};multiset.linear={{initial,Fr(0),Fr(0),Fr(-1)}};sigma(io,multiset,io.prover()?&h:nullptr,rng);
 for(unsigned kind=0;kind<3;++kind){LeafAuthenticator auth=[&,kind,g,g2,tau](IO&live,const Query&q,const Hidden*leaf,Random*random,std::string name){Hidden c,aa,vv;if(kind==2){c=evaluate(live,final,q.point,random,name+"/final");Fr address=0,scale=1;for(auto&bit:q.point){address+=scale*bit;scale*=Fr(2);}auto expected=g2*address+g*r1::mle(table,q.point)-tau;Hidden l;if(live.prover())l=*leaf;else l.handle=q.evaluation;linear(live,name+"/fingerprint",{l,c,known(live,name+"/public",expected)},{Fr(1),Fr(-1),Fr(-1)},random);evidence(live,name+"/final");}else{
   // The three MLEs and openings stay sequential and independent. Only this
   // invocation's challenge-dependent weight vector is shared.
   std::vector<Fr>weights;if(live.prover())weights=evaluateWeights(q.point);
   aa=evaluateWithWeights(live,a,q.point,weights,random,name+"/address");
   vv=evaluateWithWeights(live,v,q.point,weights,random,name+"/value");
   c=evaluateWithWeights(live,read,q.point,weights,random,name+"/read");
   Hidden l;if(live.prover())l=*leaf;else l.handle=q.evaluation;linear(live,name+"/fingerprint",{l,aa,vv,c,known(live,name+"/offset",Fr(kind==0?1:0)-tau)},{Fr(1),-g2,-g,Fr(-1),Fr(-1)},random);evidence(live,name+"/address");evidence(live,name+"/value");evidence(live,name+"/read");}};
  auto product=[&]{if(kind==2)hiddenProduct(io,table.size(),roots[kind],leaves[kind],auth,rng,role+"/product/2",{});else hiddenProduct(io,a.padded,roots[kind],{},auth,rng,role+"/product/"+std::to_string(kind),base_leaves);};
  if(kind==0 && io.prover()){AddOneGuard guard(*base_leaves);product();}else product();}
 evidence(io,role+"/multiset");
}
void hiddenRange(IO&io,const VectorOracle&q,unsigned bits,const VectorFactory&factory,Random*rng,const std::string&role,bool prepared){
 perf4::Scope native_scope("Native/Range",role,q.padded);
 shape(q);need(bits>=1 && bits<=126,"lookup Range bits");Writer statement;statement.text("R7-HIDDEN-RANGE-9BIT-v1");statement.text(q.id);statement.number(bits);statement.number(q.actual,8);statement.number(q.padded,8);bindPublic(io,role+"/statement",statement.b);unsigned count=(bits+8)/9;std::vector<VectorOracle>digits;
 for(unsigned h=0;h<count;++h){auto values=std::make_shared<std::vector<Fr>>();if(io.prover() && !prepared){values->resize(q.padded,Fr(0));for(std::size_t i=0;i<q.padded;++i){auto b=r1::encode(q.value(i));for(unsigned bit=bits;bit<256;++bit)need(!(b[bit/8]&(1u<<(bit%8))),"lookup private Range exceeds bitwidth");need(i<q.actual || q.value(i).isZero(),"lookup private Range nonzero padding");unsigned digit=0;for(unsigned j=0;j<std::min(9u,bits-9*h);++j){auto k=9*h+j;digit|=((b[k/8]>>(k%8))&1u)<<j;}values->at(i)=Fr(digit);}}
  digits.push_back(factory(io,role+"/digit/"+std::to_string(h),q.actual,q.padded,privateValues(io,values)));shape(digits.back());need(digits.back().actual==q.actual && digits.back().padded==q.padded,"lookup digit factory dimensions");}
 auto r=point(io,q.padded,role+"/row");std::vector<Hidden>opened{evaluate(io,q,r,rng,role+"/query")};std::vector<Fr>coeff{Fr(-1)};Fr scale=1;for(unsigned h=0;h<count;++h){opened.push_back(evaluate(io,digits[h],r,rng,role+"/digit-evaluation/"+std::to_string(h)));coeff.push_back(scale);scale*=Fr(512);}linear(io,role+"/reconstruction",opened,coeff,rng);evidence(io,role+"/reconstruction");for(unsigned h=0;h<count;++h)evidence(io,role+"/digit/"+std::to_string(h)+"/opening");
 for(unsigned h=0;h<count;++h){std::vector<Fr>table(std::size_t(1)<<std::min(9u,bits-9*h),Fr(0));for(std::size_t j=0;j<table.size();++j)table[j]=Fr(j);hiddenMemory(io,digits[h],digits[h],table,factory,rng,role+"/memory/"+std::to_string(h),prepared);}tail(io,q,rng,role+"/zero-tail");evidence(io,role+"/zero-tail"); // Includes a verifier-known empty tail when actual==padded.
}
void hiddenRangeBinding(IO&io,const VectorOracle&source,const VectorOracle&encoded,unsigned bits,bool sign,Random*rng,const std::string&role){pairShape(source,encoded);need(bits>=1 && bits<=126,"lookup binding bitwidth");Writer identity;identity.text(source.id);identity.text(encoded.id);identity.number(bits);identity.number(sign);bindPublic(io,role+"/identity",identity.b);auto r=point(io,source.padded,role+"/point");auto s=evaluate(io,source,r,rng,role+"/source"),q=evaluate(io,encoded,r,rng,role+"/encoded");Fr bias=sign?Fr(1):Fr(0);if(sign)for(unsigned i=1;i<bits;++i)bias*=Fr(2);auto b=known(io,role+"/bias",bias*active(source.actual,r));linear(io,role+"/relation",{q,s,b},{Fr(1),Fr(-1),Fr(-1)},rng);}
void hiddenExp(IO&io,const VectorOracle&index,const VectorOracle&value,r1::ExpProfile profile,const VectorFactory&factory,Random*rng,const std::string&role,bool prepared){
 perf4::Scope native_scope("Native/Exp",role,index.padded);
 pairShape(index,value);need(profile==r1::ExpProfile::LegacyClampOne || profile==r1::ExpProfile::Unclamped,"lookup Exp profile");Writer statement;statement.text("R7-HIDDEN-EXP-u32-St2^-9-Se2^-20-v1");statement.text(index.id);statement.text(value.id);statement.number(unsigned(profile));auto digest=r1::expTableDigest(profile);statement.raw(Bytes(digest.begin(),digest.end()));bindPublic(io,role+"/statement",statement.b);
 constexpr std::uint32_t K=8191,Scale=1048576;std::array<VectorOracle,4>helper;const std::array<std::string,4>names{"x","selector","delta","native-E"};for(unsigned k=0;k<4;++k){auto values=std::make_shared<std::vector<Fr>>();if(io.prover() && !prepared){values->resize(index.padded,Fr(0));for(std::size_t i=0;i<index.padded;++i){if(i>=index.actual){if(k==3)values->at(i)=value.value(i)+Fr(Scale);continue;}auto raw=integer(index.value(i),std::uint64_t(1)<<32);values->at(i)=k==0?Fr(std::min<std::size_t>(raw,K)):k==1?Fr(raw<K?1:0):k==2?Fr(raw<K?K-1-raw:raw-K):value.value(i);}}helper[k]=factory(io,role+"/"+names[k],index.actual,index.padded,privateValues(io,values));shape(helper[k]);need(helper[k].actual==index.actual && helper[k].padded==index.padded,"lookup Exp factory dimensions");}
 hiddenRange(io,index,32,factory,rng,role+"/range/t",prepared);for(unsigned k=0;k<3;++k)hiddenRange(io,helper[k],std::array<unsigned,3>{13,1,32}[k],factory,rng,role+"/range/"+names[k],prepared);
 for(unsigned kind=0;kind<2;++kind){auto name=role+"/branch/"+std::to_string(kind);auto row=point(io,index.padded,name+"/row");auto w=std::make_shared<std::vector<Fr>>(r1::weights(row));Value eq=[w](std::size_t i){return w->at(i);},mask=[actual=index.actual](std::size_t i){return Fr(i<actual?1:0);};std::vector<Value>f;if(io.prover())f={eq,helper[kind==0?0:2].value,helper[1].value,index.value,mask};Expression expr;expr.factor_count=5;
  if(kind==0)expr.terms={{Fr(1),{0,1}},{Fr(-1),{0,2,3}},{Fr(-std::int64_t(K)),{0,4}},{Fr(K),{0,2}}};else expr.terms={{Fr(1),{0,1}},{Fr(2),{0,2,3}},{Fr(-1),{0,4,3}},{Fr(K),{0,4}},{Fr(-std::int64_t(2*K-1)),{0,2}}};
  auto zero=known(io,name+"/zero",Fr(0));auto end=expressionSumcheck(io,index.padded,zero,f,expr,rng,name+"/sumcheck");need(end.endpoints.size()==5,"lookup Exp branch endpoint count");constant(io,end.endpoints[0],r1::equality(row,end.point),rng,name+"/eq");constant(io,end.endpoints[4],active(index.actual,end.point),rng,name+"/mask");helper[kind==0?0:2].open(io,{end.point,end.endpoints[1].handle},io.prover()?&end.endpoints[1]:nullptr,rng,name+"/helper");helper[1].open(io,{end.point,end.endpoints[2].handle},io.prover()?&end.endpoints[2]:nullptr,rng,name+"/selector");index.open(io,{end.point,end.endpoints[3].handle},io.prover()?&end.endpoints[3]:nullptr,rng,name+"/index");evidence(io,role+"/constraint/"+std::to_string(kind));}
 auto r=point(io,index.padded,role+"/value-point");auto original=evaluate(io,value,r,rng,role+"/original-E"),native=evaluate(io,helper[3],r,rng,role+"/native-E-evaluation");auto pad=known(io,role+"/padding",Fr(Scale)*(Fr(1)-active(index.actual,r)));linear(io,role+"/value-binding",{native,original,pad},{Fr(1),Fr(-1),Fr(-1)},rng);evidence(io,role+"/value-binding/0");evidence(io,role+"/value-binding/1");hiddenMemory(io,helper[0],helper[3],r1::expTable(profile),factory,rng,role+"/indexed",prepared);
}
}
