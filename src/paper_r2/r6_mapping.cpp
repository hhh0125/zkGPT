#include "r6_trust.hpp"
#include "models.hpp"
#include <map>
#include <cstdlib>
namespace zkgpt::r6 {
namespace {
class ParameterBlock:public LLM {
 public:ParameterBlock():LLM(1,12,64,768,3072){}
 void read(prover &pr,unsigned block){initParam(pr,1);pr.val.resize(1);val=pr.val.begin();pr.val[0].resize(total_in_size);parameter_regions_.clear();
  for(unsigned k=0;k<4;++k){refreshFCParam(full_conn[k]);readFconWeight(full_conn[k].weight_start_id,pr.fc_real_row[k],pr.fc_real_col[k],4*block+k);readFconBias(full_conn[k].bias_start_id,pr.fc_real_col[k],4*block+k);}
  refreshFCParam(full_conn[0]);read_layer_norm(2*block);refreshFCParam(full_conn[2]);read_layer_norm(2*block+1);
 }
};
}
std::size_t compareProductionMapping(const r2::ModelQueryPlan&p,const std::vector<neuralNetwork::ParameterRegion>&rs,const std::vector<Fr>&w,const std::string&dir){
 validateRegions(p,rs);r1::require(w.size()==p.sources.back().spec.padded,"R6 mapping W size");
 const char*old=std::getenv("ZKGPT_PARAMETER_DIR");std::string saved=old?old:"";bool had=old;
 struct Restore{bool had;std::string saved;~Restore(){if(had)setenv("ZKGPT_PARAMETER_DIR",saved.c_str(),1);else unsetenv("ZKGPT_PARAMETER_DIR");}}restore{had,saved};setenv("ZKGPT_PARAMETER_DIR",dir.c_str(),1);
 std::map<std::string,std::size_t> at;std::size_t total=0,checked=0;for(auto&r:rs){at.emplace(r.name,total);total+=r.count;}
 for(unsigned b=0;b<p.shape.layer_count;++b){ParameterBlock n;prover pr{};n.read(pr,b);for(auto&r:n.parameterRegions()){auto target=at.at(r.name);for(std::size_t j=0;j<r.count;++j){r1::require(w.at(target+j)==pr.val[0].at(r.offset+j),"R6 independent production reader W mismatch: "+r.name);++checked;}}}
 r1::require(checked==total,"R6 production mapping coverage");for(std::size_t j=total;j<w.size();++j)r1::require(w[j].isZero(),"R6 W padded tail nonzero");return checked;
}
}
