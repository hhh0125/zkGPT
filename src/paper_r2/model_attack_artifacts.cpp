#include "wire.hpp"
#include <filesystem>
#include <iostream>
#include <fstream>
namespace r1=zkgpt::r1;
using namespace zkgpt::r2;
int main(int argc,char **argv){try{
 r1::init();r1::require(argc==3,"usage: r2_model_attack_artifacts VALID_DIRECTORY NEW_ATTACK_DIRECTORY");
 const std::filesystem::path in=argv[1],out=argv[2];r1::require(!std::filesystem::exists(out),"attack output exists");std::filesystem::create_directories(out);
 auto [g,s]=parseStatement(r1::readFile((in/"model.statement").string()));auto p=parseProof(g,s,r1::readFile((in/"model.proof").string()));
 const auto resources=r1::planLookupBatch(s);std::ofstream coverage(out/"coverage.json");
 coverage<<"{\"ranges\":[";for(std::size_t j=0;j<s.ranges.size();++j){const auto q=r1::rangeSpec(s,j);if(j)coverage<<',';coverage<<"{\"bits\":"<<q.bits<<",\"actual\":"<<q.query.actual<<",\"padded\":"<<q.query.padded<<",\"source_spans\":"<<s.input.children[s.ranges[j].child].spans.size()<<"}";}
 coverage<<"],\"Exp_profile\":"<<unsigned(s.exps[0].profile)<<",\"Exp_actual\":"<<r1::expSpec(s,0).index.actual<<",\"Exp_padded\":"<<r1::expSpec(s,0).index.padded<<",\"W_actual\":"<<s.input.children.back().commitment.spec.actual<<",\"W_padded\":"<<s.input.children.back().commitment.spec.padded<<",\"public_constraints\":"<<s.input.public_values.size()<<",\"estimated_lookup_prover_bytes_excluding_circuit_GKR\":"<<resources.estimated_prover_bytes<<",\"max_generators\":"<<resources.max_generators<<",\"max_rounds\":"<<resources.max_sumcheck_rounds<<"}";
 auto save=[&](const std::string &n,const r1::LookupBatchSpec &ss,const Proof &pp){r1::writeFile((out/(n+".statement")).string(),serializeStatement(g,ss));r1::writeFile((out/(n+".proof")).string(),serializeProof(g,ss,pp));};
 auto bad=s;bad.input.public_values[32768].value+=Fr(1);save("public-Y",bad,p);
 bad=s;bad.input.public_values[32768+768].value=Fr(1);save("public-padding",bad,p);
 bad=s;bad.input.children[0].spans[0].offset+=1;save("derived-map",bad,p);
 bad=s;bad.input.master.rows[0]+=r1::generators(bad.input.master.spec).u;save("parent-commitment",bad,p);
 bad=s;bad.input.children.back().commitment.rows[0]+=r1::generators(bad.input.children.back().commitment.spec).u;save("model-W",bad,p);
 bad=s;auto k=bad.exps[0].index_child;bad.input.children[k].commitment.rows[0]+=r1::generators(bad.input.children[k].commitment.spec).u;save("Exp-index-commitment",bad,p);
 bad=s;k=bad.exps[0].value_child;bad.input.children[k].commitment.rows[0]+=r1::generators(bad.input.children[k].commitment.spec).u;save("Exp-value-commitment",bad,p);
 bad=s;bad.exps[0].profile=r1::ExpProfile::Unclamped;save("Exp-profile",bad,p);
 bad=s;bad.ranges.pop_back();auto incomplete=p;incomplete.lookup.ranges.pop_back();save("missing-range",bad,incomplete);
 auto wrong=p;wrong.lookup.exps[0].helper[0].rows[0]+=r1::generators(wrong.lookup.exps[0].helper[0].spec).u;save("Exp-address",s,wrong);
 wrong=p;wrong.lookup.exps[0].memory.roots[0]+=Fr(1);save("Exp-memory",s,wrong);
 std::cout<<"PASS saved 11 real-proof attack artifacts; each requires the full model verifier\n";return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
