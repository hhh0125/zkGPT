#include "pc.hpp"
#include "wire.hpp"
#include <algorithm>
#include <functional>
#include <iostream>
#include <type_traits>
using namespace zkgpt::r1;
using mcl::bn::G2;
using Fp=G1::Fp;
static unsigned checks=0,order_calls=0;
static void check(bool ok,const char *name){++checks;if(!ok)throw std::runtime_error(name);}
static void rejects(const std::function<void()> &f,const char *name){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,name);}
static bool countedOrder(const G1 &p){++order_calls;G1 q;G1::mulGeneric(q,p,G1::order_);return q.isZero();}
struct CountOrder {decltype(G1::isValidOrderFast) prior;CountOrder():prior(G1::isValidOrderFast){order_calls=0;G1::isValidOrderFast=countedOrder;}~CountOrder(){G1::isValidOrderFast=prior;}};
static Bytes strictEncode(const G1 &p){require(p.isValid()&&p.isValidOrder(),"strict invalid G1");Bytes out(G1::getSerializedByteSize());require(p.serialize(out.data(),out.size())==out.size(),"strict size");return out;}
static G1 strictDecode(const Bytes &b){require(b.size()==G1::getSerializedByteSize(),"strict length");G1 p;require(p.deserialize(b.data(),b.size())==b.size()&&p.isValid()&&p.isValidOrder(),"strict decode");require(strictEncode(p)==b,"strict canonical");return p;}
static G1 point(unsigned i){G1 p;mcl::bn::hashAndMapToG1(p,"PERF-P0A/fixture/"+std::to_string(i));return p;}
static Bytes integerBytes(const mpz_class &v){Bytes out(32,0);mcl::gmp::getArray(out.data(),out.size(),v);return out;}
static void compareDecode(const Bytes &b){bool old_ok=true,new_ok=true;G1 oldp,newp;try{oldp=strictDecode(b);}catch(const std::exception&){old_ok=false;}try{newp=decodeG1(b);}catch(const std::exception&){new_ok=false;}check(old_ok==new_ok,"strict/new decode acceptance");if(old_ok)check(strictEncode(oldp)==encode(newp),"strict/new decoded canonical bytes");}
static void corruptGroup(Bytes &wire,const G1 &p){auto bytes=encode(p);auto at=std::search(wire.begin(),wire.end(),bytes.begin(),bytes.end());check(at!=wire.end(),"wire group fixture found");auto bad=integerBytes(Fp::getOp().mp);std::copy(bad.begin(),bad.end(),at);}
int main(){try{
 init();
 static_assert(!std::is_invocable_v<Bytes(*)(const G1&),const G2&>,"R1 G1 codec must not accept G2");
 check(Fp::getModulo()=="16798108731015832284940804142231733909889187121439069848933715426072753864723","pinned BN254 p");
 check(Fr::getModulo()==kModulus&&G1::a_.isZero()&&G1::b_==Fp(2),"pinned BN254 equation/r");
 G1 infinity;infinity.clear();check(encode(infinity)==Bytes(32,0),"canonical infinity bytes");check(decodeG1(Bytes(32,0)).isZero(),"external canonical infinity accepted");
 for(unsigned i=0;i<64;++i){auto a=point(i),b=point(i+100);for(const auto &p:std::vector<G1>{a,-a,a+b,a-b,a*Fr(i),infinity}){auto expected=strictEncode(p);check(encode(p)==expected,"normal/closure canonical byte identity");check(decodeG1(expected)==p,"normal/closure roundtrip");Fs x("g1-diff"),y("g1-diff");x.bytes("group",expected);y.group("group",p);check(x.digest()==y.digest()&&x.challenge("r")==y.challenge("r"),"FS digest/challenge identity");}}
 for(int delta=-2;delta<=2;++delta)for(unsigned sign:{0U,128U}){auto b=integerBytes(Fp::getOp().mp+delta);b.back()|=sign;compareDecode(b);if(delta>=0)rejects([&]{decodeG1(b);},"coordinate >=p rejected");}
 auto good=encode(point(7));auto short_b=good;short_b.pop_back();rejects([&]{decodeG1(short_b);},"truncated G1");auto long_b=good;long_b.push_back(0);rejects([&]{decodeG1(long_b);},"trailing G1 bytes");
 auto unused=good;unused.back()|=64;rejects([&]{decodeG1(unused);},"unused high coordinate bit");
 bool nonsquare_found=false;for(unsigned i=1;i<128;++i){Fp x=i,y2,y;G1::getWeierstrass(y2,x);if(!Fp::squareRoot(y,y2)){nonsquare_found=true;Bytes b(32,0);b[0]=i;rejects([&]{decodeG1(b);},"compressed off-curve nonsquare");break;}}check(nonsquare_found,"nonsquare fixture discovery");
 for(unsigned i=0;i<128;++i){auto b=hash(Bytes{static_cast<unsigned char>(i),99});compareDecode(Bytes(b.begin(),b.end()));}
 auto p=point(8);auto old=encode(p);p.y+=Fp(1);rejects([&]{encode(p);},"same-address mutation rejected");p=point(8);check(encode(p)==old,"same-address restored point accepted");
 for(unsigned i=0;i<64;++i){G1 temporary=point(i);check(encode(temporary)==strictEncode(temporary),"temporary lifetime no stale validation");}
 auto spec=PCSpec::make("p0a/commit",8,8,Layout::RowMajor);std::vector<Fr>w{1,2,3,4,5,6,7,8},r{Fr(2),Fr(3),Fr(4)};auto c=commit(spec,w);Fs pt("p0a-pc");auto proof=open(spec,c,w,r,pt,"claim");Fs vt("p0a-pc");verify(spec,c,r,proof,vt,"claim");check(pt.digest()==vt.digest(),"small PC transcript convergence");
 auto badc=c;badc.rows[0].y+=Fp(1);rejects([&]{Fs t("p0a-pc");verify(spec,badc,r,proof,t,"claim");},"off-curve commitment row rejected");badc=c;badc.rows[0]+=point(99);rejects([&]{Fs t("p0a-pc");verify(spec,badc,r,proof,t,"claim");},"on-curve changed commitment rejected");badc=c;badc.spec.role="p0a/other";rejects([&]{Fs t("p0a-pc");verify(spec,badc,r,proof,t,"claim");},"cross-role commitment rejected");
 auto badproof=proof;badproof.rounds[0].left+=point(98);rejects([&]{Fs t("p0a-pc");verify(spec,c,r,badproof,t,"claim");},"on-curve altered opening rejected");Fs restored("p0a-pc");verify(spec,c,r,proof,restored,"claim");check(restored.digest()==vt.digest(),"original object after copied mutation");
 LookupBatchSpec s;s.input.context="P0A/untrusted-artifacts";s.input.master=c;auto childSpec=PCSpec::make("p0a/query",8,8,Layout::ColumnMajor);auto child=commit(childSpec,w);s.input.children.push_back({child,{{0,8,Fr(0)}}});s.input.public_values.push_back({0,Fr(1)});s.ranges.push_back({0,"p0a/range",9});
 Fs lp("p0a-lookup"),lv("p0a-lookup");auto lookup=proveLookupBatch(s,w,{w},lp);verifyLookupBatch(s,lookup,lv);check(lp.digest()==lv.digest(),"small untrusted lookup proof valid");auto sw=serializeLookup(s),pw=serializeLookup(s,lookup);auto parsed=parseLookupSpec(sw);auto decoded=parseLookupProof(parsed,pw);Fs dv("p0a-lookup");verifyLookupBatch(parsed,decoded,dv);check(dv.digest()==lp.digest(),"external statement/proof accepted");
 auto broken_s=sw;corruptGroup(broken_s,s.input.master.rows[0]);rejects([&]{parseLookupSpec(broken_s);},"external statement noncanonical group rejected");auto broken_p=pw;corruptGroup(broken_p,lookup.ranges[0].digits[0].rows[0]);rejects([&]{parseLookupProof(s,broken_p);},"external proof noncanonical group rejected");
 // Explicit parser/native representation boundaries required by P0-A.
 Bytes signed_zero(32,0);signed_zero.back()=128;compareDecode(signed_zero);
 auto nonstandard=infinity;nonstandard.x=4;nonstandard.y=8;nonstandard.z=0;
 check(encode(nonstandard)==strictEncode(nonstandard)&&encode(nonstandard)==Bytes(32,0),"legacy-compatible nonstandard Jacobian infinity");
 auto invalid_infinity=infinity;invalid_infinity.x=1;invalid_infinity.y=0;
 rejects([&]{encode(invalid_infinity);},"invalid Jacobian infinity equation rejected");
 auto projective=point(7);Fp z=3;projective.x*=z*z;projective.y*=z*z*z;projective.z=z;
 check(encode(projective)==strictEncode(point(7)),"nonnormalized Jacobian encoding preserved");
 auto invalid_field=point(7);using Unit=std::remove_cv_t<std::remove_pointer_t<decltype(invalid_field.x.getUnit())>>;
 auto units=const_cast<Unit*>(invalid_field.x.getUnit());for(size_t i=0;i<Fp::getUnitSize();++i)units[i]=~Unit(0);
 check(!invalid_field.x.isValid(),"malformed internal field fixture");rejects([&]{encode(invalid_field);},"malformed internal coordinate rejected");
 // Reviewer counterexample: specialB dispatch must not claim the b=2 curve.
 const int saved_special_b=G1::specialB_;G1::specialB_=mcl::ec::local::Plus4;
 G1 order_three;order_three.x=0;order_three.y=2;order_three.z=1;
 rejects([&]{strictEncode(order_three);},"legacy rejects alternate-curve order-three fixture");
 check(!isValidG1(order_three),"P0A stale specialB dispatch accepted order-three point");
 rejects([&]{encode(order_three);},"stale specialB encoding rejected");G1::specialB_=saved_special_b;
 const int saved_special_a=G1::specialA_;G1::specialA_=mcl::ec::local::Minus3;
 {CountOrder count;try{encode(point(7));}catch(const std::exception&){}check(order_calls>0,"stale specialA uses strict order path");}G1::specialA_=saved_special_a;
 check(encode(point(7))==good,"special dispatch restoration preserves point bytes");
 // Actual alternate supported curve; its G1 still executes the strict order path.
 mcl::bn::initPairing(mcl::BN_SNARK1);auto other=point(5);auto expected=strictEncode(other);{CountOrder count;check(encode(other)==expected,"alternate curve canonical preserved");check(order_calls>0,"alternate curve keeps explicit order validation");}
 // This build's G1 field capacity is 256 bits: BLS12_381 cannot initialize.
 bool bls_rejected=false;try{mcl::bn::initPairing(mcl::BLS12_381);}catch(const std::exception&){bls_rejected=true;}check(bls_rejected,"BLS12 excluded by actual 256-bit build");init();
 // G2 contains non-subgroup on-curve points and is deliberately outside this API.
 bool g2_found=false;for(unsigned i=1;i<64&&!g2_found;++i){mcl::bn::Fp2 x,y2,y;x.a=i;x.b=1;G2::getWeierstrass(y2,x);if(mcl::bn::Fp2::squareRoot(y,y2)){G2 q;bool ok=false;q.set(&ok,x,y,false);if(ok&&!q.isValidOrder()){g2_found=true;check(!q.isValidOrder(),"G2 non-subgroup rejection remains active");}}}check(g2_found,"G2 non-subgroup fixture exists");
 // Guard must inspect current coefficients, not remember a previous init().
 const auto old_b=G1::b_;G1::b_=Fp(3);G1 changed;bool found=false;for(unsigned i=1;i<64&&!found;++i){Fp x=i,y2,y;G1::getWeierstrass(y2,x);if(Fp::squareRoot(y,y2)){changed.set(&found,x,y,false);}}
 check(found,"alternate equation fixture");{CountOrder count;bool accepted=true;try{encode(changed);}catch(const std::exception&){accepted=false;}(void)accepted;check(order_calls>0,"changed equation must keep order check");}G1::b_=old_b;
 p=point(19);unsigned encode_orders=0,decode_orders=0,shape_orders=0;{CountOrder count;encode(p);encode_orders=order_calls;}{auto b=strictEncode(p);CountOrder count;decodeG1(b);decode_orders=order_calls;}{CountOrder count;Fs t("shape-count");append(t,c);shape_orders=order_calls;}
 std::cout<<"CHECK_COUNTS encode_order="<<encode_orders<<" decode_order="<<decode_orders<<" shape_and_encoding_order="<<shape_orders<<'\n';
 check(encode_orders==0&&decode_orders==0&&shape_orders==0,"P0A redundant BN254 order multiplications still executed");
 std::cout<<"PASS P0A G1 "<<checks<<" checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
