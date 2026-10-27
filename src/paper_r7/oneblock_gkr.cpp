#include "oneblock_gkr.hpp"
#include "oneblock_sumcheck.hpp"
#include "prover.hpp"
#include "gkr_protocol.hpp"
#include "host_safety.hpp"
#include "r6_adapter.hpp"
#include <array>
#include <algorithm>

namespace zkgpt::r7::oneblock {
using namespace crypto;
namespace {

std::size_t domain(int bits){need(bits>=0 && bits<=24,"Oneblock GKR public domain bound");return std::size_t{1}<<unsigned(bits);}
unsigned bits(std::size_t n){need(n && !(n&(n-1)),"Oneblock GKR power-of-two dimension");return r1::log2exact(n);}

Hidden known(IO&io,const std::string&role,const Fr&value){
 Hidden h;h.handle.session=io.session();h.handle.role=role;h.value=value;h.handle.point=scalarCommit(value,Fr(0));
 bindPublic(io,role+"/public-handle",encode(h.handle));return h;
}
Hidden exchange(IO&io,const std::string&role,const Fr&value,Random*rng){
 Hidden h;Bytes bytes;if(io.prover()){h=hide(io.session(),role,value,*rng);bytes=encode(h.handle);}
 bytes=io.message(role+"/commitment",bytes);h.handle=decodeHandle(bytes);validate(h.handle,io.session(),role);return h;
}
Hidden derived(IO&io,const std::string&role,const std::vector<Hidden>&h,const std::vector<Fr>&coeff){
 auto result=combine(io.session(),role,h,coeff);bindPublic(io,role+"/derived",encode(result.handle));return result;
}

// p returns a*x^2+b*x+c. Coefficients are committed in constant-first order.
// Only the live V coin following all three commitments defines the next point.
std::pair<Hidden,Fr> round(IO&io,const Hidden&previous,const quadratic_poly&p,Random*rng,const std::string&role){
 std::vector<Hidden>h{previous};
 const std::array<Fr,3>coeff{p.c,p.b,p.a};
 for(unsigned j=0;j<3;++j)h.push_back(exchange(io,role+"/c/"+std::to_string(j),coeff[j],rng));
 Relations recurrence;recurrence.role=role+"/recurrence";for(auto&x:h)recurrence.handles.push_back(x.handle);
 recurrence.linear={{Fr(-1),Fr(2),Fr(1),Fr(1)}};
 sigma(io,recurrence,io.prover()?&h:nullptr,rng);
 auto r=io.coin(role+"/r");std::vector<Hidden>c(h.begin()+1,h.end());
 return {derived(io,role+"/evaluation",c,{Fr(1),r,r*r}),r};
}

std::vector<Fr> beta(unsigned count,const std::vector<Fr>&point,const Fr&scale){
 auto n=domain(int(count));std::vector<Fr>out(n,Fr(0));if(scale.isZero())return out;
 need(point.size()>=count,"Oneblock GKR public predicate point dimension");
 auto w=r1::weights(std::vector<Fr>(point.begin(),point.begin()+count));
 for(std::size_t j=0;j<n;++j)out[j]=w[j]*scale;
 return out;
}

// Frozen prover exposes no setter for both current output points. A local
// four-layer arithmetic view provides it without touching frozen source:
// 0=borrowed Master, 1=borrowed preceding values, 2=real public layer,
// 3=empty-gate seed. Its public Update/Finalize calls store the V's already
// selected coordinates in the private point arrays before entering layer 2.
// No seed data or challenge comes from a prover message. Only this layer's
// public wiring is copied; all field witness storage is borrowed via swap.
class NativeLayerOracle {
 ::prover&source_;std::uint32_t id_;::prover oracle_;
 public:
 NativeLayerOracle(::prover&source,const layeredCircuit&graph,std::uint32_t id):source_(source),id_(id){
  oracle_.C.init(0,4);oracle_.val.resize(4);
  oracle_.C.circuit[0]=graph.circuit[0];
  auto&previous=oracle_.C.circuit[1];previous.size=graph.circuit[id-1].size;previous.bit_length=graph.circuit[id-1].bit_length;previous.ty=graph.circuit[id-1].ty;
  oracle_.C.circuit[2]=graph.circuit[id];
  auto&seed=oracle_.C.circuit[3];seed.ty=layerType::PLACE_HOLDER;seed.size=seed.zero_start_id=1;seed.bit_length=0;
  // Allocation and public copying finish before any private ownership moves.
  oracle_.val[0].swap(source_.val[0]);if(id_>1)oracle_.val[1].swap(source_.val[id_-1]);
 }
 NativeLayerOracle(const NativeLayerOracle&)=delete;
 ~NativeLayerOracle(){oracle_.val[0].swap(source_.val[0]);if(id_>1)oracle_.val[1].swap(source_.val[id_-1]);}
 ::prover&start(const std::vector<Fr>&g0,const std::vector<Fr>&g1,const Fr&alpha,const Fr&weight_beta){
  need(!g0.empty() && g0.size()<=24 && g1.size()<=24,"Oneblock GKR seed point dimensions");
  auto&seed=oracle_.C.circuit[3];seed.bit_length_u[0]=i8(g0.size());seed.max_bl_u=i8(g0.size());
  seed.bit_length_v[0]=g1.empty()?-1:i8(g1.size());seed.max_bl_v=g1.empty()?0:i8(g1.size());seed.need_phase2=!g1.empty();
  std::vector<Fr>unused;
  oracle_.init();oracle_.sumcheckInitAll(unused.begin());oracle_.sumcheckInit(Fr(0),Fr(0));
  oracle_.sumcheckInitPhase1(Fr(1));Fr last=0,a,b;
  for(auto&r:g0){oracle_.sumcheckUpdate1(last);last=r;}
  oracle_.sumcheckFinalize1(last,a,b);
  if(!g1.empty()){
   oracle_.sumcheckInitPhase2();last=0;
   for(auto&r:g1){oracle_.sumcheckUpdate2(last);last=r;}
   oracle_.sumcheckFinalize2(last,a,b);
  }
  oracle_.sumcheckInit(alpha,weight_beta);return oracle_;
 }
};

void zeroCarry(IO&io,const Hidden&previous,const Hidden&u1,const Hidden&v1,Random*rng,const std::string&role){
 std::vector<Hidden>h{previous,u1,v1};Relations relation;relation.role=role+"/zero-output-carry";
 for(auto&x:h)relation.handles.push_back(x.handle);
 relation.linear={{Fr(1),Fr(0),Fr(0)},{Fr(0),Fr(1),Fr(0)},{Fr(0),Fr(0),Fr(1)}};
 sigma(io,relation,io.prover()?&h:nullptr,rng);
}

// The public wiring relation is identical to computePredicates/finalPredicateValue
// in frozen gkr_verifier.cpp. P's witness and P's predicate values never enter it.
void predicates(const layeredCircuit&graph,std::size_t id,const std::vector<Fr>&g0,const std::vector<Fr>&g1,
                const Fr&alpha,const Fr&weight_beta,const Fr&zero_scale,const std::vector<Fr>&u,
                const std::vector<Fr>*v,std::array<Fr,2>&uni,std::array<Fr,3>&bin){
 const auto&l=graph.circuit.at(id);auto bg=beta(unsigned(l.bit_length),g0,alpha);auto bg1=beta(unsigned(l.bit_length),g1,weight_beta);
 for(std::size_t j=0;j<bg.size();++j){bg[j]+=bg1[j];if(l.zero_start_id<l.size && j>=l.zero_start_id)bg[j]*=zero_scale;}
 auto bu=beta(unsigned(l.max_bl_u),u,Fr(1));uni={Fr(0),Fr(0)};bin={Fr(0),Fr(0),Fr(0)};
 for(const auto&gate:l.uni_gates){
  need(gate.g<bg.size() && gate.u<bu.size(),"Oneblock GKR public unary gate index");
  uni[gate.lu==0?0:1]+=bg[gate.g]*bu[gate.u]*host_safety::field(gate.sc);
 }
 if(!v)return;
 auto bv=beta(unsigned(l.max_bl_v),*v,Fr(1));for(auto&x:uni)x*=bv[0];
 for(const auto&gate:l.bin_gates){
  need(gate.g<bg.size() && gate.u<bu.size() && gate.v<bv.size() && gate.l<=2,"Oneblock GKR public binary gate index");
  bin[gate.l]+=bg[gate.g]*bu[gate.u]*bv[gate.v]*host_safety::field(gate.sc);
 }
}

void finalRelation(IO&io,const Hidden&previous,const std::array<Hidden,4>&end,const std::array<Fr,2>&uni,
                   const std::array<Fr,3>&bin,Random*rng,const std::string&role){
 // Positions: previous,u0,u1,v0,v1,u0*v0,u1*v1,u1*v0.
 std::vector<Hidden>h{previous,end[0],end[1],end[2],end[3]};
 const std::array<std::array<std::size_t,2>,3>pairs{{{{1,3}},{{2,4}},{{2,3}}}};
 Relations relation;relation.role=role+"/predicate";
 for(unsigned j=0;j<3;++j){auto a=pairs[j][0],b=pairs[j][1];h.push_back(exchange(io,role+"/product/"+std::to_string(j),io.prover()?h[a].value*h[b].value:Fr(0),rng));relation.products.push_back({a,b,5+j});}
 for(auto&x:h)relation.handles.push_back(x.handle);
 relation.linear={{Fr(-1),uni[0],uni[1],Fr(0),Fr(0),bin[0],bin[1],bin[2]}};
 sigma(io,relation,io.prover()?&h:nullptr,rng);
}

void terminal(IO&io,PendingGKR&pending,const layeredCircuit&graph,std::uint32_t id,unsigned port,unsigned matrix_point,
              const std::vector<Fr>&point,const Hidden&value){
 const auto&l=graph.circuit.at(id);const int width=port?l.bit_length_v[0]:l.bit_length_u[0];
 if(width<0)return;
 const auto actual=port?l.size_v[0]:l.size_u[0];const auto&map=port?l.ori_id_v:l.ori_id_u;
 need(actual==map.size() && actual<=domain(width) && point.size()>=unsigned(width),"Oneblock GKR terminal source shape");
 GKRTerminal t;t.layer=id;t.port=port;t.matrix_point=matrix_point;t.actual=actual;t.padded=domain(width);
 t.point.assign(point.begin(),point.begin()+width);t.value=value;
 Writer binding;binding.number(id);binding.number(port);binding.number(matrix_point);binding.number(actual,8);binding.number(t.padded,8);
 for(auto&r:t.point)binding.scalar(r);
 binding.raw(encode(value.handle));
 bindPublic(io,"Oneblock/GKR/terminal/"+std::to_string(id)+"/"+std::to_string(port)+"/"+std::to_string(matrix_point),binding.b);
 pending.terminals.push_back(std::move(t));
}

struct MatrixResult{Hidden x,w;std::vector<Fr>x_point,w_point;};
MatrixResult matrix(IO&io,const layer&l,const std::vector<Fr>&parent,const Hidden&initial,::prover*p,
                    unsigned sequence_bits,Random*rng,const std::string&role,PendingGKR&pending){
 need(l.bit_length>=int(sequence_bits) && l.bit_length_u[0]>=int(sequence_bits),"Oneblock GKR FC public dimensions");
 unsigned k=unsigned(l.bit_length)-sequence_bits,m=unsigned(l.bit_length_u[0])-sequence_bits;
 need(l.bit_length_v[0]==int(m+k) && parent.size()>=k+sequence_bits && l.size_u[0]==domain(int(m+sequence_bits)) &&
      l.size_v[0]==domain(int(m+k)) && l.ori_id_u.size()==l.size_u[0] && l.ori_id_v.size()==l.size_v[0],"Oneblock GKR FC source layout");
 std::vector<Fr>row_point(parent.begin()+k,parent.begin()+k+sequence_bits),column_point(parent.begin(),parent.begin()+k);
 auto rows=beta(sequence_bits,row_point,Fr(1)),columns=beta(k,column_point,Fr(1));
 std::vector<Value>factors;
 if(io.prover()){
  factors.push_back([p,&l,m,rows](std::size_t s){Fr sum=0;for(std::size_t j=0;j<rows.size();++j)sum+=rows[j]*p->val[0].at(l.ori_id_u.at(s+(j<<m)));return sum;});
  factors.push_back([p,&l,m,columns](std::size_t s){Fr sum=0;for(std::size_t j=0;j<columns.size();++j)sum+=columns[j]*p->val[0].at(l.ori_id_v.at(s+(j<<m)));return sum;});
 }
 Expression expression;expression.factor_count=2;expression.terms.push_back({Fr(1),{0,1}});
 auto reduction=expressionSumcheck(io,domain(int(m)),initial,factors,expression,rng,role);
 need(reduction.endpoints.size()==2 && reduction.point.size()==m,"Oneblock GKR FC reduction endpoints");
 MatrixResult result;result.x=reduction.endpoints[0];result.w=reduction.endpoints[1];result.x_point=reduction.point;result.w_point=reduction.point;
 result.x_point.insert(result.x_point.end(),row_point.begin(),row_point.end());result.w_point.insert(result.w_point.end(),column_point.begin(),column_point.end());
 ++pending.matrices;pending.rounds+=m;return result;
}
}

PendingGKR runHiddenGKR(IO&io,const layeredCircuit&graph,::prover*p,std::size_t sequence_length,Random*rng){
 need(graph.size>=2 && graph.circuit.size()==graph.size,"Oneblock GKR public circuit shape");
 need(io.prover()?(p && rng):(!p && !rng),"Oneblock GKR P/V private boundary");
 const unsigned sequence_bits=bits(sequence_length);
 domain(int(sequence_bits));domain(graph.circuit.back().bit_length);
 const auto fingerprint=fingerprintCircuit(graph);Writer binding;binding.text("R7-ONEBLOCK-HIDDEN-GKR-v1");binding.raw(Bytes(fingerprint.begin(),fingerprint.end()));binding.number(sequence_length,8);
 bindPublic(io,"Oneblock/GKR/public-circuit",binding.b);
 if(io.prover())need(fingerprintCircuit(p->C)==fingerprint && p->val.size()==graph.size,"Oneblock GKR private/public circuit identity");
 std::vector<std::vector<Fr>>u(graph.size+1),v(graph.size+1);
 u[graph.size].resize(unsigned(graph.circuit.back().bit_length));
 for(std::size_t j=0;j<u.back().size();++j)u.back()[j]=io.coin("Oneblock/GKR/output-point/"+std::to_string(j));
 auto previous=known(io,"Oneblock/GKR/public-zero-output",Fr(0));
 checked(io,"gkr/public-output-evaluation");
 auto parent_u1=known(io,"Oneblock/GKR/initial-u1",Fr(0)),parent_v1=known(io,"Oneblock/GKR/initial-v1",Fr(0));
 Fr alpha=1,weight_beta=0;auto g0=u.back();std::vector<Fr>g1;
 PendingGKR pending;
 for(std::uint32_t id=graph.size;id-->1;){
  const auto&l=graph.circuit.at(id);auto role="Oneblock/GKR/layer/"+std::to_string(id);
  // Most native R6 layers are independent systems of zero equalities. Their
  // output constraint is meaningful even when an FC/master-only layer above
  // has no previous-layer edge. A fresh V point checks every such system.
  if(l.ty!=layerType::FCONN && l.zero_start_id==0){
   zeroCarry(io,previous,parent_u1,parent_v1,rng,role);
   g0.resize(unsigned(l.bit_length));for(std::size_t j=0;j<g0.size();++j)g0[j]=io.coin(role+"/zero-output-point/"+std::to_string(j));
   g1.clear();alpha=1;weight_beta=0;previous=known(io,role+"/zero-output",Fr(0));
  }
  const auto zero_scale=l.zero_start_id<l.size?io.coin(role+"/zero-scale"):Fr(1);
  std::array<Hidden,4>end;
  if(l.ty==layerType::FCONN){
   need(l.zero_start_id>=l.size && l.bit_length_u[1]<0 && l.bit_length_v[1]<0,"Oneblock GKR unsupported FC zero/carry metadata");
   auto first=matrix(io,l,g0,parent_u1,p,sequence_bits,rng,role+"/matrix/u",pending);
   checked(io,"gkr/layer/"+std::to_string(id)+"/matrix/u");
   auto second=matrix(io,l,g1,parent_v1,p,sequence_bits,rng,role+"/matrix/v",pending);
   checked(io,"gkr/layer/"+std::to_string(id)+"/matrix/v");
   u[id]=first.x_point;v[id]=first.w_point;
   end[0]=first.x;end[2]=first.w;end[1]=known(io,role+"/final/u1",Fr(0));end[3]=known(io,role+"/final/v1",Fr(0));
   terminal(io,pending,graph,id,0,0,u[id],end[0]);terminal(io,pending,graph,id,1,0,v[id],end[2]);
   terminal(io,pending,graph,id,0,1,second.x_point,second.x);terminal(io,pending,graph,id,1,1,second.w_point,second.w);
   checked(io,"gkr/layer/"+std::to_string(id)+"/predicate");
  }else{
   // Native ordinary phases all contain at least one variable. The frozen P
   // finalizer indexes round-1 and cannot represent an empty phase through its
   // public API. This restriction is checked, never silently skipped.
   need(l.max_bl_u>0 && (!l.need_phase2 || l.max_bl_v>0),"Oneblock GKR zero-round ordinary phase unsupported");
   domain(l.max_bl_u);if(l.need_phase2)domain(l.max_bl_v);
   need(!alpha.isZero() || !weight_beta.isZero(),"Oneblock GKR nonzero layer has no authenticated output claim");
   std::unique_ptr<NativeLayerOracle>view;::prover*arithmetic=nullptr;
   if(io.prover())executePrivateWitness([&]{view=std::make_unique<NativeLayerOracle>(*p,graph,id);arithmetic=&view->start(g0,g1,alpha,weight_beta);});
   Fr last=0;u[id].resize(unsigned(l.max_bl_u));
   if(io.prover())executePrivateWitness([&]{arithmetic->sumcheckInitPhase1(zero_scale);});
   for(unsigned j=0;j<unsigned(l.max_bl_u);++j){
    quadratic_poly polynomial;if(io.prover())executePrivateWitness([&]{polynomial=arithmetic->sumcheckUpdate1(last);});
    auto step=round(io,previous,polynomial,rng,role+"/phase1/round/"+std::to_string(j));previous=step.first;last=u[id][j]=step.second;++pending.rounds;
   }
   Fr u0=0,u1=0;if(io.prover())executePrivateWitness([&]{arithmetic->sumcheckFinalize1(last,u0,u1);});
   end[0]=l.bit_length_u[0]>=0?exchange(io,role+"/final/u0",u0,rng):known(io,role+"/final/u0",Fr(0));
   end[1]=l.bit_length_u[1]>=0?exchange(io,role+"/final/u1",u1,rng):known(io,role+"/final/u1",Fr(0));
   Fr v0=0,v1=0;
   if(l.need_phase2){
    v[id].resize(unsigned(l.max_bl_v));last=0;if(io.prover())executePrivateWitness([&]{arithmetic->sumcheckInitPhase2();});
    for(unsigned j=0;j<unsigned(l.max_bl_v);++j){
     quadratic_poly polynomial;if(io.prover())executePrivateWitness([&]{polynomial=arithmetic->sumcheckUpdate2(last);});
     auto step=round(io,previous,polynomial,rng,role+"/phase2/round/"+std::to_string(j));previous=step.first;last=v[id][j]=step.second;++pending.rounds;
    }
    if(io.prover())executePrivateWitness([&]{arithmetic->sumcheckFinalize2(last,v0,v1);});
   }
   end[2]=l.need_phase2 && l.bit_length_v[0]>=0?exchange(io,role+"/final/v0",v0,rng):known(io,role+"/final/v0",Fr(0));
   end[3]=l.need_phase2 && l.bit_length_v[1]>=0?exchange(io,role+"/final/v1",v1,rng):known(io,role+"/final/v1",Fr(0));
   std::array<Fr,2>uni;std::array<Fr,3>bin;predicates(graph,id,g0,g1,alpha,weight_beta,zero_scale,u[id],l.need_phase2?&v[id]:nullptr,uni,bin);
   finalRelation(io,previous,end,uni,bin,rng,role);
   checked(io,"gkr/layer/"+std::to_string(id)+"/phase1");
   if(l.need_phase2)checked(io,"gkr/layer/"+std::to_string(id)+"/phase2");
   checked(io,"gkr/layer/"+std::to_string(id)+"/predicate");
   if(l.zero_start_id<l.size)checked(io,"gkr/layer/"+std::to_string(id)+"/zero-region");
   terminal(io,pending,graph,id,0,0,u[id],end[0]);terminal(io,pending,graph,id,1,0,v[id],end[2]);
  }
  // The exact original endpoint handles enter the next layer; no new free
  // initial claim may replace this publicly derived linear combination.
  Writer finals;for(auto&h:end)finals.raw(encode(h.handle));bindPublic(io,role+"/final-handles",finals.b);
  alpha=l.bit_length_u[1]>=0?io.coin(role+"/alpha"):Fr(0);
  weight_beta=(l.bit_length_v[1]>=0 || l.ty==layerType::FFT)?io.coin(role+"/beta"):Fr(0);
  previous=derived(io,role+"/carry",{end[1],end[3]},{alpha,weight_beta});parent_u1=end[1];parent_v1=end[3];
  checked(io,"gkr/layer/"+std::to_string(id)+"/carry");
  g0=u[id];g1=v[id];++pending.layers;++pending.carries;
 }
 if(io.prover())p->releaseSumcheckWorkspace();
 return pending;
}
}
