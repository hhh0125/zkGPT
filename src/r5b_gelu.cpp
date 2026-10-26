#include "neuralNetwork.hpp"
#include "r5b_numeric.hpp"
#include "utils.hpp"
using namespace zkgpt::r5b;
namespace hs=host_safety;
void neuralNetwork::r5bGelu(layer *c,i64 &id,int real){
 const int count=len*channel_out,o=r5bAllocate(hs::checkedSize(10,hs::mul(9,count)));
 const int y=o+10,u=y+count,sign=u+count,t=sign+count,branch=t+count,tu=branch+count,u2=tu+count,u3=u2+count,r=u3+count;
 auto instance=gelu_witness_instance++;
 auto range=[&](const char*label,int at,unsigned bits,bool signed_value=false){witness_registry.addRange(WitnessKind::GELU,"gelu_"+std::to_string(instance)+"_"+label,at,count,bits,signed_value);};
 range("y",y,64,true);range("abs",u,64);range("sign",sign,1);range("t",t,1);range("branch",branch,64);range("tu",tu,32);range("tu2",u2,32);range("tu3",u3,32);range("remainder",r,24);
 if(!structure_only)for(int i=0;i<len;++i)for(int j=0;j<real;++j){int k=i*channel_out+j;I q=convert(val[0][q_offset+k]),abs=q<0?hs::neg(q):q,inside=abs<=246,selected=inside?abs:0,square=hs::mul(selected,abs),cube=hs::mul(square,abs);
  I n=hs::add(hs::sub(hs::mul(4194304,hs::add(q,abs)),hs::mul(75,cube)),hs::sub(hs::mul(34720,square),hs::mul(4002420,selected))),yy=zkgpt::r5b::round(n,8388608);
  val[0][y+k]=hs::field(yy);val[0][u+k]=hs::field(abs);val[0][sign+k]=q<0?1:0;val[0][t+k]=hs::field(inside);val[0][branch+k]=hs::field(inside?hs::sub(246,abs):hs::sub(abs,247));
  val[0][tu+k]=hs::field(selected);val[0][u2+k]=hs::field(square);val[0][u3+k]=hs::field(cube);val[0][r+k]=hs::field(remainder(n,8388608,yy));
 }
 auto start=[&](layer &cc,int n,layerType ty){initLayer(cc,n,ty);cc.need_phase2=true;cc.zero_start_id=0;};
 start(c[0],8*count,layerType::GELU_1);Gates g{c[0],o};
 for(int i=0;i<len;++i)for(int j=0;j<channel_out;++j){int k=i*channel_out+j;
  if(j>=real){for(int a:{u,sign,t,branch,tu,u2,u3,r})g.u(g.next(),a+k,1);continue;}
  int row=g.next();g.u(row,u+k,1);g.u(row,q_offset+k,-1);g.b(row,sign+k,q_offset+k,2);
  row=g.next();g.u(row,u+k,1);g.constant(row,-247);g.u(row,t+k,493);g.b(row,t+k,u+k,-2);g.u(row,branch+k,-1);
  row=g.next();g.b(row,t+k,u+k);g.u(row,tu+k,-1);
 }
 r5bFinish(c[0],id);
 start(c[1],2*count,layerType::GELU_2);Gates g2{c[1],o};
 for(int i=0;i<len;++i)for(int j=0;j<real;++j){int k=i*channel_out+j,row=g2.next();g2.b(row,tu+k,u+k);g2.u(row,u2+k,-1);row=g2.next();g2.b(row,u2+k,u+k);g2.u(row,u3+k,-1);}
 r5bFinish(c[1],id);
 start(c[2],count,layerType::GELU_3);Gates g3{c[2],o};
 for(int i=0;i<len;++i)for(int j=0;j<channel_out;++j){int k=i*channel_out+j,row=g3.next();if(j>=real){g3.u(row,y+k,1);continue;}
  g3.u(row,q_offset+k,8388608);g3.u(row,u+k,8388608);g3.u(row,u3+k,-150);g3.u(row,u2+k,69440);g3.u(row,tu+k,-8004840);g3.constant(row,8388608);g3.u(row,y+k,-16777216);g3.u(row,r+k,-1);
 }
 q_offset=y;r5bFinish(c[2],id);
}
