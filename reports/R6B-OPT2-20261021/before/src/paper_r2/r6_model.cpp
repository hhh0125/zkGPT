#include "r6_model.hpp"
#include "r5b_trust.hpp"
#include <set>
#include <map>
namespace zkgpt::r6 {
void auditGraph(const layeredCircuit &c,const LLM &nn,const r2::ModelQueryPlan &p){
 const auto blocks=p.shape.layer_count;const auto &edges=nn.blockConnections();
 r1::require(p.r6_engineering && p.r5b_candidate && c.size==1+23*blocks && edges.size()==blocks,"R6 graph/shape coverage");
 r1::require(nn.metadataWitnessElements()==0,"R6 metadata must not allocate field witness");
 std::map<std::string,std::size_t> offsets;for(auto &r:nn.getWitnessRegistry().constraints())offsets.emplace(r.name,r.val0_offset);
 const auto &regions=nn.parameterRegions();r1::require(regions.size()==12*blocks,"R6 parameter region coverage");
 std::size_t fcs=0;
 for(std::size_t b=0;b<blocks;++b){const auto &e=edges[b];
  r1::require(e.block==b && e.first_layer==1+23*b && e.last_layer==23+23*b,"R6 block layer order");
  r1::require(e.input==(b?edges[b-1].output:0),"R6 disconnected block input");
  const auto &ln=c.circuit[e.first_layer];std::set<u32> ln_inputs(ln.ori_id_u.begin(),ln.ori_id_u.end());
  for(std::size_t j=0;j<32768;++j)r1::require(ln_inputs.count(e.input+j)==1,"R6 LN misses previous block output/padding");
  for(unsigned which=0;which<2;++which){const auto &l=c.circuit[e.first_layer+(which?22:11)];
   auto output=which?e.output:e.attention_residual,skip=which?e.attention_residual:e.input;
   auto q=offsets.at("round_"+std::to_string(4*b+(which?3:1))+"_q");
   r1::require(l.ty==layerType::RELU && l.size==32768 && l.uni_gates.size()==98304 && l.bin_gates.empty(),"R6 residual graph shape");
   for(std::size_t j=0;j<32768;++j)for(unsigned k=0;k<3;++k){const auto &g=l.uni_gates[3*j+k];auto expected=(k==0?output:k==1?q:skip)+j;
    r1::require(g.g==j && g.lu==0 && g.sc==(k?-1:1) && l.ori_id_u.at(g.u)==expected,"R6 residual branch binding");}
  }
  for(unsigned part=0;part<2;++part){auto id=2*b+part;const auto&weight=regions[8*blocks+2*id],&bias=regions[8*blocks+2*id+1];
   r1::require(weight.name=="ln_"+std::to_string(id)+".weight" && bias.name=="ln_"+std::to_string(id)+".bias","R6 LN global parameter identity");
   const auto&l=c.circuit[e.first_layer+(part?14:2)];r1::require(l.ty==layerType::LAYER_NORM_3 && l.bin_gates.size()==3*32*768,"R6 LN output constraint shape");
   auto centered=offsets.at("layer_norm_"+std::to_string(id)+"_centered"),sigma=offsets.at("layer_norm_"+std::to_string(id)+"_sigma");
   for(std::size_t row=0;row<32;++row)for(std::size_t col=0;col<768;++col){const auto&g=l.bin_gates[3*(row*768+col)],&gb=l.bin_gates[3*(row*768+col)+1];
    r1::require(g.l==0 && g.sc==120000000 && l.ori_id_u.at(g.u)==weight.offset+col && l.ori_id_v.at(g.v)==centered+row*1024+col,"R6 LN gamma/centered master binding");
    r1::require(gb.l==0 && gb.sc==2 && l.ori_id_u.at(gb.u)==bias.offset+col && l.ori_id_v.at(gb.v)==sigma+row,"R6 LN beta/root master binding");}
  }
  const unsigned fc_layer[4]={3,9,15,20};
  for(unsigned k=0;k<4;++k){auto id=4*b+k;const auto &l=c.circuit[e.first_layer+fc_layer[k]];
   auto input=k==0?offsets.at("layer_norm_"+std::to_string(2*b)+"_y"):k==1?offsets.at("softmax_"+std::to_string(b)+"_output"):k==2?offsets.at("layer_norm_"+std::to_string(2*b+1)+"_y"):offsets.at("gelu_"+std::to_string(b)+"_y");
   const auto &w=regions[2*id],&bias=regions[2*id+1];
   r1::require(l.ty==layerType::FCONN && w.name=="fc_"+std::to_string(id)+".weight" && bias.name=="fc_"+std::to_string(id)+".bias","R6 FC/global parameter order");
   for(std::size_t j=0;j<l.ori_id_u.size();++j)r1::require(l.ori_id_u[j]==input+j,"R6 FC activation mapping");
   r1::require(l.ori_id_v.size()==w.count,"R6 FC W size");for(std::size_t j=0;j<w.count;++j)r1::require(l.ori_id_v[j]==w.offset+j,"R6 FC W coordinates");++fcs;
  }
 }
 r1::require(fcs==4*blocks && nn.getWitnessRegistry().expQueries().size()==6336*blocks,"R6 FC/Exp coverage");
 for(std::size_t i=0;i<nn.getWitnessRegistry().expQueries().size();++i){const auto &q=nn.getWitnessRegistry().expQueries()[i];r1::require(q.operator_id==i/6336 && q.query_id==i%6336,"R6 Exp block/order coverage");}
 r1::require(p.output.front()==edges.back().output && p.output.back()==edges.back().output+32767,"R6 final hidden state output");
 r1::require(p.constants.size()==8*blocks,"R6 numerical constant coverage");
}
void writePlan(std::ostream &o,const layeredCircuit&c,const LLM&nn,const r2::ModelQueryPlan&p){
 auditGraph(c,nn,p);auto res=r2::modelResources(c,p);
 std::size_t layer_fields=0,gate_capacity=0,coordinate_capacity=0,max_layer=0;
 for(std::size_t i=0;i<c.circuit.size();++i){const auto &l=c.circuit[i];if(i)layer_fields+=(std::size_t(1)<<l.bit_length);max_layer=std::max(max_layer,std::size_t(1)<<l.bit_length);gate_capacity+=l.uni_gates.capacity()*sizeof(uniGate)+l.bin_gates.capacity()*sizeof(binGate);coordinate_capacity+=(l.ori_id_u.capacity()+l.ori_id_v.capacity())*sizeof(u32);}
 o<<"{\"blocks\":"<<p.shape.layer_count<<",\"profile\":\""<<r5b::kProfile<<"\",\"profile_sha256\":\""<<r5b::profileHash()<<"\",\"circuit\":\""<<p.circuit_id<<"\",\"layers\":"<<c.size<<",\"metadata_field_witness_elements\":"<<nn.metadataWitnessElements()<<",\"master_actual\":"<<p.master.actual<<",\"master_padded\":"<<p.master.padded<<",\"W_actual\":"<<p.sources.back().spec.actual<<",\"W_padded\":"<<p.sources.back().spec.padded<<",\"child_padded_elements\":"<<res.child_elements<<",\"range_values\":"<<res.range_values<<",\"Exp_actual\":"<<res.exp_values<<",\"terminals\":"<<res.terminals<<",\"gate_and_map_bytes\":"<<res.gate_bytes<<",\"gate_capacity_bytes\":"<<gate_capacity<<",\"coordinate_capacity_bytes\":"<<coordinate_capacity<<",\"nonmaster_layer_padded_fields\":"<<layer_fields<<",\"max_layer_padded_fields\":"<<max_layer<<",\"lookup_prover_bytes\":"<<res.lookup_prover_bytes<<",\"lookup_retained_query_bytes\":"<<res.lookup_retained_bytes<<",\"lookup_local_workspace_bytes\":"<<res.lookup_local_bytes<<",\"lookup_mapped_workspace_bytes\":"<<res.lookup_mapped_bytes<<",\"gkr_checked_input_max_padded\":"<<res.gkr_checked_input_padded<<",\"shared_legacy_resource_envelope_prover\":"<<res.prover_peak_bytes<<",\"shared_legacy_resource_envelope_verifier\":"<<res.verifier_peak_bytes<<",\"proof_upper_bytes\":"<<res.proof_upper_bytes<<",\"full12_proof_executed\":false,\"connections\":[";
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
