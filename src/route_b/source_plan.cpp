#include "source_plan.hpp"
#include <sstream>
#include <set>

namespace route_b::production {
namespace {
PCSpec mainPC(){return {"Hyrax-row-IPA-Fr-BN254","zkGPT/main","C0","mixed-input;row-major;lsb-first",16384,16384};}
PCSpec rangePC(std::size_t q,std::size_t p){return {"Hyrax-row-IPA-Fr-BN254","zkGPT/range","range/Z/"+std::to_string(q),"linear-combination-of-same-D-chunk-rows",p/4096,4096};}
PCSpec expPC(unsigned k){return {"Hyrax-row-IPA-Fr-BN254","zkGPT/exp-lookup/v4","exp/vector/"+std::to_string(k),"same-parent-exp-eight-vector-rows",256,512};}
PCSpec directPC(std::size_t k){return {"vector-IPA-Fr-BN254","zkGPT.route-b.lasso-prototype.v1.vector-ipa","direct/Q/"+std::to_string(k),"same-Q-as-local-B1-and-opening;D1-to-C0-required",1,256};}
SourceSpec source(SourceId id,const std::string &kind,std::size_t m,std::vector<Span> map,PCSpec pc,std::vector<std::string> deps) {
  SourceSpec s;s.id=id;s.kind=kind;s.m=m;s.p=paddedSize(m);s.d=dimensions(s.p);s.support=m;s.shape={m};s.projection=std::move(map);s.pc=std::move(pc);s.dependencies=std::move(deps);return s;
}
std::string gkrID(unsigned i){return "gkr/layer/"+std::to_string(i);}
std::string rq(std::size_t q){return "range/reconstruction/"+std::to_string(q);}
void str(std::ostream&o,const std::string&s){o<<'"';for(unsigned char c:s){if(c=='"'||c=='\\')o<<'\\'<<c;else if(c<32){const char *hex="0123456789abcdef";o<<"\\u00"<<hex[c>>4]<<hex[c&15];}else o<<c;}o<<'"';}
template<class T,class F>void arr(std::ostream&o,const std::vector<T>&v,F f){o<<'[';bool first=true;for(auto&x:v){if(!first)o<<',';first=false;f(o,x);}o<<']';}
void strings(std::ostream&o,const std::vector<std::string>&v){arr(o,v,[](auto&os,auto&x){str(os,x);});}
void nums(std::ostream&o,const std::vector<std::size_t>&v){arr(o,v,[](auto&os,auto x){os<<x;});}
void spans(std::ostream&o,const std::vector<Span>&v){arr(o,v,[](auto&os,auto&x){os<<'['<<x.offset<<','<<x.count<<','<<x.bias_bits<<']';});}
void pc(std::ostream&o,const PCSpec&p){o<<"{\"scheme\":";str(o,p.scheme);o<<",\"domain\":";str(o,p.domain);o<<",\"oracle\":";str(o,p.oracle);o<<",\"relation\":";str(o,p.relation);o<<",\"rows\":"<<p.rows<<",\"columns\":"<<p.columns<<'}';}
void spec(std::ostream&o,const SourceSpec&s){o<<"{\"id\":["<<s.id.family<<','<<s.id.index<<','<<s.id.port<<','<<s.id.point<<"],\"kind\":";str(o,s.kind);
  o<<",\"m\":"<<s.m<<",\"p\":"<<s.p<<",\"d\":"<<s.d<<",\"support\":"<<s.support<<",\"shape\":";nums(o,s.shape);
  o<<",\"projection\":";spans(o,s.projection);o<<",\"bits\":"<<s.bits<<",\"is_signed\":"<<(s.is_signed?"true":"false")<<",\"alias\":";str(o,s.alias);
  o<<",\"padding\":";str(o,s.padding);o<<",\"value_rule\":";str(o,s.value_rule);o<<",\"pc\":";pc(o,s.pc);o<<",\"dependencies\":";strings(o,s.dependencies);o<<'}';}
void obligation(std::ostream&o,const ObligationSpec&s){o<<"{\"id\":";str(o,s.id);o<<",\"kind\":";str(o,s.kind);o<<",\"relation\":";str(o,s.relation);o<<",\"table\":";str(o,s.table);
  o<<",\"child_id\":";str(o,s.child_id);o<<",\"native_padding\":";str(o,s.native_padding);
  o<<",\"index\":"<<s.index<<",\"subindex\":"<<s.subindex<<",\"m\":"<<s.m<<",\"p\":"<<s.p<<",\"shift\":"<<s.shift<<",\"query_length\":"<<s.query_length<<",\"table_size\":"<<s.table_size<<",\"bits\":"<<s.bits<<",\"pc\":";pc(o,s.pc);o<<",\"dim_pc\":";pc(o,s.dim_pc);o<<",\"dependencies\":";strings(o,s.dependencies);o<<'}';}
std::string encoded(const ExpectedPlan&p){std::ostringstream o;writePlan(o,p);return o.str();}
}
ExpectedPlan buildExpectedPlan(const TrustedDeployment&deployment,const PublicModelLayout&layout,const layeredCircuit&C,const WitnessRegistry&R) {
  // Never trust a caller's precomputed layout. Rebuild and check its full public
  // metadata. This is deliberately independent of the supplied artifact plan.
  auto l=rebuildProductionLayout(deployment,C,R);
  std::ostringstream a,b;writeLayout(a,l,C);writeLayout(b,layout,C);require(a.str()==b.str(),"untrusted supplied layout");
  ExpectedPlan p;p.policy_id=deployment.policy_id;p.model_id=deployment.model_id;p.implementation_profile=deployment.implementation_profile;p.mode=deployment.mode;p.input_pc=mainPC();
  for(unsigned i=1;i<C.size;++i) {
    auto &layer=C.circuit[i];
    for(unsigned point=0;point<(layer.ty==layerType::FCONN?2U:1U);++point)for(unsigned port=0;port<2;++port){
      auto m=port?layer.size_v[0]:layer.size_u[0];if(!m)continue;
      p.sources.push_back(source({0,i,port,point},"gkr-input",m,l.ports[2*i+port],mainPC(),{gkrID(i)}));
    }
    ObligationSpec o;o.id=gkrID(i);o.kind="gkr-layer";o.index=i;o.m=layer.size;o.p=paddedSize(layer.size);o.shift=layer.zero_start_id;
    o.relation=layer.ty==layerType::FCONN?"two-matrix-points-full-contraction-and-output-coordinates;continuation-is-not-input":"all-unary-binary-gates;continuation-to-previous-layer";
    o.pc=mainPC();o.dependencies={"gkr/zero/"+std::to_string(i)};p.obligations.push_back(o);
    o.id=o.dependencies[0];o.kind="gkr-zero";o.relation="V[i][zero_start:size]=0;last-check-output-is-zero-not-public-Y";o.dependencies.clear();p.obligations.push_back(o);
  }
  for(std::size_t q=0;q<l.ranges.queries.size();++q) {
    auto &query=l.ranges.queries[q];std::vector<Span> map;
    for(auto &region:l.ranges.regions)if(region.proof_constraint_index==q){require(region.proof_start==([&]{std::size_t n=0;for(auto&x:map)n+=x.count;return n;})(),"Range mapping has hole");map.push_back({region.val0_offset,region.count,region.is_signed?region.bits:0});}
    auto s=source({1,q,0,0},"range-Z",query.actual_query_size,std::move(map),rangePC(q,query.padded_query_size),{rq(q)});s.bits=query.bits;s.value_rule="w[pi]+per-region-signed-bias;Z=sum(2^shift*D)";p.sources.push_back(std::move(s));
    ObligationSpec reconstruct;reconstruct.id=rq(q);reconstruct.kind="range-reconstruction";reconstruct.index=q;reconstruct.m=query.actual_query_size;reconstruct.p=query.padded_query_size;reconstruct.bits=query.bits;reconstruct.pc=rangePC(q,query.padded_query_size);reconstruct.relation="same-D-row-linear-Z;encoded-bias;all-actual-and-padding-rows";
    for(unsigned h=0,shift=0;shift<query.bits;++h){unsigned bits=std::min(9U,query.bits-shift);ObligationSpec o;o.id="range/chunk/"+std::to_string(q)+"/"+std::to_string(h);o.kind="range-chunk";o.index=q;o.subindex=h;o.m=query.actual_query_size;o.p=query.padded_query_size;o.shift=shift;o.bits=bits;o.table="integer-interval-[0,2^bits)";
      o.pc=rangePC(q,o.p);o.pc.oracle="range/D/"+std::to_string(q)+"/"+std::to_string(h);o.pc.relation="exact-same-D-as-reconstruction-and-lookup";
      o.query_length=o.p;o.table_size=std::size_t{1}<<bits;
      o.child_id=(deployment.mode==Mode::LassoV2?"lasso/range/":"logup/range/")+std::to_string(q)+"/"+std::to_string(h);
      o.native_padding=deployment.mode==Mode::LassoV2?"none;parent-tail-is-member-row":"legacy-parent-padding";
      o.relation=deployment.mode==Mode::BridgeV1?"all-LogUp-membership-relations":"all-production-Lasso-membership-relations";
      reconstruct.dependencies.push_back(o.id);p.obligations.push_back(o);shift+=bits;
    }p.obligations.push_back(reconstruct);
  }
  for(unsigned k=0;k<2;++k){std::vector<Span> map;for(auto &e:l.exp){auto off=k?e.E_witness_index:e.t_witness_index;if(!map.empty()&&map.back().offset+map.back().count==off)++map.back().count;else map.push_back({off,1,0});}
    std::vector<std::string> deps={"exp/branch/0","exp/branch/1","range/uint32-t-E"};
    for(unsigned i=0;i<6;++i)deps.push_back("exp/membership/"+std::to_string(i));
    auto s=source({2,k,0,0},k?"exp-E":"exp-t",l.exp.size(),std::move(map),expPC(k),std::move(deps));s.bits=32;s.padding=k?"1048576":"0";s.value_rule="uint32;operator-major;query-minor;E-index=t-index+6336";p.sources.push_back(s);
  }
  for(unsigned i=0;i<2;++i){ObligationSpec o;o.id="exp/branch/"+std::to_string(i);o.kind="exp-branch";o.index=i;o.m=l.exp.size();o.p=paddedSize(o.m);o.query_length=o.p;o.pc=expPC(0);o.relation=i?"s*(2*K-1-2*t)=sum(256^h*d_h)-t+K":"s*(t-K)=x-K";o.table="K=6891;padding(t,E,s,x,d)=(0,1048576,1,0,6890);limbs=(234,26,0,0)";o.dependencies={"exp/eight-vector-openings","range/uint32-t-E"};for(unsigned j=0;j<6;++j)o.dependencies.push_back("exp/membership/"+std::to_string(j));p.obligations.push_back(o);}
  for(unsigned i=0;i<6;++i){
    const bool lasso=deployment.mode==Mode::LassoV2;
    ObligationSpec o;o.id="exp/membership/"+std::to_string(i);o.kind="exp-membership";o.index=i;o.m=l.exp.size();o.p=paddedSize(o.m);o.query_length=o.p;o.bits=i==0?13:i==1?1:8;o.table_size=std::size_t{1}<<o.bits;
    o.table=i==0?(lasso?"T[y]=canonicalExpValue(y);8192;K=6891;tail=1":"canonical-indexed-pair-8192;K=6891;tail=(K,1)"):i==1?"{0,1}":"[0,256)";
    o.pc=expPC(i==0?(lasso?1:3):i==1?2:i+2);
    if(i==0){if(lasso)o.dim_pc=expPC(3);else o.pc.oracle="exp/P=Cx+alpha*CE";}
    o.child_id=lasso?(i==0?"lasso/exp/E-indexed":i==1?"lasso/exp/s":"lasso/exp/d/"+std::to_string(i-2)):"logup/exp/"+std::to_string(i);
    o.native_padding=lasso?"none;parent-tail-is-member-row":"legacy-parent-padding";
    o.relation=lasso?(i==0?"Lasso;query-is-exact-CE;dim-is-exact-Cx;no-new-commitments-or-pair-compression":"Lasso;same-exp-parent-query-oracle;validity-memory-addresses"):"LogUp;six-memberships;alpha-after-eight-C";
    o.dependencies={"exp/eight-vector-openings"};p.obligations.push_back(o);
  }
  {ObligationSpec o;o.id="exp/eight-vector-openings";o.kind="exp-openings";o.m=l.exp.size();o.p=paddedSize(o.m);o.pc=expPC(0);o.relation=deployment.mode==Mode::BridgeV1?"t,E,s,x,d0,d1,d2,d3;eight-exact-oracles;twelve-vector-openings;indexed-pair-compression;canonical-table-ID":"t,E,s,x,d0,d1,d2,d3;eight-exact-oracles;indexed-E-reuses-Cx-address;no-random-pair-compression;canonical-table-ID";o.table=hexDigest(canonicalExpTableId());p.obligations.push_back(o);o.id="range/uint32-t-E";o.kind="exp-range";o.relation="every-t-and-E-row-in-registered-unsigned32-range";o.dependencies={"range/reconstruction/2"};p.obligations.push_back(o);}
  if(deployment.mode==Mode::BridgeV1)for(unsigned b=0;b<12;++b) {
    const PublicRangeRegion *region=nullptr;for(auto &r:l.ranges.regions)if(r.name=="gelu_"+std::to_string(b)+"_t")region=&r;
    require(region&&region->count==131072&&region->bits==1&&!region->is_signed,"direct GeLU parent mismatch");
    for(unsigned tile=0;tile<512;++tile){auto k=512*b+tile;auto id="direct/local/"+std::to_string(k);
      auto s=source({3,k,0,0},"gelu-direct",256,{{region->val0_offset+256*tile,256,0}},directPC(k),{id,"gkr/gelu-threshold-and-slacks"});s.bits=1;s.value_rule="full12-gelu-t-direct-v1;table=[0,1];segment_bits=[1];additive=false;instance=1";p.sources.push_back(s);
      ObligationSpec o;o.id=id;o.kind="direct-B1";o.index=k;o.m=o.p=256;o.bits=1;o.table="[0,1];segment_bits=[1];additive=false;instance=1";o.pc=directPC(k);o.relation="complete-B1-local-proof-and-same-Q-opening;D1-to-C0-required";o.dependencies={"gkr/gelu-threshold-and-slacks"};p.obligations.push_back(o);
    }
  }
  {ObligationSpec o;o.id="gkr/gelu-threshold-and-slacks";o.kind="gelu-gates";o.pc=mainPC();o.relation="all-twelve-GeLU-layer-relations;Boolean-membership-does-not-replace-threshold";for(unsigned b=0;b<12;++b)for(unsigned j=18;j<=20;++j)o.dependencies.push_back(gkrID(23*b+j));p.obligations.push_back(o);}
  if(l.n0<l.N0){auto s=source({4,0,0,0},"zero-tail",l.N0,{{l.n0,l.N0-l.n0,0}},mainPC(),{"input/zero-padding"});s.support=l.N0-l.n0;s.value_rule="mask-C0-indices-[n0,N0);verifier-claim=0";p.sources.push_back(s);}
  std::vector<Span> constants;for(auto c:l.constants)constants.push_back({c,1,0});
  const auto constant_count=constants.size();auto s=source({5,0,0,0},"public-constants",constant_count,std::move(constants),mainPC(),{"public/constants-binding"});s.bits=1;s.value_rule="actual=1;verifier-computes-chi-sum;pad=0";p.sources.push_back(s);
  s=source({6,0,0,0},"public-output",32768,{{l.output_offset,32768,0}},mainPC(),{"public/output-binding"});s.shape={32,768,1024};s.value_rule="public-quantized-Y;row-pad-768-to-1024-with-zero;verifier-MLE;not-last-constraint-output";p.sources.push_back(s);
  for(std::size_t j=0;j<l.parameters.size();++j){auto &r=l.parameters[j];ObligationSpec o;o.id="model/parameter/"+std::to_string(j);o.kind="model-binding";o.index=j;o.m=r.count;o.p=paddedSize(o.m);o.shift=r.offset;o.pc=mainPC();o.relation="same-approved-anchor-masked-parameter-rows;full-row-equality-and-mixed-row-IPA";o.table=r.name;p.obligations.push_back(o);}
  for(auto name:{"public/input-binding","public/output-binding","public/constants-binding","input/zero-padding"}){ObligationSpec o;o.id=name;o.kind="public-binding";o.pc=mainPC();o.relation=std::string(name)=="public/input-binding"?"public-X-32x768-row-pad-to-32x1024;anchor-model-binding-same-C0":std::string(name)=="public/output-binding"?"Y-32x768-row-pad-to-32x1024;D1-to-final-residual-advice":std::string(name)=="public/constants-binding"?"all-96-used-append-constants-one;D1-to-C0":"C0[n0:N0]=0;no-independent-query-commitment";p.obligations.push_back(o);}
  return p;
}
void writePlan(std::ostream&o,const ExpectedPlan&p){o<<"{\"schema\":\"route-b-P0-source-plan-v1\",\"policy_id\":";str(o,p.policy_id);o<<",\"mode\":"<<static_cast<unsigned>(p.mode)<<",\"model_id\":";str(o,p.model_id);o<<",\"implementation_profile\":";str(o,p.implementation_profile);o<<",\"input_pc\":";pc(o,p.input_pc);o<<",\"sources\":";arr(o,p.sources,spec);o<<",\"obligations\":";arr(o,p.obligations,obligation);o<<'}';}
void writeLayout(std::ostream&o,const PublicModelLayout&l,const layeredCircuit&C){o<<"{\"n0\":"<<l.n0<<",\"N0\":"<<l.N0<<",\"ell\":"<<l.ell<<",\"output_offset\":"<<l.output_offset<<",\"constants\":";nums(o,l.constants);
  o<<",\"parameters\":";arr(o,l.parameters,[](auto&os,auto&r){os<<'[';str(os,r.name);os<<','<<r.offset<<','<<r.count<<','<<r.rows<<','<<r.cols<<']';});
  o<<",\"regions\":";arr(o,l.ranges.regions,[](auto&os,auto&r){os<<'['<<static_cast<unsigned>(r.kind)<<',';str(os,r.name);os<<','<<r.val0_offset<<','<<r.count<<','<<r.bits<<','<<(r.is_signed?"true":"false")<<','<<r.proof_constraint_index<<','<<r.proof_start<<']';});
  o<<",\"groups\":";arr(o,l.ranges.queries,[](auto&os,auto&q){os<<'['<<q.bits<<','<<q.actual_query_size<<','<<q.padded_query_size<<']';});
  o<<",\"exp\":";arr(o,l.exp,[](auto&os,auto&e){os<<'['<<e.operator_id<<','<<e.query_id<<','<<e.t_witness_index<<','<<e.E_witness_index<<']';});
  o<<",\"layers\":[";for(unsigned i=1;i<C.size;++i){if(i>1)o<<',';const auto &x=C.circuit[i];o<<"{\"index\":"<<i<<",\"type\":"<<static_cast<unsigned>(x.ty)<<",\"size\":"<<x.size<<",\"zero_start\":"<<x.zero_start_id<<",\"m\":["<<x.size_u[0]<<','<<x.size_v[0]<<"],\"d\":["<<static_cast<int>(x.bit_length_u[0])<<','<<static_cast<int>(x.bit_length_v[0])<<"],\"projection\":[";spans(o,l.ports[2*i]);o<<',';spans(o,l.ports[2*i+1]);o<<"]}";}o<<"]}";
}
void requirePlanMatchesTrusted(const TrustedDeployment&d,const PublicModelLayout&l,const layeredCircuit&C,const WitnessRegistry&R,const ExpectedPlan&candidate){
  // Cheap policy rejection precedes rebuilding or processing an artifact's maps.
  require(candidate.policy_id==d.policy_id&&candidate.model_id==d.model_id&&candidate.implementation_profile==d.implementation_profile&&candidate.mode==d.mode,"unapproved artifact policy/model/profile/mode");
  const auto expected=buildExpectedPlan(d,l,C,R);require(encoded(expected)==encoded(candidate),"source or local-obligation metadata differs from trusted public layout");
}
TrustedPlan::TrustedPlan(const TrustedDeployment&d,const PublicModelLayout&l,const layeredCircuit&C,const WitnessRegistry&R):expected_(buildExpectedPlan(d,l,C,R)){}
void TrustedPlan::requireMatches(const ExpectedPlan&candidate)const{require(encoded(expected_)==encoded(candidate),"source or local-obligation metadata differs from verifier-owned expected plan");}
}
