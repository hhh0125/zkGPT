#include "models.hpp"
#include "host_safety.hpp"
#include "paper_r1/core.hpp"
#include <iostream>
#include <limits>
#include "paper_r1/exp.hpp"
#include "r5b_numeric.hpp"
namespace r1=zkgpt::r1;
class R5BFixture:public LLM {
public:
 R5BFixture():LLM(1,12,64,768,3072){setR5BEngineeringCandidate();total_in_size=total_para_size=total_relu_in_size=total_ave_in_size=total_max_in_size=0;}
 bool valid(const prover&p,const std::vector<layer>&cs){
  for(const auto &r:getWitnessRegistry().constraints())for(std::size_t j=0;j<r.count;++j){auto v=convert(p.val[0][r.val0_offset+j]);if(!(r.is_signed?fitsSignedBits(v,r.bits):fitsUnsignedBits(v,r.bits)))return false;}
  for(std::size_t k=0;k<cs.size();++k){const auto &c=cs[k];std::vector<Fr> values(c.size,Fr(0));unsigned id=k+1;
   for(auto &g:c.uni_gates)values[g.g]+=p.val[g.lu][g.u]*host_safety::field(g.sc);
   for(auto &g:c.bin_gates)values[g.g]+=p.val[g.getLayerIdU(id)][g.u]*p.val[g.getLayerIdV(id)][g.v]*host_safety::field(g.sc);
   for(auto &v:values)if(!v.isZero())return false;
  }return true;
 }
 void roundBoundaries(){
  const __int128 wide=__int128(1)<<70;std::vector<__int128> input={-1537,-1536,-1535,-513,-512,-511,-1,0,1,511,512,513,1535,1536,1537,wide+512,-wide-512};
  prover p;SIZE=3;len=1;channel_out=input.size();p.val.resize(3);val=p.val.begin();val[0].assign(1,Fr(0));for(auto i:input)val[1].push_back(host_safety::field(i));layer c;i64 id=2;roundLayer(c,id,1.0f/1024);
  for(std::size_t i=0;i<input.size();++i){__int128 a=input[i],q=a/1024,rem=a%1024;if(rem<0){--q;rem+=1024;}if(rem>=512)++q;r1::require(convert(val[0][q_offset+i])==q,"exact FC boundary mismatch");}
  validateWitnessRegistry(val[0]);
  // Alternate result at a negative half-tie, with consistently updated remainder,
  // must fail the actual registered bound even though the legacy interval allowed it.
  auto &region=getWitnessRegistry().constraints();const auto at=region[2].val0_offset;
  val[0][q_offset+4]=-1;val[0][at+4]=2048;
  r1::require(!fitsUnsignedBits(convert(val[0][at+4]),region[2].bits),"alternate tie admitted");
 }
 void attentionBoundaries(){
  prover p;SIZE=5;len=4;headnum=headdim=channel_in=1;channel_out=3;q_offset=0;p.val.resize(5);val=p.val.begin();val[0].assign(12,Fr(0));
  std::vector<long long>d={-2048,-100,-50,0,0,0,100,50,2048,-2048};for(auto x:d)val[1].push_back(Fr(x));
  for(int i=0;i<4;++i)val[0][3*i+2]=10*(i+1)-25;
  std::vector<layer> cs(3);i64 id=2;r5bAttention(cs.data(),id);r1::require(valid(p,cs),"honest Attention production constraints");
  auto at=[&](const char*s){for(auto&r:getWitnessRegistry().constraints())if(r.name==std::string("softmax_0_")+s)return r.val0_offset;throw std::runtime_error("missing test region");};
  auto rr=[](long long n){long long q=n/4096,r=n%4096;if(r<0){--q;r+=4096;}return q+(r>=2048);};
  for(int i=0;i<4;++i){int base=i*(i+1)/2;long long maximum=rr(25*d[base]);for(int j=0;j<=i;++j)maximum=std::max(maximum,rr(25*d[base+j]));long long sum=0,weighted=0;
   for(int j=0;j<=i;++j){auto gap=maximum-rr(25*d[base+j]);r1::require(convert(val[0][at("t")+base+j])==gap,"scaled max/index exact reference");auto e=r1::expValue(std::uint32_t(gap),r1::ExpProfile::LegacyClampOne);r1::require(val[0][at("E")+base+j]==Fr(e),"Exp identity");sum+=e;weighted+=static_cast<long long>(e)*(10*(j+1)-25);}
   long long q=weighted/sum,r=weighted%sum;if(r<0){--q;r+=sum;}q+=r>=sum/2+sum%2;r1::require(convert(val[0][at("output")+i])==q,"exact normalization reference");
  }
  auto original=val[0];val[0][at("pmax")+1]+=1;for(int j=1;j<=2;++j)val[0][at("t")+j]+=1;r1::require(!valid(p,cs),"unattained upper bound accepted");val[0]=original;
  val[0][at("maximum_selector")]=2;r1::require(!valid(p,cs),"nonboolean selector accepted");val[0]=original;
  val[0][at("maximum_selector")]=0;r1::require(!valid(p,cs),"missing selector accepted");val[0]=original;
  val[0][at("score")]=Fr(-13);val[0][at("scale_remainder")]=Fr(8192);r1::require(!valid(p,cs),"negative half tie alternate admitted");val[0]=original;
  val[0][at("division_remainder")]+=1;r1::require(!valid(p,cs),"unbound normalization remainder accepted");
 }
 void lnBoundaries(){
  for(int mode=0;mode<5;++mode){
   R5BFixture f;prover p;f.SIZE=4;f.len=1;f.channel_in=1024;f.q_offset=0;p.val.resize(4);f.val=p.val.begin();p.val[0].assign(3072,Fr(0));
   for(int j=0;j<768;++j){p.val[0][j]=mode==0?42:mode==1?(j%2?4000:-4000):j%7-3;p.val[0][1024+j]=(mode>=3?0:1024);p.val[0][2048+j]=(mode==3?5:mode==4?-5:0);}
   f.layer_norm_w_q_start[0]=1024;f.layer_norm_b_q_start[0]=2048;std::vector<layer>cs(3);i64 id=1;f.r5bLayerNorm(cs.data(),id,0,768);
   if(!f.valid(p,cs)){std::cerr<<"LN case "<<mode<<"\n";f.validateWitnessRegistry(p.val[0]);throw std::runtime_error("LN production equality failure");}
   auto at=[&](const char*s){for(auto&r:f.getWitnessRegistry().constraints())if(r.name==std::string("layer_norm_0_")+s)return r.val0_offset;throw std::runtime_error("missing LN test region");};
   __int128 sum=0,H=0;for(int j=0;j<768;++j)sum+=convert(p.val[0][j]);for(int j=0;j<768;++j){__int128 a=768*convert(p.val[0][j])-sum;H+=a*a;}
   __int128 B=78125*H+3710851743744LL,D=60000000*B;
   __int128 root=static_cast<__int128>(std::sqrt(static_cast<long double>(D)));while(root*root>D)--root;while((root+1)*(root+1)<=D)++root;
   r1::require(convert(p.val[0][at("B")])==B && convert(p.val[0][at("D")])==D && convert(p.val[0][at("sigma")])==root,"LN exact variance/rational/root reference");
   for(int j=0;j<768;++j){__int128 a=768*convert(p.val[0][j])-sum,N=60000000*convert(p.val[0][1024+j])*a+convert(p.val[0][2048+j])*root,den=10*root,q=convert(p.val[0][at("y")+j]);__int128 r=2*N+den-2*den*q;r1::require(r>=0 && r<2*den,"LN unique exact half-up reference");if(mode==0 || mode==4)r1::require(q==0,"LN zero variance or negative half tie");if(mode==3)r1::require(q==1,"LN positive half tie");}
   for(int j=768;j<1024;++j)r1::require(p.val[0][at("y")+j].isZero(),"LN output padding not zero");
   auto original=p.val[0];p.val[0][at("B")]+=1;r1::require(!f.valid(p,cs),"wrong epsilon/variance accepted");p.val[0]=original;
   p.val[0][at("sigma")]=0;r1::require(!f.valid(p,cs),"zero sigma accepted");p.val[0]=original;
   p.val[0][at("root_lower")]+=1;r1::require(!f.valid(p,cs),"unbound root interval accepted");p.val[0]=original;
   p.val[0][at("y")+768]=1;r1::require(!f.valid(p,cs),"nonzero internal padding accepted");
  }
  // The declared D<2^112/sigma<2^56 domain is enforced; extreme values are
  // rejected rather than silently replacing epsilon or using float overflow.
  bool rejected=false;try{R5BFixture f;prover p;f.SIZE=4;f.len=1;f.channel_in=1024;f.q_offset=0;p.val.resize(4);f.val=p.val.begin();p.val[0].assign(3072,Fr(0));p.val[0][0]=400000001;f.layer_norm_w_q_start[0]=1024;f.layer_norm_b_q_start[0]=2048;layer c[3];i64 id=1;f.r5bLayerNorm(c,id,0,768);f.validateWitnessRegistry(p.val[0]);}catch(const std::exception&){rejected=true;}r1::require(rejected,"out-of-domain LN extreme accepted");
 }
 __int128 lnZeroVariance(){prover p;SIZE=4;len=1;channel_in=1024;q_offset=0;p.val.resize(4);val=p.val.begin();val[0].assign(3072,Fr(0));for(int i=0;i<768;++i)val[0][1024+i]=1024;
  layer_norm_w_c[0]=layer_norm_b_c[0]=1;layer_norm_w_e[0]=layer_norm_b_e[0]=-10;layer_norm_w_q_start[0]=1024;layer_norm_b_q_start[0]=2048;
  bool mask[1024];for(int i=0;i<1024;++i)mask[i]=i<768;i64 id=1;layer c[3];
  r5bLayerNorm(c,id,0,768);for(auto&r:getWitnessRegistry().constraints())if(r.name=="layer_norm_0_B")return convert(val[0][r.val0_offset]);throw std::runtime_error("missing B");}
 void geluBoundaries(){
  prover p;SIZE=4;len=1;channel_out=2048;q_offset=0;p.val.resize(4);val=p.val.begin();val[0].assign(2048,Fr(0));int n=0;
  for(int i=-600;i<=600;++i)val[0][n++]=i;
  val[0][n++]=Fr(std::numeric_limits<std::int64_t>::min());val[0][n++]=Fr(std::numeric_limits<std::int64_t>::max());
  std::vector<layer>cs(3);i64 id=1;r5bGelu(cs.data(),id,n);r1::require(valid(p,cs),"GELU honest production constraints");
  auto at=[&](const char*s){for(auto&r:getWitnessRegistry().constraints())if(r.name==std::string("gelu_0_")+s)return r.val0_offset;throw std::runtime_error("missing GELU region");};
  for(int i=0;i<n;++i){__int128 q=convert(val[0][i]),u=q<0?-q:q,t=u<=246,N=4194304*(q+u);if(t)N-=u*(75*u*u-34720*u+4002420);__int128 y=convert(val[0][at("y")+i]),r=2*N+8388608-16777216*y;
   r1::require(r>=0 && r<16777216,"GELU exact rational reference");r1::require(convert(val[0][at("t")+i])==t,"unique threshold reference");if(!t)r1::require(y==(q>0?q:0),"shared-unit tail slope");
  }
  for(int i=n;i<2048;++i)r1::require(val[0][at("y")+i].isZero(),"GELU padding");
  // P(246)=0 exactly: the continuous real extension has no threshold jump.
  r1::require(75*246*246-34720*246+4002420==0,"continuous threshold polynomial");
  auto orig=val[0];for(int u:{246,247}){int k=600+u;val[0][at("t")+k]=u==246?0:1;val[0][at("branch")+k]=-1;r1::require(!valid(p,cs),"alternate threshold branch accepted");val[0]=orig;}
  val[0][at("abs")]+=1;r1::require(!valid(p,cs),"unbound abs accepted");val[0]=orig;
  val[0][at("tu3")+600+100]+=1;r1::require(!valid(p,cs),"unbound cubic accepted");val[0]=orig;
  val[0][at("y")+n]=1;r1::require(!valid(p,cs),"nonzero GELU padding accepted");
 }
 long long geluTail(){prover p;SIZE=4;len=channel_out=1;q_offset=0;p.val.resize(4);val=p.val.begin();val[0].assign(1,Fr(256));i64 id=1;layer c[3];
  r5bGelu(c,id,1);for(auto&r:getWitnessRegistry().constraints())if(r.name=="gelu_0_y")return (long long)convert(val[0][r.val0_offset]);throw std::runtime_error("missing y");}
 long long roundNegativeTie(){prover p;SIZE=3;len=channel_out=1;p.val.resize(3);val=p.val.begin();val[0].assign(1,Fr(0));val[1].assign(1,Fr(-512));layer c;i64 id=2;roundLayer(c,id,1.0f/1024);return (long long)convert(val[0][q_offset]);}
 long long scaledGap(){prover p;SIZE=5;len=2;headnum=headdim=channel_in=1;channel_out=3;q_offset=0;p.val.resize(5);val=p.val.begin();val[0].assign(6,Fr(0));val[1].assign({Fr(0),Fr(100),Fr(50)});layer c[3];i64 id=2;r5bAttention(c,id);for(auto &r:getWitnessRegistry().constraints())if(r.name=="softmax_0_t")return (long long)convert(val[0][r.val0_offset+2]);throw std::runtime_error("missing t");}
};
int main(int argc,char**argv){try{r1::init();r1::require(argc==2,"mode");R5BFixture f;
 if(std::string(argv[1])=="round")r1::require(f.roundNegativeTie()==0,"R5B half-up -0.5 must be zero, not legacy away -1");
 else if(std::string(argv[1])=="attention")r1::require(f.scaledGap()==1,"R5B scaled-score 100/50 gap must be 1");
 else if(std::string(argv[1])=="round-boundaries")f.roundBoundaries();
 else if(std::string(argv[1])=="attention-boundaries")f.attentionBoundaries();
 else if(std::string(argv[1])=="ln-boundaries")f.lnBoundaries();
 else if(std::string(argv[1])=="ln")r1::require(f.lnZeroVariance()==__int128(3710851743744LL),"R5B exact epsilon rational numerator missing");
 else if(std::string(argv[1])=="gelu-boundaries")f.geluBoundaries();
 else if(std::string(argv[1])=="gelu")r1::require(f.geluTail()==256,"R5B shared-unit positive GELU tail must have slope one");
 else if(std::string(argv[1])=="arithmetic"){
  using namespace zkgpt::r5b;const I lo=-(__int128(1)<<126)-(__int128(1)<<126),hi=-(lo+1);
  r1::require(round(lo,1)==lo && round(hi,1)==hi && round(-5,2)==-2 && round(5,2)==3 && round(-7,3)==-2 && round(-1,3)==0,"exact signed rounding extremes");
  for(I v:{I(1),I(2),I(10),I(123456789),I(1)<<48,I(1)<<63}){I square=host_safety::mul(v,v);r1::require(root(square)==v && root(square-1)==v-1,"floor integer root boundary");}
  for(int mode=0;mode<2;++mode){bool rejected=false;try{if(mode)root(-1);else round(1,0);}catch(const std::exception&){rejected=true;}r1::require(rejected,"invalid exact arithmetic accepted");}
 }
 else if(std::string(argv[1])=="public-layout"){LLM nn(1,12,64,768,3072);nn.setR5BEngineeringCandidate();auto c=nn.buildPublicCircuit();for(std::size_t i=1;i+1<c.size;++i)if(c.circuit[i].ty==layerType::FCONN){const auto&p=c.circuit[i+1];r1::require(p.need_phase2 && p.bit_length_u[1]>=c.circuit[i].bit_length && p.bit_length_v[1]>=c.circuit[i].bit_length,"FC parent lost one evaluation point");}for(auto &l:c.circuit)if(l.ty!=layerType::FCONN && l.need_phase2)r1::require(l.max_bl_v>=0 && !l.bin_gates.empty(),"unary R5B layer must not request an absent GKR phase-2 port");}
 else throw std::invalid_argument("mode");std::cout<<"PASS "<<argv[1]<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
