#include "oneblock_plan.hpp"
#include "paper_r2/r6_model.hpp"
#include "paper_r2/r6_trust.hpp"
#include "paper_r2/r5b_trust.hpp"
#include <set>
#include <sstream>
namespace zkgpt::r7::oneblock {
namespace {
using namespace crypto;
std::size_t power(std::size_t n){std::size_t p=1;while(p<n)p*=2;return p;}
std::string object(const r1::PCSpec&s){return "r7/"+s.role;}
void add(Coverage&p,const std::string&id,const std::string&family,const std::string&source,std::size_t n=0,std::size_t rounds=0){p.required.push_back({id,family,source,n,rounds});}
void memory(Coverage&p,const r1::MemorySpec&s){
 const auto id=s.id;auto n=s.address.padded;add(p,id+"/multiset","MEMORY_MULTISET",object(s.address)+"|"+object(s.value)+"|"+object(r1::readSpec(s))+"|"+object(r1::finalSpec(s)),n);
 for(unsigned kind=0;kind<3;++kind){auto root=id+"/product/"+std::to_string(kind);auto size=kind==2?s.table.size():n;add(p,root+"/root","PRODUCT_ROOT",id,size);
  for(std::size_t width=size;width>1;width/=2)add(p,root+"/level/"+std::to_string(r1::log2exact(width)-1),"PRODUCT_LAYER",id,width/2,r1::log2exact(width/2));
  if(kind==2)add(p,root+"/leaf/final","MEMORY_LEAF",object(r1::finalSpec(s)),size);
  else{add(p,root+"/leaf/address","MEMORY_LEAF",object(s.address),size);add(p,root+"/leaf/value","MEMORY_LEAF",object(s.value),size);add(p,root+"/leaf/read","MEMORY_LEAF",object(r1::readSpec(s)),size);}
 }
}
void range(Coverage&p,const r1::RangeSpec&s){
 add(p,s.id+"/reconstruction","RANGE_RECONSTRUCTION",object(s.query),s.query.padded);add(p,s.id+"/zero-tail","RANGE_TAIL",object(s.query),s.query.padded-s.query.actual);
 for(unsigned h=0;h<(s.bits+8)/9;++h){add(p,s.id+"/digit/"+std::to_string(h)+"/opening","RANGE_DIGIT",object(r1::digitSpec(s,h)),s.query.padded);memory(p,r1::digitMemory(s,h));}
}
std::string quoted(const std::string&s){std::string o="\"";for(auto c:s){if(c=='\"'||c=='\\')o+='\\';need(static_cast<unsigned char>(c)>=32,"coverage nonprintable ID");o+=c;}return o+'\"';}
}
Coverage discover(unsigned blocks){
 Coverage p;p.context=master3::trustedContext(blocks);master3::QuietStdout quiet;LLM nn(blocks,12,64,768,3072);r6::configure(nn);auto graph=nn.buildPublicCircuit();auto q=r2::planModelQueries(graph,nn.getWitnessRegistry(),nn.parameterRegions(),r1::ExpProfile::LegacyClampOne);r6::auditGraph(graph,nn,q);r6::validateRegions(q,nn.parameterRegions());crypto::need(q.circuit_id==p.context.plan.circuit_id,"native coverage circuit mismatch");std::ostringstream native;r6::writePlan(native,graph,nn,q);p.native_json=native.str();p.objects=p.context.objects;p.source_objects=p.objects.size();
 const auto base=object(q.master);add(p,"master/registration","MASTER_REGISTRATION",base,q.master.padded);add(p,"master/zero-tail","MASTER_TAIL",base,q.master.padded-q.master.actual);add(p,"master/hidden-openings","MASTER_OPENING",base,q.master.padded);
 for(const auto&o:p.objects){add(p,"object/"+o.id+"/commitment","OBJECT_COMMITMENT",o.id,o.padded);if(o.kind!=ObjectKind::BaseMaster)add(p,"object/"+o.id+"/source","OBJECT_SOURCE",o.parent,o.padded);p.object_padded_coordinates+=o.padded;
  // Cost of DIRECT reuse of current complete representation registration.
  auto rows=(o.padded+1023)/1024;p.full_representation_response_bytes+=(o.actual+rows)*32;
 }
 for(const auto&s:q.sources)add(p,"mapped/"+s.spec.role,"MAPPED_INPUT",base,s.spec.padded,r1::log2exact(q.master.padded));
 add(p,"public/X","PUBLIC_BINDING",base,q.shape.sequence_length*power(q.shape.hidden_dimension));add(p,"public/Y","PUBLIC_BINDING",base,q.output.size());add(p,"public/constants","PUBLIC_BINDING",base,q.constants.size()+q.reserved_zero.size());
 add(p,"model/checkpoint-exporter-profile","MODEL_AUTHORITY",object(q.sources.back().spec),q.sources.back().spec.actual);add(p,"model/anchor-hiding-rerandomization","MODEL_RERANDOMIZATION",object(q.sources.back().spec),q.sources.back().spec.rows);add(p,"model/C_W_model-to-master","MODEL_MASTER_LINK",base,q.sources.back().spec.actual);
 for(const auto&r:nn.parameterRegions()){p.model_regions.push_back(r.name);add(p,"model/region/"+r.name,"MODEL_PARAMETER_LINK",object(q.sources.back().spec),r.count);}
 if(blocks==12)for(auto&e:nn.blockConnections())if(e.block)add(p,"full12/block/"+std::to_string(e.block+1)+"/input","CROSS_BLOCK_STATE","native-master/"+std::to_string(e.input),32768);
 add(p,"gkr/public-output-evaluation","GKR_OUTPUT","public-zero-constraint-output",q.output.size());
 for(std::size_t i=1;i<graph.circuit.size();++i){auto&l=graph.circuit[i];auto id="gkr/layer/"+std::to_string(i);add(p,id+"/predicate","GKR_PREDICATE",base,l.size);
  if(l.ty==layerType::FCONN){++p.fc_layers;for(auto port:{"u","v"})add(p,id+"/matrix/"+port,"FC_MATRIX",base,l.size,l.ori_id_u.empty()?0:r1::log2exact(l.ori_id_u.size())-r1::log2exact(power(q.shape.sequence_length)));}
  else{add(p,id+"/phase1","GKR_SUMCHECK",base,l.size,unsigned(l.max_bl_u));if(l.need_phase2)add(p,id+"/phase2","GKR_SUMCHECK",base,l.size,unsigned(l.max_bl_v));}
  if(l.zero_start_id<l.size)add(p,id+"/zero-region","GKR_ZERO_REGION",base,l.size-l.zero_start_id);
  add(p,id+"/carry","GKR_INTERNAL_AGGREGATION",i>1?"gkr/layer/"+std::to_string(i-1):base,l.size);
  for(unsigned port=0;port<2;++port){auto bits=port?l.bit_length_v[0]:l.bit_length_u[0];const auto&ids=port?l.ori_id_v:l.ori_id_u;if(bits>=0){add(p,id+"/terminal/"+(port?"v":"u")+"/0","GKR_MASTER_TERMINAL",base,ids.size(),unsigned(bits));++p.native_terminals;}if(l.ty==layerType::FCONN){add(p,id+"/terminal/"+(port?"v":"u")+"/1","GKR_MASTER_TERMINAL",base,ids.size());++p.native_terminals;}}
 }
 add(p,"gkr/input-aggregation","GKR_INPUT_AGGREGATION",base,q.master.padded,r1::log2exact(q.master.padded));add(p,"gkr/input-aggregation-terminal","GKR_AGGREGATION_TERMINAL",base,q.master.padded);
 for(const auto&r:q.ranges)range(p,{r.id,q.sources.at(r.child).spec,r.bits});
 for(const auto&e:q.exps){r1::ExpSpec s{e.id,q.sources.at(e.index_child).spec,q.sources.at(e.value_child).spec,e.profile};for(unsigned k=0;k<4;++k)range(p,r1::expRangeSpec(s,k));for(unsigned k=0;k<2;++k){add(p,e.id+"/constraint/"+std::to_string(k),"EXP_SUMCHECK",object(s.index),s.index.padded,r1::log2exact(s.index.padded));add(p,e.id+"/value-binding/"+std::to_string(k),"EXP_VALUE_BINDING",object(s.value),s.value.padded);}memory(p,r1::expMemorySpec(s));}
 requireCoverage(p);return p;
}
void requireCoverage(const Coverage&p){crypto::need(!p.required.empty() && p.objects.size()==p.source_objects,"coverage empty/object loss");std::set<std::string>ids;for(auto&r:p.required)crypto::need(!r.id.empty() && !r.family.empty() && ids.insert(r.id).second,"coverage duplicate/empty relation");std::set<std::string>objects;for(auto&o:p.objects)crypto::need(objects.insert(o.id).second,"coverage duplicate object");}
std::string identity(const Coverage&p){r1::Fs f(p.context.plan.shape.layer_count==1?"R7-ONEBLOCK-NATIVE-COVERAGE-ROWMAJOR-v2":"R7-FULL12-NATIVE-COVERAGE-ROWMAJOR-v3");f.text("circuit",p.context.plan.circuit_id);f.text("layout",p.context.layout_sha256);for(auto&r:p.required){f.text("id",r.id);f.text("family",r.family);f.text("source",r.source);f.integer("domain",r.domain);f.integer("rounds",r.rounds);}auto d=f.digest();return crypto::hex(d);}
void writeCoverage(std::ostream&o,const Coverage&p){requireCoverage(p);o<<"{\"schema\":"<<quoted(p.context.plan.shape.layer_count==1?"R7-ONEBLOCK-NATIVE-COVERAGE-ROWMAJOR-v2":"R7-FULL12-NATIVE-COVERAGE-ROWMAJOR-v3")<<",\"coverage_sha256\":"<<quoted(identity(p))<<",\"circuit\":"<<quoted(p.context.plan.circuit_id)<<",\"native_plan_sha256\":"<<quoted(r5b::digestHex(Bytes(p.native_json.begin(),p.native_json.end())))<<",\"object_count\":"<<p.objects.size()<<",\"auxiliary_and_source_count\":"<<p.objects.size()-1<<",\"required_count\":"<<p.required.size()<<",\"FC_layers\":"<<p.fc_layers<<",\"native_master_terminals\":"<<p.native_terminals<<",\"object_padded_coordinates\":"<<p.object_padded_coordinates<<",\"direct_full_registration_response_bytes\":"<<p.full_representation_response_bytes<<",\"status\":\"DISCOVERED_NOT_CRYPTOGRAPHICALLY_VERIFIED\",\"required\":[";bool first=true;for(auto&r:p.required){if(!first)o<<',';first=false;o<<"{\"id\":"<<quoted(r.id)<<",\"family\":"<<quoted(r.family)<<",\"source\":"<<quoted(r.source)<<",\"domain\":"<<r.domain<<",\"sumcheck_rounds\":"<<r.rounds<<",\"status\":\"UNVERIFIED\"}";}o<<"],\"objects\":[";first=true;for(auto&v:p.objects){if(!first)o<<',';first=false;o<<"{\"id\":"<<quoted(v.id)<<",\"parent\":"<<quoted(v.parent)<<",\"kind\":"<<unsigned(v.kind)<<",\"actual\":"<<v.actual<<",\"padded\":"<<v.padded<<"}";}o<<"]}\n";}
}
