#include "neuralNetwork.hpp"
#include "r5b_numeric.hpp"
#include "utils.hpp"
using namespace zkgpt::r5b;
namespace hs=host_safety;
namespace {
void start(layer &c,int n,layerType ty){initLayer(c,n,ty);c.need_phase2=true;c.zero_start_id=0;}
std::string name(const char*op,std::size_t id,const char*value){return std::string(op)+"_"+std::to_string(id)+"_"+value;}
}
int neuralNetwork::r5bAllocate(int count){
 int base=hs::checkedSize(r5bMasterSize());
 if(r6Metadata()){r6_master_size_=hs::checkedSize(base,count);total_relu_in_size=hs::checkedSize(total_relu_in_size,count);return base;}
 val[0].resize(hs::checkedSize(val[0].size(),count));
 for(int i=base;i<base+count;++i)val[0][i].clear();
 total_relu_in_size=hs::checkedSize(total_relu_in_size,count);val[0][base]=1;return base;
}
void neuralNetwork::r5bFinish(layer &c,i64 &id){
 c.need_phase2=!c.bin_gates.empty();
 calcNormalLayer(c,id);
 if(!structure_only)for(auto &v:val[id])if(!v.isZero())throw std::logic_error("R5B numerical GKR equality failed at layer "+std::to_string(id));
 ++id;
}
void neuralNetwork::r5bRound(layer &c,i64 &id,int bias){
 const int n=len*channel_out,o=r5bAllocate(hs::checkedSize(20,hs::mul(4,n)));
 const int y=o+20,a=y+n,r=a+n,rc=r+n;
 start(c,4*n,layerType::RELU);Gates g{c,o};
 for(int i=0;i<n;++i){
  if(!structure_only){
   I aa=convert(val[id-1][i]);if(bias>=0)aa=hs::add(aa,convert(val[0][bias+i%channel_out]));
   I q=zkgpt::r5b::round(aa,1024),rr=remainder(aa,1024,q);
   val[0][a+i]=hs::field(aa);val[0][y+i]=hs::field(q);val[0][r+i]=hs::field(rr);val[0][rc+i]=hs::field(hs::sub(2047,rr));
  }
  int row=g.next();g.u(row,i,1,id-1);if(bias>=0)g.u(row,bias+i%channel_out,1);g.u(row,a+i,-1);
  row=g.next();g.u(row,a+i,2);g.constant(row,1024);g.u(row,y+i,-2048);g.u(row,r+i,-1);
  row=g.next();g.u(row,r+i,1);g.u(row,rc+i,1);g.constant(row,-2047);
  // Existing Thaler FC verification authenticates two parent evaluation
  // points. Retain both prior-layer ports with an exact redundant identity:
  // p*p - p*A + p*bias = p*(p+bias-A) = 0.
  row=g.next();c.bin_gates.emplace_back(row,i,i,1,1);
  c.bin_gates.emplace_back(row,i,a+i,-1,2);
  if(bias>=0)c.bin_gates.emplace_back(row,i,bias+i%channel_out,1,2);
 }
 auto instance=rounding_witness_instance++;
 witness_registry.addRange(WitnessKind::ROUNDING,name("round",instance,"q"),y,n,64,true);
 witness_registry.addRange(WitnessKind::ROUNDING,name("round",instance,"affine"),a,n,112,true);
 witness_registry.addRange(WitnessKind::ROUNDING,name("round",instance,"remainder"),r,n,11,false);
 witness_registry.addRange(WitnessKind::ROUNDING,name("round",instance,"complement"),rc,n,11,false);
 q_offset=y;r5bFinish(c,id);
}
void neuralNetwork::r5bAttention(layer *c,i64 &id){
 const int rows=headnum*len,t=rows*(len+1)/2,h=rows*headdim,ys=len*channel_in;
 const int o=r5bAllocate(10+2*rows+ys+6*t+3*h);
 const int se=o+10,y=se+rows,m=y+ys,raw=m+rows,s=raw+t,ti=s+t,e=ti+t,z=e+t,sr=z+t,a=sr+t,r=a+h,rc=r+h;
 auto instance=softmax_witness_instance++;
 auto range=[&](const char*label,int at,int n,unsigned bits,bool sign=false){witness_registry.addRange(WitnessKind::SOFTMAX,name("softmax",instance,label),at,n,bits,sign);};
 range("sumE",se,rows,32);range("output",y,ys,64,true);range("pmax",m,rows,64,true);
 range("raw",raw,t,96,true);range("score",s,t,64,true);range("t",ti,t,32);range("E",e,t,32);range("maximum_selector",z,t,1);range("scale_remainder",sr,t,13);
 range("weighted",a,h,96,true);range("division_remainder",r,h,64);range("division_complement",rc,h,64);
 witness_registry.addExpQueries(instance,ti,e,t);
 // Existing production table and tail policy, unchanged.
 if(!structure_only){
  compute_e_table();
  for(int head=0;head<headnum;++head)for(int i=0;i<len;++i){
   int row=head*len+i,base=head*len*(len+1)/2+i*(i+1)/2;I maximum=0;
   for(int j=0;j<=i;++j){int k=base+j;I d=convert(val[id-1][k]),scaled=zkgpt::r5b::round(hs::mul(25,d),4096);val[0][raw+k]=hs::field(d);val[0][s+k]=hs::field(scaled);val[0][sr+k]=hs::field(remainder(hs::mul(25,d),4096,scaled));if(!j || scaled>maximum)maximum=scaled;}
   val[0][m+row]=hs::field(maximum);bool selected=false;I sum=0;
   for(int j=0;j<=i;++j){int k=base+j;I gap=hs::sub(maximum,convert(val[0][s+k]));if(gap<0 || !fitsUnsignedBits(gap,32))throw std::range_error("R5B Exp gap outside u32");int ev=gap<EXP_TABLE_SIZE?table[std::size_t(gap)]:1;
    val[0][ti+k]=hs::field(gap);val[0][e+k]=ev;sum=hs::add(sum,ev);
    if(!selected && gap==0){val[0][z+k]=1;selected=true;}
   }
   if(sum<=0)throw std::range_error("R5B nonpositive sumE");val[0][se+row]=hs::field(sum);
   for(int col=0;col<headdim;++col){I weighted=0;for(int j=0;j<=i;++j){int v=q_offset+j*channel_out+2*headnum*headdim+head*headdim+col;weighted=hs::add(weighted,hs::mul(convert(val[0][v]),convert(val[0][e+base+j])));}
    int k=row*headdim+col,yi=y+i*channel_in+head*headdim+col;I q=zkgpt::r5b::round(weighted,sum),rr=remainder(weighted,sum,q);
    val[0][a+k]=hs::field(weighted);val[0][yi]=hs::field(q);val[0][r+k]=hs::field(rr);val[0][rc+k]=hs::field(hs::sub(hs::sub(hs::mul(2,sum),1),rr));
   }
  }
 }
 start(c[0],3*t+2*rows,layerType::SOFTMAX_1);Gates g{c[0],o};
 for(int k=0;k<t;++k){int row=g.next();g.u(row,k,1,id-1);g.u(row,raw+k,-1);
  row=g.next();g.u(row,raw+k,50);g.constant(row,4096);g.u(row,s+k,-8192);g.u(row,sr+k,-1);
 }
 for(int head=0;head<headnum;++head)for(int i=0;i<len;++i){int ri=head*len+i,base=head*len*(len+1)/2+i*(i+1)/2;int select=g.next(),gap=g.next();g.constant(select,-1);
  for(int j=0;j<=i;++j){int k=base+j,row=g.next();g.u(row,m+ri,1);g.u(row,s+k,-1);g.u(row,ti+k,-1);zkgpt::r5b::attained(c[0],select,gap,z+k,ti+k);}
 }
 r5bFinish(c[0],id);
 start(c[1],rows+h,layerType::SOFTMAX_2);Gates g2{c[1],o};
 for(int head=0;head<headnum;++head)for(int i=0;i<len;++i){int ri=head*len+i,base=head*len*(len+1)/2+i*(i+1)/2,row=g2.next();g2.u(row,se+ri,-1);
  for(int j=0;j<=i;++j)g2.u(row,e+base+j,1);
  for(int col=0;col<headdim;++col){int k=ri*headdim+col;row=g2.next();g2.u(row,a+k,-1);for(int j=0;j<=i;++j)g2.b(row,q_offset+j*channel_out+2*headnum*headdim+head*headdim+col,e+base+j);}
 }
 r5bFinish(c[1],id);
 start(c[2],2*h+ys-h,layerType::SOFTMAX_3);Gates g3{c[2],o};
 for(int head=0;head<headnum;++head)for(int i=0;i<len;++i)for(int col=0;col<headdim;++col){int ri=head*len+i,k=ri*headdim+col,yi=y+i*channel_in+head*headdim+col,row=g3.next();
  g3.u(row,a+k,2);g3.u(row,se+ri,1);g3.b(row,se+ri,yi,-2);g3.u(row,r+k,-1);
  row=g3.next();g3.u(row,r+k,1);g3.u(row,rc+k,1);g3.u(row,se+ri,-2);g3.constant(row,1);
 }
 for(int i=0;i<len;++i)for(int col=headnum*headdim;col<channel_in;++col)g3.u(g3.next(),y+i*channel_in+col,1);
 q_offset=y;r5bFinish(c[2],id);
}
