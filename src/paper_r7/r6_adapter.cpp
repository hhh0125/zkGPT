#include "r6_adapter.hpp"
#include "paper_r2/r6_model.hpp"
#include "paper_r2/r6_trust.hpp"
#include "paper_r1/wire.hpp"
#include "paper_r1/exp.hpp"
#include <set>
#include <iostream>
#include <cstdlib>
namespace zkgpt::r7 {
namespace {
struct Quiet {std::ostream&stream;std::streambuf*saved;struct Sink:std::streambuf{int overflow(int c) override{return c;}}sink;explicit Quiet(std::ostream&s):stream(s),saved(s.rdbuf(&sink)){}~Quiet(){stream.rdbuf(saved);}};
void dimensions(r1::Fs&t,const r1::PCSpec&s){t.text("object/role",s.role);t.integer("object/actual",s.actual);t.integer("object/padded",s.padded);t.integer("object/rows",s.rows);t.integer("object/columns",s.columns);t.integer("object/layout",static_cast<unsigned>(s.layout));}
void add(std::vector<WitnessObject>&out,const r1::PCSpec&s,ObjectKind kind,const std::string&parent){out.push_back({"r7/"+s.role,parent,kind,s.actual,s.padded,{},false});}
void memoryObjects(std::vector<WitnessObject>&out,const r1::MemorySpec&m,const std::string&parent){add(out,r1::readSpec(m),ObjectKind::MemoryRead,parent);add(out,r1::finalSpec(m),ObjectKind::MemoryFinal,parent);}
void rangeObjects(std::vector<WitnessObject>&out,const r1::RangeSpec&r,const std::string&parent){for(unsigned h=0;h<(r.bits+8)/9;++h){auto d=r1::digitSpec(r,h);add(out,d,ObjectKind::RangeDigit,parent);memoryObjects(out,r1::digitMemory(r,h),"r7/"+d.role);}}
}
std::string checkManifest(const std::string&file,const std::string&pin){
 auto bytes=r1::readFile(file);r1::require(r5b::digestHex(bytes)==pin,"independent model manifest SHA256 mismatch");
 r6::parseManifest(bytes,1);return pin;
}
void executePrivateWitness(const std::function<void()>&operation){
 try{Quiet out(std::cout);Quiet err(std::cerr);operation();}
 catch(...){throw std::runtime_error("R7 private witness generation/validation failed; details suppressed");}
}
R7Context buildR6Context(const std::string&pin){
 r1::require(pin.size()==64,"independent model manifest SHA256 required");
 R7Context c;c.model_manifest_sha256=pin;
 {Quiet quiet(std::cout);LLM nn(1,12,64,768,3072);r6::configure(nn);auto graph=nn.buildPublicCircuit();c.plan=r2::planModelQueries(graph,nn.getWitnessRegistry(),nn.parameterRegions(),r1::ExpProfile::LegacyClampOne);r6::auditGraph(graph,nn,c.plan);r6::validateRegions(c.plan,nn.parameterRegions());}
 r1::require(c.plan.circuit_id=="1cf3bc83a9a9f289e540238255d467b7fa6e2801209aa00f07fe232c48635a9a","frozen oneblock circuit changed");
 add(c.objects,c.plan.master,ObjectKind::BaseMaster,"");
 r1::Fs layout("zkGPT.R7.R6-layout.split-aux.v1");dimensions(layout,c.plan.master);
 for(std::size_t i=0;i<c.plan.sources.size();++i){const auto&s=c.plan.sources[i];add(c.objects,s.spec,i+1==c.plan.sources.size()?ObjectKind::ModelW:ObjectKind::MappedChild,c.objects.front().id);c.objects.back().spans=s.spans;dimensions(layout,s.spec);layout.integer("spans",s.spans.size());for(auto&span:s.spans){layout.integer("span/offset",span.offset);layout.integer("span/count",span.count);layout.scalar("span/bias",span.bias);}}
 for(auto&r:c.plan.ranges){layout.integer("range/child",r.child);layout.integer("range/bits",r.bits);rangeObjects(c.objects,{r.id,c.plan.sources.at(r.child).spec,r.bits},"r7/"+c.plan.sources.at(r.child).spec.role);}
 for(auto&e:c.plan.exps){
  r1::ExpSpec exp{e.id,c.plan.sources.at(e.index_child).spec,c.plan.sources.at(e.value_child).spec,e.profile};
  layout.integer("exp/profile",static_cast<unsigned>(e.profile));
  for(unsigned k=0;k<4;++k){
   auto h=r1::expHelperSpec(exp,k);add(c.objects,h,ObjectKind::ExpHelper,"r7/"+exp.index.role);
   auto range=r1::expRangeSpec(exp,k);rangeObjects(c.objects,range,"r7/"+range.query.role);
  }
  auto memory=r1::expMemorySpec(exp);memoryObjects(c.objects,memory,"r7/"+memory.address.role);
 }
 for(auto i:c.plan.output)layout.integer("Y/master-index",i);
 for(auto i:c.plan.constants)layout.integer("constant/one",i);
 for(auto i:c.plan.reserved_zero)layout.integer("constant/zero",i);
 std::set<std::string> ids;for(auto&o:c.objects){r1::require(ids.insert(o.id).second,"R7 duplicate witness object identity");layout.text("aux/id",o.id);layout.text("aux/parent",o.parent);layout.integer("aux/kind",static_cast<unsigned>(o.kind));layout.integer("aux/actual",o.actual);layout.integer("aux/padded",o.padded);}
 const auto d=layout.digest();c.layout_sha256=r5b::hex(Bytes(d.begin(),d.end()));return c;
}
R7WitnessStore::R7WitnessStore()=default;
R7WitnessStore::~R7WitnessStore()=default;
void R7WitnessStore::generateR6(const R7Context&c,const std::string&manifest){
 r1::require(!execution_,"R7 witness store already populated");checkManifest(manifest,c.model_manifest_sha256);
 auto m=r6::parseManifest(r1::readFile(manifest),1);r6::checkParameterFiles(m,"data/gpt2_int");
 // Freeze the parameter directory across the existing production readers.
 const char*old=std::getenv("ZKGPT_PARAMETER_DIR");const bool had=old;const std::string saved=old?old:"";
 struct Restore{bool had;std::string saved;~Restore(){if(had)setenv("ZKGPT_PARAMETER_DIR",saved.c_str(),1);else unsetenv("ZKGPT_PARAMETER_DIR");}}restore{had,saved};
 r1::require(setenv("ZKGPT_PARAMETER_DIR","data/gpt2_int",1)==0,"parameter directory setup failed");
 network_=std::make_unique<LLM>(1,12,64,768,3072);execution_=std::make_unique<::prover>();r6::configure(*network_);network_->setWitnessCapacityHint(c.plan.master.actual);
 executePrivateWitness([&]{network_->createFiniteWitness(*execution_);network_->validateCurrentWitness(*execution_);});
 r1::require(legacyCommitmentAbsent(),"R7 must not call legacy nonhiding PC");
 auto p=r2::planModelQueries(execution_->C,network_->getWitnessRegistry(),network_->parameterRegions(),r1::ExpProfile::LegacyClampOne);
 r1::require(p.circuit_id==c.plan.circuit_id && p.master==c.plan.master && p.output==c.plan.output,"R7 witness differs from verifier-owned R6 plan");
 r6::validateRegions(p,network_->parameterRegions());r1::require(baseMaster().size()==p.master.actual,"R7 actual base Master size");
 r6::checkParameterFiles(m,"data/gpt2_int");objects_=c.objects;objects_.front().materialized=true;
}
const std::vector<Fr>& R7WitnessStore::baseMaster()const{r1::require(execution_ && !execution_->val.empty(),"R7 no witness");return execution_->val[0];}
std::vector<Fr> R7WitnessStore::publicX()const{const auto&w=baseMaster();r1::require(w.size()>=32768,"R7 X bounds");return {w.begin(),w.begin()+32768};}
std::vector<Fr> R7WitnessStore::publicY(const R7Context&c)const{const auto&w=baseMaster();std::vector<Fr> y;y.reserve(c.plan.output.size());for(auto i:c.plan.output)y.push_back(w.at(i));return y;}
bool R7WitnessStore::legacyCommitmentAbsent()const{return execution_ && execution_->cc.w==nullptr && execution_->cc.comm==nullptr && execution_->gens.empty();}
R7WitnessStore::FCContraction R7WitnessStore::firstFCContraction(const R7Context&context)const{
 FCContraction out;out.layer=4;const auto&w=baseMaster();
 r1::require(execution_ && legacyCommitmentAbsent() && context.plan.circuit_id=="1cf3bc83a9a9f289e540238255d467b7fa6e2801209aa00f07fe232c48635a9a","R7 actual frozen oneblock required");
 const auto&l=execution_->C.circuit.at(out.layer);
 r1::require(l.ty==layerType::FCONN && l.ori_id_u.size()==32768 && l.ori_id_v.size()==4194304,"R7 first FC actual padded mapping");
 for(std::size_t k=0;k<1024;++k){out.activation.push_back(w.at(l.ori_id_u[k]));out.weight.push_back(w.at(l.ori_id_v[k]));}
 out.output=execution_->val.at(out.layer).at(0);
 r1::require(r1::dot(out.activation,out.weight)==out.output,"R7 selected actual FC contraction mismatch");return out;
}
}
