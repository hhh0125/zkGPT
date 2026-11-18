#include "paper_r2_r6_model.hpp"
#include "paper_r2_r5b_trust.hpp"
namespace zkgpt::r6 {
PublicCircuit::PublicCircuit(unsigned blocks):network_(blocks,12,64,768,3072),
 graph_([&]{configure(network_);return network_.buildPublicCircuit();}()),
 plan_(r2::finalizeModelQueries(graph_,network_.getWitnessRegistry(),r1::ExpProfile::LegacyClampOne)){
}
namespace {
void serializePlan(std::ostream &o,const layeredCircuit&c,const LLM&nn,const r2::ModelQueryPlan&p){
 auto res=r2::modelResources(c,p);
 std::size_t layer_fields=0,gate_capacity=0,coordinate_capacity=0,max_layer=0;
 for(std::size_t i=0;i<c.circuit.size();++i){const auto &l=c.circuit[i];if(i)layer_fields+=(std::size_t(1)<<l.bit_length);max_layer=std::max(max_layer,std::size_t(1)<<l.bit_length);gate_capacity+=l.uni_gates.capacity()*sizeof(uniGate)+l.bin_gates.capacity()*sizeof(binGate);coordinate_capacity+=(l.ori_id_u.capacity()+l.ori_id_v.capacity())*sizeof(u32);}
 o<<"{\"blocks\":"<<p.shape.layer_count<<",\"profile\":\""<<r5b::kProfile<<"\",\"profile_sha256\":\""<<r5b::profileHash()<<"\",\"circuit\":\""<<p.circuit_id<<"\",\"layers\":"<<c.size<<",\"metadata_field_witness_elements\":"<<nn.metadataWitnessElements()<<",\"master_actual\":"<<p.master.actual<<",\"master_padded\":"<<p.master.padded<<",\"W_actual\":"<<p.sources.back().spec.actual<<",\"W_padded\":"<<p.sources.back().spec.padded<<",\"child_padded_elements\":"<<res.child_elements<<",\"range_values\":"<<res.range_values<<",\"Exp_actual\":"<<res.exp_values<<",\"terminals\":"<<res.terminals<<",\"gate_and_map_bytes\":"<<res.gate_bytes<<",\"gate_capacity_bytes\":"<<gate_capacity<<",\"coordinate_capacity_bytes\":"<<coordinate_capacity<<",\"nonmaster_layer_padded_fields\":"<<layer_fields<<",\"max_layer_padded_fields\":"<<max_layer<<",\"lookup_prover_bytes\":"<<res.lookup_prover_bytes<<",\"lookup_retained_query_bytes\":"<<res.lookup_retained_bytes<<",\"lookup_local_workspace_bytes\":"<<res.lookup_local_bytes<<",\"lookup_mapped_workspace_bytes\":"<<res.lookup_mapped_bytes<<",\"gkr_checked_input_max_padded\":"<<res.gkr_checked_input_padded<<",\"lookup_immutable_master_bytes\":"<<p.master.padded*sizeof(Fr)<<",\"lookup_mapped_incremental_bytes\":"<<res.lookup_mapped_bytes-p.master.padded*sizeof(Fr)<<",\"lookup_incremental_prover_bytes\":"<<res.lookup_retained_bytes+std::max(res.lookup_local_bytes,res.lookup_mapped_bytes-p.master.padded*sizeof(Fr))<<",\"planned_val0_capacity_bytes\":"<<p.master.actual*sizeof(Fr)<<",\"shared_cc_caller_master_bytes\":"<<p.master.padded*sizeof(Fr)<<",\"graph_additional_safety_reserve_bytes\":"<<2*(gate_capacity+coordinate_capacity)<<",\"shared_legacy_resource_envelope_prover\":"<<res.prover_peak_bytes<<",\"shared_legacy_resource_envelope_verifier\":"<<res.verifier_peak_bytes<<",\"proof_upper_bytes\":"<<res.proof_upper_bytes<<",\"full12_proof_executed\":false,\"connections\":[";
 bool first=true;for(auto&e:nn.blockConnections()){if(!first)o<<',';first=false;o<<"{\"block\":"<<e.block<<",\"input\":"<<e.input<<",\"attention_residual\":"<<e.attention_residual<<",\"output\":"<<e.output<<",\"first_layer\":"<<e.first_layer<<",\"last_layer\":"<<e.last_layer<<"}";}
 o<<"],\"parameters\":[";first=true;for(auto&r:nn.parameterRegions()){if(!first)o<<',';first=false;o<<"{\"name\":\""<<r.name<<"\",\"offset\":"<<r.offset<<",\"count\":"<<r.count<<",\"rows\":"<<r.rows<<",\"cols\":"<<r.cols<<"}";}
 o<<"],\"ranges\":[";first=true;for(auto&r:nn.getWitnessRegistry().constraints()){if(!first)o<<',';first=false;o<<"{\"name\":\""<<r.name<<"\",\"offset\":"<<r.val0_offset<<",\"count\":"<<r.count<<",\"bits\":"<<r.bits<<",\"signed\":"<<(r.is_signed?"true":"false")<<"}";}
 o<<"],\"children\":[";first=true;for(auto&q:p.sources){if(!first)o<<',';first=false;o<<"{\"role\":\""<<q.spec.role<<"\",\"actual\":"<<q.spec.actual<<",\"padded\":"<<q.spec.padded<<",\"rows\":"<<q.spec.rows<<",\"columns\":"<<q.spec.columns<<",\"spans\":[";bool sf=true;for(auto&s:q.spans){if(!sf)o<<',';sf=false;o<<"{\"offset\":"<<s.offset<<",\"count\":"<<s.count<<",\"bias\":\""<<r5b::hex(r1::encode(s.bias))<<"\"}";}o<<"]}";}
 o<<"],\"terminal_plans\":[";first=true;
 auto coordinateDigest=[](const std::vector<u32>&ids){r1::Bytes bytes;bytes.reserve(ids.size()*4);for(auto i:ids)for(unsigned j=0;j<4;++j)bytes.push_back(std::uint8_t(i>>(8*j)));return r5b::digestHex(bytes);};
 for(std::size_t i=1;i<c.circuit.size();++i){const auto&l=c.circuit[i];auto u=coordinateDigest(l.ori_id_u),v=coordinateDigest(l.ori_id_v);
  auto terminal=[&](const char*role,bool isv){const auto&ids=isv?l.ori_id_v:l.ori_id_u;if(!first)o<<',';first=false;o<<"{\"layer\":"<<i<<",\"role\":\""<<role<<"\",\"master_coordinate_count\":"<<ids.size()<<",\"coordinate_u32le_sha256\":\""<<(isv?v:u)<<"\",\"first\":"<<(ids.empty()?0:ids.front())<<",\"last\":"<<(ids.empty()?0:ids.back())<<"}";};
  if(l.bit_length_u[0]>=0)terminal("input-u0",false);if(l.bit_length_v[0]>=0)terminal("input-v0",true);if(l.ty==layerType::FCONN){terminal("matrix-u",false);terminal("matrix-v",true);}
 }
 o<<"],\"gkr_layers\":[";first=true;for(std::size_t i=0;i<c.circuit.size();++i){const auto&l=c.circuit[i];if(!first)o<<',';first=false;o<<"{\"layer\":"<<i<<",\"type\":"<<unsigned(l.ty)<<",\"actual\":"<<l.size<<",\"padded\":"<<(std::size_t(1)<<l.bit_length)<<",\"unary_gates\":"<<l.uni_gates.size()<<",\"binary_gates\":"<<l.bin_gates.size()<<",\"u_master\":"<<l.size_u[0]<<",\"v_master\":"<<l.size_v[0]<<",\"phase2\":"<<(l.need_phase2?"true":"false")<<"}";}o<<"]}\n";r1::require(bool(o),"R6 plan output failed");
}
}
void writePlan(std::ostream&o,const layeredCircuit&c,const LLM&nn,const r2::ModelQueryPlan&p){
 serializePlan(o,c,nn,p);
}
void writePlan(std::ostream&o,const PublicCircuit&a){serializePlan(o,a.graph(),a.network(),a.plan());}
}
