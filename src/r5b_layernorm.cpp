#include "neuralNetwork.hpp"
#include "r5b_numeric.hpp"
#include "utils.hpp"
using namespace zkgpt::r5b;
namespace hs=host_safety;
void neuralNetwork::r5bLayerNorm(layer *c,i64 &id,int ln_id,int real){
 if(real!=768 || channel_in!=1024)throw std::invalid_argument("R5B LN requires 768 real/1024 padded channels");
 const int count=len*channel_in,o=r5bAllocate(10+4*count+6*len);
 const int y=o+10,a=y+count,r=a+count,rc=r+count,sum=rc+count,B=sum+len,D=B+len,sigma=D+len,lo=sigma+len,hi=lo+len;
 const int gamma=layer_norm_w_q_start[ln_id],beta=layer_norm_b_q_start[ln_id];
 auto instance=layer_norm_witness_instance++;
 auto range=[&](const char*label,int at,int n,unsigned bits,bool sign=false){witness_registry.addRange(WitnessKind::LAYER_NORM,"layer_norm_"+std::to_string(instance)+"_"+label,at,n,bits,sign);};
 range("y",y,count,64,true);range("centered",a,count,64,true);range("remainder",r,count,64);range("complement",rc,count,64);
 range("sum",sum,len,80,true);range("B",B,len,96);range("D",D,len,112);range("sigma",sigma,len,56);range("root_lower",lo,len,64);range("root_upper",hi,len,64);
 if(!structure_only)for(int i=0;i<len;++i){
  I s=0,H=0;for(int j=0;j<real;++j)s=hs::add(s,convert(val[0][q_offset+i*channel_in+j]));
  val[0][sum+i]=hs::field(s);
  for(int j=0;j<real;++j){int k=i*channel_in+j;I aa=hs::sub(hs::mul(768,convert(val[0][q_offset+k])),s);val[0][a+k]=hs::field(aa);H=hs::add(H,hs::mul(aa,aa));}
  I bb=hs::add(hs::mul(78125,H),3710851743744LL),dd=hs::mul(60000000,bb),ss=root(dd);
  if(ss<=0)throw std::range_error("R5B LN nonpositive root");
  val[0][B+i]=hs::field(bb);val[0][D+i]=hs::field(dd);val[0][sigma+i]=hs::field(ss);
  val[0][lo+i]=hs::field(hs::sub(dd,hs::mul(ss,ss)));
  val[0][hi+i]=hs::field(hs::sub(hs::sub(hs::mul(hs::add(ss,1),hs::add(ss,1)),1),dd));
  for(int j=0;j<real;++j){int k=i*channel_in+j;I n=hs::add(hs::mul(hs::mul(60000000,convert(val[0][gamma+j])),convert(val[0][a+k])),hs::mul(convert(val[0][beta+j]),ss));
   I denominator=hs::mul(10,ss),q=zkgpt::r5b::round(n,denominator),rr=remainder(n,denominator,q);
   val[0][y+k]=hs::field(q);val[0][r+k]=hs::field(rr);val[0][rc+k]=hs::field(hs::sub(hs::sub(hs::mul(2,denominator),1),rr));
  }
 }
 auto start=[&](layer &cc,int n,layerType ty){initLayer(cc,n,ty);cc.need_phase2=true;cc.zero_start_id=0;};
 start(c[0],len*(2+real+channel_in-real),layerType::LAYER_NORM_1);Gates g{c[0],o};
 for(int i=0;i<len;++i){int sums=g.next(),bcheck=g.next();g.u(sums,sum+i,-1);g.u(bcheck,B+i,-1);g.constant(bcheck,3710851743744LL);
  for(int j=0;j<real;++j){int k=i*channel_in+j,row=g.next();g.u(sums,q_offset+k,1);g.u(row,q_offset+k,768);g.u(row,sum+i,-1);g.u(row,a+k,-1);g.b(bcheck,a+k,a+k,78125);}
  for(int j=real;j<channel_in;++j)g.u(g.next(),q_offset+i*channel_in+j,1);
 }
 r5bFinish(c[0],id);
 start(c[1],3*len,layerType::LAYER_NORM_2);Gates g2{c[1],o};
 for(int i=0;i<len;++i){int row=g2.next();g2.u(row,B+i,60000000);g2.u(row,D+i,-1);
  row=g2.next();g2.u(row,D+i,1);g2.b(row,sigma+i,sigma+i,-1);g2.u(row,lo+i,-1);
  row=g2.next();g2.b(row,sigma+i,sigma+i,1);g2.u(row,sigma+i,2);g2.u(row,D+i,-1);g2.u(row,hi+i,-1);
 }
 r5bFinish(c[1],id);
 start(c[2],2*count,layerType::LAYER_NORM_3);Gates g3{c[2],o};
 for(int i=0;i<len;++i)for(int j=0;j<channel_in;++j){int k=i*channel_in+j,row=g3.next();
  if(j>=real){g3.u(row,y+k,1);g3.u(g3.next(),a+k,1);continue;}
  g3.b(row,gamma+j,a+k,120000000);g3.b(row,beta+j,sigma+i,2);g3.u(row,sigma+i,10);g3.b(row,y+k,sigma+i,-20);g3.u(row,r+k,-1);
  row=g3.next();g3.u(row,r+k,1);g3.u(row,rc+k,1);g3.u(row,sigma+i,-20);g3.constant(row,1);
 }
 q_offset=y;r5bFinish(c[2],id);
}
