#include "production_layout.hpp"
#include <cybozu/sha2.hpp>
#include <fstream>
#include <limits>
#include <map>
#include <set>

namespace route_b::production {
void require(bool b,const std::string &m) {if(!b) throw std::runtime_error(m);}
std::size_t paddedSize(std::size_t n) {
  require(n>0,"empty source");std::size_t p=1;
  while(p<n) {require(p<=std::numeric_limits<std::size_t>::max()/2,"size overflow");p*=2;}return p;
}
std::size_t dimensions(std::size_t p) {require(p && !(p&(p-1)),"non-power-of-two");std::size_t d=0;while(p>1){p/=2;++d;}return d;}
std::string hexDigest(const BindingDigest &d) {std::string s;for(auto x:d){s+="0123456789abcdef"[x>>4];s+="0123456789abcdef"[x&15];}return s;}
namespace {
std::vector<uint8_t> read(const std::string &path) {std::ifstream f(path,std::ios::binary);require(bool(f),"missing policy file: "+path);return {(std::istreambuf_iterator<char>(f)),{}};}
std::string sha(const std::vector<uint8_t> &v) {cybozu::Sha256 h;BindingDigest d;h.digest(d.data(),32,v.data(),v.size());return hexDigest(d);}
bool shapeOK(const WitnessShape&s){return s.sequence_length==32 && s.layer_count==12 && s.head_count==12 && s.head_dimension==64 && s.hidden_dimension==768 && s.mlp_dimension==3072;}
// Scalar append rules, independently checked against the actual registry and
// circuit. These do not create any witness values or read model parameters.
WitnessRegistry scalarLayout(PublicModelLayout &l) {
  WitnessRegistry r;r.setShape({32,12,12,64,768,3072});
  std::size_t pos=32768;
  for(unsigned f=0;f<48;++f) {
    const std::size_t ci=f%4==3?4096:1024,co=f%4==1||f%4==3?1024:4096;
    const std::size_t ri=f%4==3?3072:768,ro=f%4==0?2304:f%4==2?3072:768;
    l.parameters.push_back({"fc_"+std::to_string(f)+".weight",pos,ci*co,ro,ri});pos+=ci*co;
    l.parameters.push_back({"fc_"+std::to_string(f)+".bias",pos,co,ro,1});pos+=co;
  }
  auto add=[&](WitnessKind k,const std::string&prefix,unsigned id,const char *name,std::size_t off,std::size_t count,unsigned bits,bool sign=false){r.addRange(k,prefix+"_"+std::to_string(id)+"_"+name,off,count,bits,sign);};
  auto ln=[&](unsigned id) {
    l.parameters.push_back({"ln_"+std::to_string(id)+".weight",pos,1024,768,1});
    l.parameters.push_back({"ln_"+std::to_string(id)+".bias",pos+1024,1024,768,1});pos+=2048;
    l.constants.push_back(pos);const auto o=pos;pos+=10+2*32768+4*32+2*24576;
    add(WitnessKind::LAYER_NORM,"layer_norm",id,"y",o+10,32768,64,true);
    add(WitnessKind::LAYER_NORM,"layer_norm",id,"sum",o+10+32768,32,64,true);
    add(WitnessKind::LAYER_NORM,"layer_norm",id,"B",o+10+32768+32,32,64);
    add(WitnessKind::LAYER_NORM,"layer_norm",id,"sigma",o+10+32768+64,32,64);
    add(WitnessKind::LAYER_NORM,"layer_norm",id,"delta1",o+10+32768+96,32,64);
    add(WitnessKind::LAYER_NORM,"layer_norm",id,"delta2",o+10+32768+128,24576,126);
    add(WitnessKind::LAYER_NORM,"layer_norm",id,"term1",o+10+65536+128,24576,64);
    add(WitnessKind::LAYER_NORM,"layer_norm",id,"term2",o+10+65536+128+24576,24576,64);
  };
  auto round=[&](unsigned id,std::size_t count) {auto o=pos;l.constants.push_back(o);pos+=20+4*count;
    const char *names[]={"q","delta","term1","term2"};for(unsigned j=0;j<4;++j)add(WitnessKind::ROUNDING,"round",id,names[j],o+20+j*count,count,j==1?126:64,j==0);
  };
  for(unsigned b=0;b<12;++b) {
    ln(2*b);round(4*b,131072);
    auto o=pos;l.constants.push_back(o);
    constexpr std::size_t H=384,T=6336,J=24576,Y=32768,old=10+2*H+4*T+J+Y;
    const char *names[]={"sumE","pmax","delta1","delta2","t","E","delta3","output","lookup_term1","lookup_term2","division_term1","division_term2"};
    const std::size_t offsets[]={10,10+H,10+2*H,10+2*H+T,10+2*H+2*T,10+2*H+3*T,10+2*H+4*T,10+2*H+4*T+J,old,old+T,old+2*T,old+2*T+J};
    const std::size_t counts[]={H,H,T,T,T,T,J,Y,T,T,J,J};
    const unsigned bits[]={32,64,64,126,32,32,126,64,64,64,64,64};
    r.addExpQueries(b,o+offsets[4],o+offsets[5],T);
    for(unsigned j=0;j<12;++j)add(WitnessKind::SOFTMAX,"softmax",b,names[j],o+offsets[j],counts[j],bits[j],j==7);
    pos+=old+2*T+2*J;
    round(4*b+1,32768);pos+=32768;
    ln(2*b+1);round(4*b+2,131072);
    o=pos;l.constants.push_back(o);constexpr std::size_t B=131072,R=98304;
    const char *gn[]={"y","abs","t","delta1","delta2","delta3","term1","term2"};
    const std::size_t go[]={10,10+B,10+2*B,10+3*B,10+3*B+R,10+3*B+2*R,10+3*B+3*R,10+3*B+4*R};
    for(unsigned j=0;j<8;++j)add(WitnessKind::GELU,"gelu",b,gn[j],o+go[j],j<3?B:R,j==2?1:j==5?126:64,j==0);
    pos+=10+3*B+5*R;
    round(4*b+3,32768);l.output_offset=pos;pos+=32768;
  }
  l.n0=pos;l.N0=paddedSize(pos);l.ell=dimensions(l.N0);return r;
}
void compareRegistry(const WitnessRegistry&a,const WitnessRegistry&b) {
  const auto &x=a.constraints(),&y=b.constraints();require(x.size()==y.size(),"Range region count mismatch");
  for(std::size_t i=0;i<x.size();++i)require(x[i].kind==y[i].kind&&x[i].name==y[i].name&&x[i].val0_offset==y[i].val0_offset&&x[i].count==y[i].count&&x[i].bits==y[i].bits&&x[i].is_signed==y[i].is_signed,"Range region semantics/order mismatch: "+std::to_string(i));
  require(a.expQueries().size()==b.expQueries().size(),"Exp count mismatch");
  for(std::size_t i=0;i<a.expQueries().size();++i) {const auto &u=a.expQueries()[i],&v=b.expQueries()[i];require(u.operator_id==v.operator_id&&u.query_id==v.query_id&&u.t_witness_index==v.t_witness_index&&u.E_witness_index==v.E_witness_index,"Exp position/order mismatch");}
}
std::vector<Span> compress(const std::vector<u32>&v,std::size_t n0) {
  std::vector<bool> seen(n0);std::vector<Span> runs;
  for(auto x:v){require(x<n0,"ori_id out of bounds");require(!seen[x],"ori_id alias within port");seen[x]=true;if(!runs.empty()&&runs.back().offset+runs.back().count==x)++runs.back().count;else runs.push_back({x,1,0});}return runs;
}
// The mapped gate access order must introduce local indices 0,1,... exactly.
// This catches unused/extra, permuted and invalid subset entries without a hash.
void checkGateOrder(const layer &l,unsigned i) {
  std::size_t nu=0,nv=0;std::vector<bool> su(l.ori_id_u.size()),sv(l.ori_id_v.size());
  auto use=[&](std::size_t x,bool v){auto &s=v?sv:su;auto &n=v?nv:nu;require(x<s.size(),"gate subset index out of bounds");if(!s[x]){require(x==n++,"gate first-occurrence order mismatch");s[x]=true;}};
  for(auto &g:l.uni_gates)if(!g.lu)use(g.u,false);
  for(auto &g:l.bin_gates){if(!g.getLayerIdU(i))use(g.u,false);if(!g.getLayerIdV(i))use(g.v,true);}
  require(nu==su.size()&&nv==sv.size(),"ori_id contains unused entries");
}
}
TrustedDeployment TrustedDeployment::full12(Mode mode) {
  require(mode==Mode::BridgeV1||mode==Mode::LassoV2,"unapproved mode");TrustedDeployment d;d.mode=mode;
  d.policy_id="full12-reviewed-layout-v1";
  d.model_id="d3c62c91c331e828c1141440ac1822b3da3e26c3ba7d8c28cf4c42e50a127c65";
  d.implementation_profile="3965ecaa02a1bd52181cf3d5e6ec40ffb5b785e9ac3b51b876d15dd03d779ca8";
  d.anchor_sha256="464e722650b16c5205d6f0576a1059571a6160e032bd8ad30b4d618a22b31de3";
  const auto bytes=read("task9_logs_20261014/full12.anchor.bin");require(sha(bytes)==d.anchor_sha256,"unapproved anchor bytes");
  d.anchor=decodeModelAnchor(bytes);
  require(hexDigest(d.anchor.model_id)==d.model_id&&hexDigest(d.anchor.profile_id)==d.implementation_profile,"unapproved model/profile");
  require(hexDigest(quantizationImplementationProfile())==d.implementation_profile,"current implementation profile differs");return d;
}
PublicModelLayout rebuildProductionLayout(const TrustedDeployment&d,const layeredCircuit&C,const WitnessRegistry&R) {
  require(d.policy_id=="full12-reviewed-layout-v1" && (d.mode==Mode::BridgeV1||d.mode==Mode::LassoV2),"unapproved deployment");
  require(shapeOK(R.shape())&&shapeOK(d.anchor.layout.shape),"unapproved shape");
  require(hexDigest(d.anchor.model_id)==d.model_id&&hexDigest(d.anchor.profile_id)==d.implementation_profile,"deployment identity mismatch");
  require(C.size==277&&C.circuit.size()==277,"public circuit layer count mismatch");
  require(fingerprintCircuit(C)==d.anchor.circuit_fingerprint,"public circuit differs from approved anchor fingerprint");
  PublicModelLayout l;auto expected=scalarLayout(l);
  require(C.circuit[0].size==l.n0&&static_cast<std::size_t>(C.circuit[0].bit_length)==l.ell,"public circuit append layout mismatch");
  R.validateLayout(l.n0);compareRegistry(R,expected);l.ranges=planRangeLayout(R);l.exp=R.expQueries();
  require(d.anchor.parameters.size()==l.parameters.size(),"parameter region count mismatch");
  for(std::size_t i=0;i<l.parameters.size();++i){auto&a=l.parameters[i];auto&b=d.anchor.parameters[i];require(a.name==b.name&&a.offset==b.offset&&a.count==b.count&&a.rows==b.rows&&a.cols==b.cols,"parameter layout mismatch");}
  for(auto c:l.constants)for(auto &p:l.parameters)require(c<p.offset||c-p.offset>=p.count,"constant aliases model parameter");
  l.ports.resize(C.size*2);
  for(unsigned i=1;i<C.size;++i) {
    const auto &layer=C.circuit[i];const auto t=(i-1)%23+1,b=(i-1)/23;
    const bool fc=t==4||t==10||t==16||t==21;require((layer.ty==layerType::FCONN)==fc,"unexpected FCONN layer");
    if(!fc)checkGateOrder(layer,i);
    for(unsigned port=0;port<2;++port) {
      const auto &v=port?layer.ori_id_v:layer.ori_id_u;auto m=port?layer.size_v[0]:layer.size_u[0];int bits=port?layer.bit_length_v[0]:layer.bit_length_u[0];
      require(v.size()==m,"ori_id length mismatch");require((m==0&&bits==-1)||(m>0&&bits==static_cast<int>(dimensions(paddedSize(m)))),"ori_id padded length mismatch");
      if(fc){unsigned f=t==4?0:t==10?1:t==16?2:3;const std::size_t starts[]={163735562,164425384,164685894,165325028},weights[]={0,4198400,5248000,9446400};
        const std::size_t start=port?32768+13641728*b+weights[f]:starts[f]+2640120*b;
        const std::size_t count=port?(f==1?1048576:4194304):(f==3?131072:32768);
        require(v.size()==count,"FCONN port size mismatch");for(std::size_t j=0;j<v.size();++j)require(v[j]==start+j,"FCONN row-major mapping mismatch");
      }
      l.ports[2*i+port]=compress(v,l.n0);
    }
  }
  return l;
}
}
